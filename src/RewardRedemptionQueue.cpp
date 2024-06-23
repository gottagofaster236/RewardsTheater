// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "RewardRedemptionQueue.h"

#include <algorithm>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <cstring>
#include <utility>

#include "Log.h"

namespace asio = boost::asio;
using namespace std::chrono_literals;

RewardRedemptionQueue::RewardRedemptionQueue(Settings& settings, TwitchRewardsApi& twitchRewardsApi)
    : settings(settings), twitchRewardsApi(twitchRewardsApi), rewardRedemptionQueueThread(1),
      ioContext(rewardRedemptionQueueThread.ioContext), rewardPlaybackPaused(false),
      rewardRedemptionQueueCondVar(ioContext, boost::posix_time::pos_infin), playObsSourceState(0),
      randomEngine(std::random_device()()) {
    asio::co_spawn(ioContext, asyncPlayRewardRedemptionsFromQueue(), asio::detached);
}

RewardRedemptionQueue::~RewardRedemptionQueue() {
    rewardRedemptionQueueThread.stop();
}

std::vector<RewardRedemption> RewardRedemptionQueue::getRewardRedemptionQueue() const {
    std::lock_guard<std::mutex> guard(rewardRedemptionQueueMutex);
    return rewardRedemptionQueue;
}

void RewardRedemptionQueue::queueRewardRedemption(const RewardRedemption& rewardRedemption) {
    std::optional<std::string> obsSourceName = settings.getObsSourceName(rewardRedemption.reward.id);
    if (!obsSourceName.has_value()) {
        return;
    }
    if (isRewardPlaybackPaused()) {
        twitchRewardsApi.updateRedemptionStatus(rewardRedemption, TwitchRewardsApi::RedemptionStatus::CANCELED);
        return;
    }
    if (!settings.isRewardRedemptionQueueEnabled()) {
        playObsSource(
            rewardRedemption.reward.id,
            obsSourceName.value(),
            settings.isRandomPositionEnabled(rewardRedemption.reward.id)
        );
        return;
    }

    {
        std::lock_guard<std::mutex> guard(rewardRedemptionQueueMutex);
        rewardRedemptionQueue.push_back(rewardRedemption);
        emit onRewardRedemptionQueueUpdated(rewardRedemptionQueue);
    }
    notifyRewardRedemptionQueueCondVar();
}

void RewardRedemptionQueue::removeRewardRedemption(const RewardRedemption& rewardRedemption) {
    bool shouldStopSource;
    {
        std::lock_guard<std::mutex> guard(rewardRedemptionQueueMutex);
        auto position = std::find(rewardRedemptionQueue.begin(), rewardRedemptionQueue.end(), rewardRedemption);
        if (position == rewardRedemptionQueue.end()) {
            return;
        }
        shouldStopSource = (position == rewardRedemptionQueue.begin());
        rewardRedemptionQueue.erase(position);
        emit onRewardRedemptionQueueUpdated(rewardRedemptionQueue);
    }

    if (shouldStopSource) {
        obs_source_media_stop(getObsSource(rewardRedemption));
    }
    twitchRewardsApi.updateRedemptionStatus(rewardRedemption, TwitchRewardsApi::RedemptionStatus::CANCELED);
}

std::vector<std::string> RewardRedemptionQueue::enumObsSources() {
    std::vector<std::string> sources;

    struct AddToSourcesCallback {
        static bool addToSources(void* param, obs_source_t* source) {
            if (isMediaSource(source)) {
                auto& sources = *static_cast<std::vector<std::string>*>(param);
                sources.emplace_back(obs_source_get_name(source));
            }
            return true;
        }
    };

    obs_enum_sources(&AddToSourcesCallback::addToSources, &sources);
    return sources;
}

bool RewardRedemptionQueue::isRewardPlaybackPaused() const {
    std::lock_guard guard(rewardRedemptionQueueMutex);
    return rewardPlaybackPaused;
}

void RewardRedemptionQueue::setRewardPlaybackPaused(bool paused) {
    {
        std::lock_guard guard(rewardRedemptionQueueMutex);
        rewardPlaybackPaused = paused;
    }
    notifyRewardRedemptionQueueCondVar();
}

RewardRedemptionQueue::ObsSourceNotFoundException::ObsSourceNotFoundException(const std::string& obsSourceName)
    : obsSourceName(obsSourceName) {}

const char* RewardRedemptionQueue::ObsSourceNotFoundException::what() const noexcept {
    return "ObsSourceNotFoundException";
}

RewardRedemptionQueue::ObsSourceNoVideoException::ObsSourceNoVideoException(const std::string& obsSourceName)
    : obsSourceName(obsSourceName) {}

const char* RewardRedemptionQueue::ObsSourceNoVideoException::what() const noexcept {
    return "ObsSourceNoVideoException";
}

void RewardRedemptionQueue::testObsSource(
    const std::string& rewardId,
    const std::string& obsSourceName,
    bool randomPositionEnabled,
    QObject* receiver,
    const char* member
) {
    asio::co_spawn(
        ioContext,
        asyncTestObsSource(
            rewardId, obsSourceName, randomPositionEnabled, *(new QObjectCallback(this, receiver, member))
        ),
        asio::detached
    );
}

asio::awaitable<void> RewardRedemptionQueue::asyncPlayRewardRedemptionsFromQueue() {
    while (true) {
        RewardRedemption nextRewardRedemption = co_await asyncGetNextRewardRedemption();
        try {
            const std::string& rewardId = nextRewardRedemption.reward.id;
            co_await asyncPlayObsSource(
                rewardId, getObsSource(nextRewardRedemption), settings.isRandomPositionEnabled(rewardId)
            );
        } catch (const ObsSourceNoVideoException&) {}
        co_await popPlayedRewardRedemptionFromQueue(nextRewardRedemption);

        auto timeBeforeNextReward =
            std::chrono::milliseconds(static_cast<long long>(1000 * settings.getIntervalBetweenRewardsSeconds()));
        co_await asio::steady_timer(ioContext, timeBeforeNextReward).async_wait(asio::use_awaitable);
    }
}

asio::awaitable<RewardRedemption> RewardRedemptionQueue::asyncGetNextRewardRedemption() {
    while (true) {
        {
            std::lock_guard guard(rewardRedemptionQueueMutex);
            if (!rewardPlaybackPaused && !rewardRedemptionQueue.empty()) {
                co_return rewardRedemptionQueue.front();
            }
        }
        try {
            co_await rewardRedemptionQueueCondVar.async_wait(asio::use_awaitable);
        } catch (const boost::system::system_error&) {
            // Condition variable notified.
        }
    }
}

void RewardRedemptionQueue::notifyRewardRedemptionQueueCondVar() {
    ioContext.post([this]() {
        rewardRedemptionQueueCondVar.cancel();  // Equivalent to notify_all() for a condition variable
    });
}

asio::awaitable<void> RewardRedemptionQueue::popPlayedRewardRedemptionFromQueue(const RewardRedemption& rewardRedemption
) {
    {
        std::lock_guard guard(rewardRedemptionQueueMutex);
        if (rewardRedemptionQueue.empty() || rewardRedemptionQueue.front() != rewardRedemption) {
            // The reward was removed and canceled by the user.
            // Wait for a bit so that the cancellation doesn't affect the next reward.
            co_await asio::steady_timer(ioContext, 500ms).async_wait(asio::use_awaitable);
            co_return;
        }
        rewardRedemptionQueue.erase(rewardRedemptionQueue.begin());
    }
    twitchRewardsApi.updateRedemptionStatus(rewardRedemption, TwitchRewardsApi::RedemptionStatus::FULFILLED);
    emit onRewardRedemptionQueueUpdated(rewardRedemptionQueue);
}

void RewardRedemptionQueue::playObsSource(
    const std::string& rewardId,
    const std::string& obsSourceName,
    bool randomPositionEnabled
) {
    playObsSource(rewardId, getObsSource(obsSourceName), randomPositionEnabled);
}

void RewardRedemptionQueue::playObsSource(
    const std::string& rewardId,
    OBSSourceAutoRelease source,
    bool randomPositionEnabled
) {
    asio::co_spawn(ioContext, asyncPlayObsSource(rewardId, std::move(source), randomPositionEnabled), asio::detached);
}

template <class T>
class ObsSignalWithCallback {
public:
    ObsSignalWithCallback(obs_source_t* source, const char* signal, signal_callback_t callback, T& param)
        : signal(obs_source_get_signal_handler(source), signal, callback, &param), param(param) {}

    ~ObsSignalWithCallback() {
        param->enabled = false;
    }

private:
    OBSSignal signal;
    T& param;
};

/// First the function sets the deadline timer for the source to start and waits for it.
/// If the source does start, then the deadline timer is updated to 1.5 times the source duration.
/// The deadline timer is cancelled when the source stops, therefore control returns to the function again.
asio::awaitable<void> RewardRedemptionQueue::asyncPlayObsSource(
    std::string rewardId,
    OBSSourceAutoRelease source,
    bool randomPositionEnabled
) {
    if (!source) {
        co_return;
    }
    unsigned state = playObsSourceState++;
    sourcePlayedByState[source] = state;

    asio::deadline_timer deadlineTimer(ioContext);
    auto mediaStartedCallback = std::make_shared<MediaStartedCallback>(ioContext);
    auto mediaEndedCallback = std::make_shared<MediaEndedCallback>(ioContext, deadlineTimer);
    ObsSignalWithCallback mediaStartedSignal(
        source, "media_started", &MediaStartedCallback::setMediaStarted, mediaStartedCallback
    );

    SourcePlayback sourcePlayback{state, rewardId, source, randomPositionEnabled, 0, 1};
    startObsSource(sourcePlayback);

    // Source may be stopped while we are starting it - therefore, register the signals later.
    ObsSignalWithCallback mediaEndedSignal(
        source, "media_ended", &MediaEndedCallback::stopDeadlineTimer, mediaEndedCallback
    );
    ObsSignalWithCallback mediaStoppedSignal(
        source, "media_stopped", &MediaEndedCallback::stopDeadlineTimer, mediaEndedCallback
    );

    // Give some time for the source to start, otherwise stop it.
    deadlineTimer.expires_from_now(boost::posix_time::milliseconds(500));
    try {
        co_await deadlineTimer.async_wait(asio::use_awaitable);
    } catch (const boost::system::system_error&) {}
    if (!isSourcePlayedByState(sourcePlayback)) {
        co_return;
    }
    co_await asyncCheckMediaStarted(sourcePlayback, *mediaStartedCallback);
    saveLastVideoSize(sourcePlayback);

    deadlineTimer.expires_from_now(getMediaEndDeadline(source));
    try {
        co_await deadlineTimer.async_wait(asio::use_awaitable);
    } catch (const boost::system::system_error&) {}
    co_await asyncStopObsSourceIfPlayedByState(sourcePlayback, true);
}

RewardRedemptionQueue::MediaStartedCallback::MediaStartedCallback(asio::io_context& ioContext) : ioContext(ioContext) {}

void RewardRedemptionQueue::MediaStartedCallback::setMediaStarted(void* param, [[maybe_unused]] calldata_t* data) {
    std::shared_ptr<MediaStartedCallback> callback = *static_cast<std::shared_ptr<MediaStartedCallback>*>(param);
    callback->ioContext.post([callback]() {
        if (callback->enabled) {
            callback->mediaStarted = true;
        }
    });
}

RewardRedemptionQueue::MediaEndedCallback::MediaEndedCallback(
    asio::io_context& ioContext,
    asio::deadline_timer& deadlineTimer
)
    : ioContext(ioContext), deadlineTimer(deadlineTimer) {}

void RewardRedemptionQueue::MediaEndedCallback::stopDeadlineTimer(void* param, [[maybe_unused]] calldata_t* data) {
    std::shared_ptr<MediaEndedCallback> callback = *static_cast<std::shared_ptr<MediaEndedCallback>*>(param);
    callback->ioContext.post([callback]() {
        if (callback->enabled) {
            callback->mediaEnded = true;
            callback->deadlineTimer.cancel();
        }
    });
}

boost::asio::awaitable<void> RewardRedemptionQueue::asyncCheckMediaStarted(
    SourcePlayback& sourcePlayback,
    MediaStartedCallback& mediaStartedCallback
) {
    if (!mediaStartedCallback.mediaStarted) {
        co_await asyncStopObsSourceIfPlayedByState(sourcePlayback, false);
        auto sourceName = obs_source_get_name(sourcePlayback.source);
        log(LOG_ERROR, "Source failed to start in time: {}", sourceName);
        throw ObsSourceNoVideoException(sourceName);
    }
}

void RewardRedemptionQueue::saveLastVideoSize(SourcePlayback& sourcePlayback) {
    std::uint32_t width = obs_source_get_width(sourcePlayback.source);
    std::uint32_t height = obs_source_get_height(sourcePlayback.source);
    if (width == 0 || height == 0) {
        return;
    }
    settings.setLastVideoSize(
        sourcePlayback.rewardId,
        obs_source_get_name(sourcePlayback.source),
        sourcePlayback.playlistIndex,
        sourcePlayback.playlistSize,
        std::make_pair(width, height)
    );
}

boost::posix_time::time_duration RewardRedemptionQueue::getMediaEndDeadline(obs_source_t* source) {
    std::int64_t durationMilliseconds = obs_source_media_get_duration(source);
    if (durationMilliseconds != -1) {
        std::int64_t deadlineMilliseconds = durationMilliseconds + durationMilliseconds / 2;
        return boost::posix_time::milliseconds(deadlineMilliseconds);
    } else {
        return boost::posix_time::pos_infin;
    }
}

asio::awaitable<void> RewardRedemptionQueue::asyncStopObsSourceIfPlayedByState(
    SourcePlayback& sourcePlayback,
    bool waitForHideTransition
) {
    if (isSourcePlayedByState(sourcePlayback)) {
        obs_source_t* source = sourcePlayback.source;
        co_await asyncStopObsSource(sourcePlayback, waitForHideTransition);
        sourcePlayedByState.erase(source);
        sourcePositionOnScenes.erase(source);
    }
}

bool RewardRedemptionQueue::isSourcePlayedByState(const SourcePlayback& sourcePlayback) {
    return sourcePlayedByState[sourcePlayback.source] == sourcePlayback.state;
}

asio::awaitable<void> RewardRedemptionQueue::asyncTestObsSource(
    std::string rewardId,
    std::string obsSourceName,
    bool randomPositionEnabled,
    QObjectCallback& callback
) {
    try {
        co_await asyncTestObsSource(rewardId, obsSourceName, randomPositionEnabled);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncTestObsSource: {}", exception.what());
        callback("std::exception_ptr", std::current_exception());
    }
}

asio::awaitable<void> RewardRedemptionQueue::asyncTestObsSource(
    const std::string& rewardId,
    const std::string& obsSourceName,
    bool randomPositionEnabled
) {
    OBSSourceAutoRelease obsSource = getObsSource(obsSourceName);
    if (!obsSource) {
        throw ObsSourceNotFoundException(obsSourceName);
    }
    co_await asyncPlayObsSource(rewardId, std::move(obsSource), randomPositionEnabled);
}

OBSSourceAutoRelease RewardRedemptionQueue::getObsSource(const RewardRedemption& rewardRedemption) {
    std::optional<std::string> obsSourceName = settings.getObsSourceName(rewardRedemption.reward.id);
    if (!obsSourceName) {
        return {};
    }
    return getObsSource(obsSourceName.value());
}

OBSSourceAutoRelease RewardRedemptionQueue::getObsSource(const std::string& obsSourceName) {
    OBSSourceAutoRelease source = obs_get_source_by_name(obsSourceName.c_str());
    if (!isMediaSource(source)) {
        return {};
    }
    return source;
}

void RewardRedemptionQueue::startObsSource(SourcePlayback& sourcePlayback) {
    if (isVlcSource(sourcePlayback.source)) {
        startVlcSource(sourcePlayback);
    } else {
        startMediaSource(sourcePlayback.source);
    }

    showObsSource(sourcePlayback);
}

void RewardRedemptionQueue::startVlcSource(SourcePlayback& sourcePlayback) {
    obs_source_t* source = sourcePlayback.source;
    OBSDataAutoRelease sourceSettings = obs_source_get_settings(source);
    if (!sourceSettings) {
        log(LOG_ERROR, "VLC Source settings are null");
        return;
    }
    OBSDataArrayAutoRelease playlist = obs_data_get_array(sourceSettings, "playlist");
    if (!playlist) {
        log(LOG_ERROR, "VLC Source playlist is null");
        return;
    }
    sourcePlayback.playlistSize = obs_data_array_count(playlist);
    if (sourcePlayback.playlistSize == 0) {
        log(LOG_ERROR, "VLC Source has an empty playlist");
        return;
    }

    std::uniform_int_distribution<std::size_t> randomSourceIndex(0, sourcePlayback.playlistSize - 1);
    sourcePlayback.playlistIndex = randomSourceIndex(randomEngine);

    obs_source_media_stop(source);
    for (std::size_t i = 0; i < sourcePlayback.playlistIndex + 1; i++) {
        obs_source_media_next(source);
    }
    obs_source_media_play_pause(source, false);
}

void RewardRedemptionQueue::startMediaSource(obs_source_t* source) {
    obs_source_media_restart(source);
}

void RewardRedemptionQueue::showObsSource(SourcePlayback& sourcePlayback) {
    struct ShowObsSourceCallback {
        SourcePlayback& sourcePlayback;
        std::map<std::string, vec2>& sourcePositionOnScenes;
        Settings& settings;
        std::default_random_engine& randomEngine;

        static bool showObsSourceOnScene(void* param, obs_source_t* sceneSource) {
            auto& [sourcePlayback, sourcePositionOnScenes, settings, randomEngine] =
                *static_cast<ShowObsSourceCallback*>(param);
            obs_scene_t* scene = obs_scene_from_source(sceneSource);
            std::string sceneUuid = obs_source_get_uuid(sceneSource);
            obs_sceneitem_t* sceneItem = findObsSource(scene, sourcePlayback.source);
            if (!sceneItem) {
                return true;
            }

            if (sourcePlayback.randomPositionEnabled) {
                if (!sourcePositionOnScenes.contains(sceneUuid)) {
                    sourcePositionOnScenes[sceneUuid] = getSourcePosition(scene, sceneItem);
                }
                RewardRedemptionQueue::setSourceRandomPosition(
                    sourcePlayback, scene, sceneItem, settings, randomEngine
                );
            }
            obs_sceneitem_set_visible(sceneItem, true);
            return true;
        }
    } callback{sourcePlayback, sourcePositionOnScenes[sourcePlayback.source], settings, randomEngine};

    obs_enum_scenes(&ShowObsSourceCallback::showObsSourceOnScene, &callback);
}

asio::awaitable<void> RewardRedemptionQueue::asyncStopObsSource(
    SourcePlayback& sourcePlayback,
    bool waitForHideTransition
) {
    co_await asyncHideObsSource(sourcePlayback, waitForHideTransition);
    obs_source_media_stop(sourcePlayback.source);
}

asio::awaitable<void> RewardRedemptionQueue::asyncHideObsSource(
    SourcePlayback& sourcePlayback,
    bool waitForHideTransition
) {
    // VLC source immediately switches to the next video (or to black if playing the last video),
    // so there's no good way to show the hide transition.
    bool vlcSource = isVlcSource(sourcePlayback.source);

    struct HideObsSourceCallback {
        obs_source_t* source;
        bool vlcSource;
        uint32_t hideTransitionDurationMs = 0;

        static bool hideObsSourceOnScene(void* param, obs_source_t* sceneSource) {
            auto& [source, vlcSource, hideTransitionDurationMs] = *static_cast<HideObsSourceCallback*>(param);
            obs_scene_t* scene = obs_scene_from_source(sceneSource);
            obs_sceneitem_t* sceneItem = findObsSource(scene, source);
            if (!sceneItem) {
                return true;
            }

            obs_sceneitem_set_visible(sceneItem, false);
            if (obs_sceneitem_get_transition(sceneItem, false)) {
                if (vlcSource) {
                    obs_sceneitem_set_transition(sceneItem, false, nullptr);
                } else {
                    hideTransitionDurationMs =
                        std::max(hideTransitionDurationMs, obs_sceneitem_get_transition_duration(sceneItem, false));
                }
            }
            return true;
        }
    } callback{sourcePlayback.source, vlcSource};

    obs_enum_scenes(&HideObsSourceCallback::hideObsSourceOnScene, &callback);

    if (vlcSource && sourcePlayback.playlistSize > 1) {
        // Playlists don't work well when chained together.
        callback.hideTransitionDurationMs = 500;
    }

    if (waitForHideTransition) {
        std::chrono::milliseconds hideTransitionDuration{callback.hideTransitionDurationMs};
        co_await asio::steady_timer(ioContext, hideTransitionDuration).async_wait(asio::use_awaitable);
    }
    restoreSourcePosition(sourcePlayback.source);
}

void RewardRedemptionQueue::restoreSourcePosition(obs_source_t* source) {
    struct RestoreSourcePositionCallback {
        obs_source_t* source;
        std::map<std::string, vec2>& sourcePositionOnScenes;

        static bool restoreSourcePositionOnScene(void* param, obs_source_t* sceneSource) {
            auto& [source, sourcePositionOnScenes] = *static_cast<RestoreSourcePositionCallback*>(param);
            obs_scene_t* scene = obs_scene_from_source(sceneSource);
            std::string sceneUuid = obs_source_get_uuid(sceneSource);
            if (!sourcePositionOnScenes.contains(sceneUuid)) {
                return true;
            }
            obs_sceneitem_t* sceneItem = findObsSource(scene, source);
            if (sceneItem) {
                setSourcePosition(scene, sceneItem, sourcePositionOnScenes[sceneUuid]);
            }
            return true;
        }
    } callback{source, sourcePositionOnScenes[source]};

    obs_enum_scenes(&RestoreSourcePositionCallback::restoreSourcePositionOnScene, &callback);
}

obs_sceneitem_t* RewardRedemptionQueue::findObsSource(obs_scene_t* scene, const obs_source_t* source) {
    return obs_scene_find_source_recursive(scene, obs_source_get_name(source));
}

void RewardRedemptionQueue::setSourceRandomPosition(
    SourcePlayback& sourcePlayback,
    obs_scene_t* scene,
    obs_scene_item* sceneItem,
    Settings& settings,
    std::default_random_engine& randomEngine
) {
    obs_source_t* source = obs_sceneitem_get_source(sceneItem);
    std::string sourceName = obs_source_get_name(source);
    std::optional<std::pair<std::uint32_t, std::uint32_t>> size =
        settings.getLastVideoSize(sourcePlayback.rewardId, sourceName, sourcePlayback.playlistIndex);
    if (!size.has_value()) {
        log(LOG_INFO, "Couldn't set random position for source {} - no size saved", sourceName);
        return;
    }

    auto [width, height] = size.value();
    obs_sceneitem_crop crop;
    obs_sceneitem_get_crop(sceneItem, &crop);
    width -= crop.left + crop.right;
    height -= crop.top + crop.bottom;

    vec2 scale = getSourceScale(scene, sceneItem);
    float scaledWidth = width * scale.x;
    float scaledHeight = height * scale.y;

    obs_video_info obsVideoInfo;
    obs_get_video_info(&obsVideoInfo);
    float maxX = std::max(0.f, obsVideoInfo.base_width - scaledWidth);
    float maxY = std::max(0.f, obsVideoInfo.base_height - scaledHeight);
    std::uniform_real_distribution<float> xDistribution(0, maxX);
    std::uniform_real_distribution<float> yDistribution(0, maxY);

    vec2 newPosition{xDistribution(randomEngine), yDistribution(randomEngine)};
    setSourcePosition(scene, sceneItem, newPosition);
}

vec2 RewardRedemptionQueue::getSourcePosition(obs_scene_t* scene, obs_scene_item* sceneItem) {
    vec2 position;
    obs_sceneitem_get_pos(sceneItem, &position);

    obs_scene_item* parentGroup = obs_sceneitem_get_group(scene, sceneItem);
    if (parentGroup) {
        vec2 parentPosition, parentScale;
        obs_sceneitem_get_pos(parentGroup, &parentPosition);
        obs_sceneitem_get_scale(parentGroup, &parentScale);

        vec2_mul(&position, &position, &parentScale);
        vec2_add(&position, &position, &parentPosition);
    }
    return position;
}

void RewardRedemptionQueue::setSourcePosition(obs_scene_t* scene, obs_scene_item* sceneItem, vec2 position) {
    obs_scene_item* parentGroup = obs_sceneitem_get_group(scene, sceneItem);
    if (parentGroup) {
        vec2 parentPosition, parentScale;
        obs_sceneitem_get_pos(parentGroup, &parentPosition);
        obs_sceneitem_get_scale(parentGroup, &parentScale);

        vec2_sub(&position, &position, &parentPosition);
        vec2_div(&position, &position, &parentScale);
    }
    obs_sceneitem_set_pos(sceneItem, &position);
}

vec2 RewardRedemptionQueue::getSourceScale(obs_scene_t* scene, obs_scene_item* sceneItem) {
    vec2 scale;
    obs_sceneitem_get_scale(sceneItem, &scale);
    obs_scene_item* parentGroup = obs_sceneitem_get_group(scene, sceneItem);
    if (parentGroup) {
        vec2 parentScale;
        obs_sceneitem_get_scale(parentGroup, &parentScale);
        vec2_mul(&scale, &scale, &parentScale);
    }
    return scale;
}

bool RewardRedemptionQueue::isMediaSource(const obs_source_t* source) {
    if (!source) {
        return false;
    }
    if (isVlcSource(source)) {
        return true;
    }
    const char* sourceId = obs_source_get_id(source);
    return std::strcmp(sourceId, "ffmpeg_source") == 0;
}

bool RewardRedemptionQueue::isVlcSource(const obs_source_t* source) {
    if (!source) {
        return false;
    }
    const char* sourceId = obs_source_get_id(source);
    return std::strcmp(sourceId, "vlc_source") == 0;
}
