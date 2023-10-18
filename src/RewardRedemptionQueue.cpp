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

RewardRedemptionQueue::RewardRedemptionQueue(const Settings& settings, TwitchRewardsApi& twitchRewardsApi)
    : settings(settings), twitchRewardsApi(twitchRewardsApi), rewardRedemptionQueueThread(1),
      ioContext(rewardRedemptionQueueThread.ioContext), rewardPlaybackPaused(false),
      rewardRedemptionQueueCondVar(ioContext, boost::posix_time::pos_infin), playObsSourceState(0) {
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
        playObsSource(obsSourceName.value());
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

    stopObsSource(getObsSource(rewardRedemption));
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

void RewardRedemptionQueue::testObsSource(const std::string& obsSourceName, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext, asyncTestObsSource(obsSourceName, *(new QObjectCallback(this, receiver, member))), asio::detached
    );
}

asio::awaitable<void> RewardRedemptionQueue::asyncPlayRewardRedemptionsFromQueue() {
    while (true) {
        RewardRedemption nextRewardRedemption = co_await asyncGetNextRewardRedemption();
        try {
            co_await asyncPlayObsSource(getObsSource(nextRewardRedemption));
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

void RewardRedemptionQueue::playObsSource(const std::string& obsSourceName) {
    playObsSource(getObsSource(obsSourceName));
}

void RewardRedemptionQueue::playObsSource(OBSSourceAutoRelease source) {
    asio::co_spawn(ioContext, asyncPlayObsSource(std::move(source)), asio::detached);
}

struct MediaStartedCallback {
    asio::io_context& ioContext;
    asio::deadline_timer& deadlineTimer;
    boost::posix_time::time_duration mediaEndDeadline;
    bool mediaStarted = false;
    bool enabled = true;

    static void setMediaEndedDeadline(void* param, [[maybe_unused]] calldata_t* data) {
        std::shared_ptr<MediaStartedCallback> callback = *static_cast<std::shared_ptr<MediaStartedCallback>*>(param);
        callback->ioContext.post([callback]() {
            if (callback->enabled) {
                // The media actually started - now set a deadline for it to end.
                // This cancels the timer, therefore waiting on it should be performed a loop.
                callback->mediaStarted = true;
                callback->deadlineTimer.expires_from_now(callback->mediaEndDeadline);
            }
        });
    }
};

struct MediaEndedCallback {
    asio::io_context& ioContext;
    asio::deadline_timer& deadlineTimer;
    bool mediaEnded = false;
    bool enabled = true;

    static void stopDeadlineTimer(void* param, [[maybe_unused]] calldata_t* data) {
        std::shared_ptr<MediaEndedCallback> callback = *static_cast<std::shared_ptr<MediaEndedCallback>*>(param);
        callback->ioContext.post([callback]() {
            if (callback->enabled) {
                callback->mediaEnded = true;
                callback->deadlineTimer.cancel();
            }
        });
    }
};

/// First the function sets the deadline timer for the source to start and waits for it.
/// If the source does start, then the deadline timer is updated to 1.5 times the source duration.
/// The deadline timer is cancelled when the source stops, therefore control returns to the function again.
asio::awaitable<void> RewardRedemptionQueue::asyncPlayObsSource(OBSSourceAutoRelease source) {
    if (!source) {
        co_return;
    }
    unsigned state = playObsSourceState++;
    sourcePlayedByState[source] = state;

    // Give some time for the source to start, otherwise stop it.
    asio::deadline_timer deadlineTimer(ioContext, boost::posix_time::milliseconds(500));

    auto mediaStartedCallback =
        std::make_shared<MediaStartedCallback>(ioContext, deadlineTimer, getMediaEndDeadline(source));
    auto mediaEndedCallback = std::make_shared<MediaEndedCallback>(ioContext, deadlineTimer);

    {
        signal_handler_t* signalHandler = obs_source_get_signal_handler(source);
        OBSSignal mediaStartedSignal(
            signalHandler, "media_started", &MediaStartedCallback::setMediaEndedDeadline, &mediaStartedCallback
        );
        OBSSignal mediaEndedSignal(
            signalHandler, "media_ended", &MediaEndedCallback::stopDeadlineTimer, &mediaEndedCallback
        );
        OBSSignal mediaStoppedSignal(
            signalHandler, "media_stopped", &MediaEndedCallback::stopDeadlineTimer, &mediaEndedCallback
        );
        startObsSource(source);

        while (!mediaEndedCallback->mediaEnded) {
            try {
                co_await deadlineTimer.async_wait(asio::use_awaitable);
                // We either hit the deadline of media starting or media ending.
                break;
            } catch (const boost::system::system_error&) {
                // Timer cancelled by "media_started", "media_ended" or "media_stopped" signals.
            }
        }

        mediaStartedCallback->enabled = mediaEndedCallback->enabled = false;
    }

    if (sourcePlayedByState[source] == state) {
        sourcePlayedByState.erase(source);
        stopObsSource(source);
    }

    if (!mediaStartedCallback->mediaStarted) {
        auto sourceName = obs_source_get_name(source);
        log(LOG_ERROR, "Source failed to start in time: {}", sourceName);
        throw ObsSourceNoVideoException(sourceName);
    }
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

asio::awaitable<void> RewardRedemptionQueue::asyncTestObsSource(std::string obsSourceName, QObjectCallback& callback) {
    try {
        co_await asyncTestObsSource(obsSourceName);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncTestObsSource: {}", exception.what());
        callback("std::exception_ptr", std::current_exception());
    }
}

asio::awaitable<void> RewardRedemptionQueue::asyncTestObsSource(const std::string& obsSourceName) {
    OBSSourceAutoRelease obsSource = getObsSource(obsSourceName);
    if (!obsSource) {
        throw ObsSourceNotFoundException(obsSourceName);
    }
    co_await asyncPlayObsSource(std::move(obsSource));
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

void RewardRedemptionQueue::startObsSource(obs_source_t* source) {
    if (!source) {
        return;
    }
    setSourceVisible(source, true);
    obs_source_media_restart(source);
}

void RewardRedemptionQueue::stopObsSource(obs_source_t* source) {
    if (!source) {
        return;
    }
    obs_source_media_stop(source);
    setSourceVisible(source, false);
}

void RewardRedemptionQueue::setSourceVisible(obs_source_t* source, bool visible) {
    struct SetSourceVisibleCallback {
        obs_source_t* source;
        bool visible;

        static bool setSourceVisibleOnScene(void* param, obs_source_t* sceneParam) {
            auto [source, visible] = *static_cast<SetSourceVisibleCallback*>(param);
            obs_scene_t* scene = obs_scene_from_source(sceneParam);
            obs_sceneitem_t* sceneItem = obs_scene_find_source(scene, obs_source_get_name(source));
            if (sceneItem) {
                obs_sceneitem_set_visible(sceneItem, visible);
            }
            return true;
        }
    } callback(source, visible);

    obs_enum_scenes(&SetSourceVisibleCallback::setSourceVisibleOnScene, &callback);
}

bool RewardRedemptionQueue::isMediaSource(const obs_source_t* source) {
    if (!source) {
        return false;
    }
    const char* sourceId = obs_source_get_id(source);
    return std::strcmp(sourceId, "ffmpeg_source") == 0 || std::strcmp(sourceId, "vlc_source") == 0;
}
