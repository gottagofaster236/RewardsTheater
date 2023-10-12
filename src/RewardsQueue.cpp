// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardsQueue.h"

#include <algorithm>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <cstring>
#include <utility>

namespace asio = boost::asio;

RewardsQueue::RewardsQueue(const Settings& settings)
    : settings(settings), rewardsQueueThread(1),
      rewardsQueueCondVar(rewardsQueueThread.ioContext, boost::posix_time::pos_infin) {
    asio::co_spawn(rewardsQueueThread.ioContext, asyncPlaySourcesFromQueue(), asio::detached);
}

RewardsQueue::~RewardsQueue() {
    rewardsQueueThread.stop();
}

std::vector<Reward> RewardsQueue::getRewardsQueue() const {
    std::lock_guard<std::mutex> guard(rewardsQueueMutex);
    return rewardsQueue;
}

void RewardsQueue::queueReward(const Reward& reward) {
    if (!settings.isRewardsQueueEnabled()) {
        std::optional<std::string> obsSourceName = settings.getObsSourceName(reward.id);
        if (obsSourceName.has_value()) {
            playObsSource(obsSourceName.value());
        }
        return;
    }

    {
        std::lock_guard<std::mutex> guard(rewardsQueueMutex);
        rewardsQueue.push_back(reward);
    }
    rewardsQueueThread.ioContext.post([this]() {
        rewardsQueueCondVar.cancel();  // Equivalent to notify_all() in a condition variable
    });
}

void RewardsQueue::removeReward(const Reward& reward) {
    stopObsSource(getObsSource(reward));
    std::lock_guard<std::mutex> guard(rewardsQueueMutex);
    auto position = std::find(rewardsQueue.begin(), rewardsQueue.end(), reward);
    if (position != rewardsQueue.end()) {
        rewardsQueue.erase(position);
    }
}

void RewardsQueue::playObsSource(const std::string& obsSourceName) {
    asio::co_spawn(rewardsQueueThread.ioContext, asyncPlayObsSource(getObsSource(obsSourceName)), asio::detached);
}

std::vector<std::string> RewardsQueue::enumObsSources() {
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

asio::awaitable<void> RewardsQueue::asyncPlaySourcesFromQueue() {
    while (true) {
        Reward nextReward = co_await asyncGetNextReward();
        co_await asyncPlayObsSource(getObsSource(nextReward));
        auto timeBeforeNextReward = std::chrono::seconds(settings.getIntervalBetweenRewardsSeconds());
        co_await asio::steady_timer(rewardsQueueThread.ioContext, timeBeforeNextReward).async_wait(asio::use_awaitable);
    }
}

asio::awaitable<Reward> RewardsQueue::asyncGetNextReward() {
    while (true) {
        {
            std::lock_guard guard(rewardsQueueMutex);
            if (!rewardsQueue.empty()) {
                Reward result = rewardsQueue.front();
                rewardsQueue.erase(rewardsQueue.begin());
                co_return result;
            }
        }
        try {
            co_await rewardsQueueCondVar.async_wait(asio::use_awaitable);
        } catch (const boost::system::system_error&) {
            // Condition variable signalled.
        }
    }
}

asio::awaitable<void> RewardsQueue::asyncPlayObsSource(OBSSourceAutoRelease source) {
    if (!source) {
        co_return;
    }
    unsigned state = playObsSourceState++;
    sourcePlayedByState[source] = state;

    asio::deadline_timer deadlineTimer = createDeadlineTimer(source);

    struct StopDeadlineTimerCallback {
        asio::deadline_timer& deadlineTimer;
        asio::io_context& ioContext;

        static void stopDeadlineTimer(void* param, [[maybe_unused]] calldata_t* data) {
            auto& [deadlineTimer, ioContext] = *static_cast<StopDeadlineTimerCallback*>(param);
            // Posting to ioContext so that cancellation occurs after the async_wait (we have one thread).
            ioContext.post([&deadlineTimer]() {
                deadlineTimer.cancel();
            });
        }
    } callback(deadlineTimer, rewardsQueueThread.ioContext);

    signal_handler_t* signalHandler = obs_source_get_signal_handler(source);
    OBSSignal mediaEndedSignal(signalHandler, "media_ended", &StopDeadlineTimerCallback::stopDeadlineTimer, &callback);
    startObsSource(source);

    try {
        co_await deadlineTimer.async_wait(asio::use_awaitable);
    } catch (const boost::system::system_error&) {
        // Timer cancelled by the media_ended signal
    }

    if (sourcePlayedByState[source] == state) {
        sourcePlayedByState.erase(source);
        mediaEndedSignal.Disconnect();
        stopObsSource(source);
    }
}

asio::deadline_timer RewardsQueue::createDeadlineTimer(obs_source_t* source) {
    asio::deadline_timer deadlineTimer(rewardsQueueThread.ioContext);

    std::int64_t durationMilliseconds = obs_source_media_get_duration(source);
    if (durationMilliseconds != -1) {
        std::int64_t deadlineMilliseconds = durationMilliseconds + durationMilliseconds / 2;
        deadlineTimer.expires_from_now(boost::posix_time::milliseconds(deadlineMilliseconds));
    } else {
        deadlineTimer.expires_from_now(boost::posix_time::pos_infin);
    }

    return deadlineTimer;
}

OBSSourceAutoRelease RewardsQueue::getObsSource(const Reward& reward) {
    std::optional<std::string> obsSourceName = settings.getObsSourceName(reward.id);
    if (!obsSourceName) {
        return {};
    }
    return getObsSource(obsSourceName.value());
}

OBSSourceAutoRelease RewardsQueue::getObsSource(const std::string& obsSourceName) {
    OBSSourceAutoRelease source = obs_get_source_by_name(obsSourceName.c_str());
    if (!isMediaSource(source)) {
        return {};
    }
    return source;
}

void RewardsQueue::startObsSource(obs_source_t* source) {
    if (!source) {
        return;
    }
    setSourceVisible(source, true);
    obs_source_media_restart(source);
}

void RewardsQueue::stopObsSource(obs_source_t* source) {
    if (!source) {
        return;
    }
    obs_source_media_stop(source);
    setSourceVisible(source, false);
}

void RewardsQueue::setSourceVisible(obs_source_t* source, bool visible) {
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

bool RewardsQueue::isMediaSource(const obs_source_t* source) {
    if (!source) {
        return false;
    }
    const char* sourceId = obs_source_get_id(source);
    return std::strcmp(sourceId, "ffmpeg_source") == 0 || std::strcmp(sourceId, "vlc_source") == 0;
}
