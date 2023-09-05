// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardsQueue.h"

#include <cstring>

std::vector<RewardAndSource> RewardsQueue::getRewardsQueue() const {
    std::lock_guard<std::mutex> lock(rewardsMutex);
    auto rewardsQueueCopy = rewardsQueue;
    return std::vector<RewardAndSource>(
        std::make_move_iterator(rewardsQueueCopy.begin()), std::make_move_iterator(rewardsQueueCopy.end())
    );
}

void RewardsQueue::queueReward(const RewardAndSource& reward) {
    bool playImmediately;
    {
        std::lock_guard<std::mutex> lock(rewardsMutex);
        playImmediately = rewardsQueue.empty();
        rewardsQueue.push_back(reward);
    }
    if (playImmediately) {
        playNextReward();
    }
}

void RewardsQueue::removeReward(const RewardAndSource& reward) {
    std::lock_guard<std::mutex> lock(rewardsMutex);
    auto position = std::find(rewardsQueue.begin(), rewardsQueue.end(), reward);
    if (position != rewardsQueue.end()) {
        rewardsQueue.erase(position);
    }
}

void RewardsQueue::playNextReward() {
    std::optional<RewardAndSource> rewardOptional = popNextReward();
    if (!rewardOptional) {
        return;
    }
    RewardAndSource reward = *rewardOptional;
    OBSSourceAutoRelease source = obs_get_source_by_name(reward.obsSourceName.c_str());

    signal_handler_t* sh = obs_source_get_signal_handler(source);
    mediaEndSignal = OBSSignal(sh, "media_ended", &onMediaEnded, this);

    proc_handler_call(obs_source_get_proc_handler(source), "restart", nullptr);
}

void RewardsQueue::playObsSource(const std::string& obsSourceName) {
    playObsSource(getObsSource(obsSourceName));
}

void RewardsQueue::stopObsSource(const std::string& obsSourceName) {
    stopObsSource(getObsSource(obsSourceName));
}

bool RewardsQueue::isMediaSource(const std::string& obsSourceName) {
    return isMediaSource(getObsSource(obsSourceName));
}

OBSSourceAutoRelease RewardsQueue::getObsSource(const std::string& obsSourceName) {
    return obs_get_source_by_name(obsSourceName.c_str());
}

void RewardsQueue::playObsSource(const obs_source_t* source) {
    proc_handler_call(obs_source_get_proc_handler(source), "restart", nullptr);
}

void RewardsQueue::stopObsSource(const obs_source_t* source) {
    proc_handler_call(obs_source_get_proc_handler(source), "stop", nullptr);
}

bool RewardsQueue::isMediaSource(const obs_source_t* source) {
    return std::strcmp(obs_source_get_id(source), "ffmpeg_source") == 0;
}

std::optional<RewardAndSource> RewardsQueue::popNextReward() {
    std::lock_guard<std::mutex> lock(rewardsMutex);
    while (!rewardsQueue.empty()) {
        RewardAndSource nextReward = rewardsQueue.front();
        rewardsQueue.pop_front();
        if (isMediaSource(nextReward.obsSourceName)) {
            return nextReward;
        }
    }
    return {};
}

void RewardsQueue::onMediaEnded(void* param, [[maybe_unused]] calldata_t* data) {
    static_cast<RewardsQueue*>(param)->playNextReward();
}
