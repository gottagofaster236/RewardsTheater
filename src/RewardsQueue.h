// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <deque>
#include <mutex>
#include <obs.hpp>
#include <optional>
#include <vector>

#include "Reward.h"
#include "Settings.h"

class RewardsQueue {
public:
    RewardsQueue(const Settings& settings);

    std::vector<Reward> getRewardsQueue() const;
    void queueReward(const Reward& reward);
    void removeReward(const Reward& reward);
    void playNextReward();

    static void playObsSource(const std::string& obsSourceName);
    static void stopObsSource(const std::string& obsSourceName);
    static bool isMediaSource(const std::string& obsSourceName);

private:
    static OBSSourceAutoRelease getObsSource(const std::string& obsSourceName);
    static void playObsSource(const obs_source_t* source);
    static void stopObsSource(const obs_source_t* source);
    static bool isMediaSource(const obs_source_t* source);

    std::optional<OBSSourceAutoRelease> popNextReward();
    static void onMediaEnded(void* param, calldata_t* data);

    std::deque<Reward> rewardsQueue;
    mutable std::mutex rewardsMutex;
    OBSSignal mediaEndSignal;

    const Settings& settings;
};
