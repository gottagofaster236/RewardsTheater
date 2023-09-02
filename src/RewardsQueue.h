// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include <deque>
#include <mutex>
#include <obs.hpp>
#include <optional>
#include <vector>

#include "Reward.h"

class RewardsQueue {
public:
    std::vector<RewardAndSource> getRewardsQueue() const;
    void queueReward(const RewardAndSource& reward);
    void removeReward(const RewardAndSource& reward);
    void playNextReward();

    static void playObsSource(const std::string& obsSourceName);
    static void stopObsSource(const std::string& obsSourceName);
    static bool isMediaSource(const std::string& obsSourceName);

private:
    static OBSSourceAutoRelease getObsSource(const std::string& obsSourceName);
    static void playObsSource(const obs_source_t* source);
    static void stopObsSource(const obs_source_t* source);
    static bool isMediaSource(const obs_source_t* source);

    std::optional<RewardAndSource> popNextReward();
    static void onMediaEnded(void* param, calldata_t* data);

    std::deque<RewardAndSource> rewardsQueue;
    mutable std::mutex rewardsMutex;
    OBSSignal mediaEndSignal;
};
