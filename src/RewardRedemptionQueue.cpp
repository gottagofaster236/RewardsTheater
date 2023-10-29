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
    {
        std::lock_guard<std::mutex> guard(rewardRedemptionQueueMutex);
        auto position = std::find(rewardRedemptionQueue.begin(), rewardRedemptionQueue.end(), rewardRedemption);
        if (position == rewardRedemptionQueue.end()) {
            return;
        }
        rewardRedemptionQueue.erase(position);
        emit onRewardRedemptionQueueUpdated(rewardRedemptionQueue);
    }

    obs_source_media_stop(getObsSource(rewardRedemption));
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
    ObsSignalWithCallback mediaEndedSignal(
        source, "media_ended", &MediaEndedCallback::stopDeadlineTimer, mediaEndedCallback
    );
    ObsSignalWithCallback mediaStoppedSignal(
        source, "media_stopped", &MediaEndedCallback::stopDeadlineTimer, mediaEndedCallback
    );

    SourcePlayback sourcePlayback{state, rewardId, source, randomPositionEnabled};
    startObsSource(sourcePlayback);
    // Give some time for the source to start, otherwise stop it.
    deadlineTimer.expires_from_now(boost::posix_time::milliseconds(500));
    try {
        co_await deadlineTimer.async_wait(asio::use_awaitable);
    } catch (const boost::system::system_error&) {}
    checkMediaStarted(sourcePlayback, *mediaStartedCallback);
    saveLastVideoSize(sourcePlayback);

    deadlineTimer.expires_from_now(getMediaEndDeadline(source));
    try {
        co_await deadlineTimer.async_wait(asio::use_awaitable);
    } catch (const boost::system::system_error&) {}
    stopObsSourceIfPlayedByState(sourcePlayback);
}

void RewardRedemptionQueue::MediaStartedCallback::setMediaStarted(void* param, [[maybe_unused]] calldata_t* data) {
    std::shared_ptr<MediaStartedCallback> callback = *static_cast<std::shared_ptr<MediaStartedCallback>*>(param);
    callback->ioContext.post([callback]() {
        if (callback->enabled) {
            callback->mediaStarted = true;
        }
    });
}

void RewardRedemptionQueue::MediaEndedCallback::stopDeadlineTimer(void* param, [[maybe_unused]] calldata_t* data) {
    std::shared_ptr<MediaEndedCallback> callback = *static_cast<std::shared_ptr<MediaEndedCallback>*>(param);
    callback->ioContext.post([callback]() {
        if (callback->enabled) {
            callback->mediaEnded = true;
            callback->deadlineTimer.cancel();
        }
    });
}

void RewardRedemptionQueue::checkMediaStarted(
    SourcePlayback& sourcePlayback,
    MediaStartedCallback& mediaStartedCallback
) {
    if (!mediaStartedCallback.mediaStarted) {
        stopObsSourceIfPlayedByState(sourcePlayback);
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
        sourcePlayback.rewardId, obs_source_get_name(sourcePlayback.source), std::make_pair(width, height)
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

void RewardRedemptionQueue::stopObsSourceIfPlayedByState(SourcePlayback& sourcePlayback) {
    if (sourcePlayedByState[sourcePlayback.source] == sourcePlayback.state) {
        stopObsSource(sourcePlayback);
        sourcePlayedByState.erase(sourcePlayback.source);
        sourcePositionOnScenes.erase(sourcePlayback.source);
    }
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
    setSourceVisible(sourcePlayback, true);
    obs_source_media_restart(sourcePlayback.source);
}

void RewardRedemptionQueue::stopObsSource(SourcePlayback& sourcePlayback) {
    obs_source_media_stop(sourcePlayback.source);
    setSourceVisible(sourcePlayback, false);
}

void RewardRedemptionQueue::setSourceVisible(SourcePlayback& sourcePlayback, bool visible) {
    struct SetSourceVisibleCallback {
        SourcePlayback& sourcePlayback;
        bool visible;
        std::map<std::string, vec2>& sourcePositionOnScenes;
        Settings& settings;
        std::default_random_engine& randomEngine;

        static bool setSourceVisibleOnScene(void* param, obs_source_t* sceneSource) {
            auto& [sourcePlayback, visible, sourcePositionOnScenes, settings, randomEngine] =
                *static_cast<SetSourceVisibleCallback*>(param);
            obs_scene_t* scene = obs_scene_from_source(sceneSource);
            std::string sceneUuid = obs_source_get_uuid(sceneSource);

            obs_sceneitem_t* sceneItem =
                obs_scene_find_source_recursive(scene, obs_source_get_name(sourcePlayback.source));
            if (!sceneItem) {
                return true;
            }

            if (visible) {
                if (!sourcePositionOnScenes.contains(sceneUuid)) {
                    sourcePositionOnScenes[sceneUuid] = getSourcePosition(scene, sceneItem);
                }
                if (sourcePlayback.randomPositionEnabled) {
                    RewardRedemptionQueue::setSourceRandomPosition(
                        sourcePlayback.rewardId, scene, sceneItem, settings, randomEngine
                    );
                }
                obs_sceneitem_set_visible(sceneItem, true);
            } else {
                obs_sceneitem_set_visible(sceneItem, false);
                setSourcePosition(scene, sceneItem, sourcePositionOnScenes[sceneUuid]);
            }

            return true;
        }
    } callback(sourcePlayback, visible, sourcePositionOnScenes[sourcePlayback.source], settings, randomEngine);

    obs_enum_scenes(&SetSourceVisibleCallback::setSourceVisibleOnScene, &callback);
}

void RewardRedemptionQueue::setSourceRandomPosition(
    const std::string& rewardId,
    obs_scene_t* scene,
    obs_scene_item* sceneItem,
    Settings& settings,
    std::default_random_engine& randomEngine
) {
    obs_source_t* source = obs_sceneitem_get_source(sceneItem);
    std::string sourceName = obs_source_get_name(source);
    std::optional<std::pair<std::uint32_t, std::uint32_t>> size = settings.getLastVideoSize(rewardId, sourceName);
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
    const char* sourceId = obs_source_get_id(source);
    return std::strcmp(sourceId, "ffmpeg_source") == 0 || std::strcmp(sourceId, "vlc_source") == 0;
}
