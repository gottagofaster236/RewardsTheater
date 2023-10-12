// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <obs.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "IoThreadPool.h"
#include "Reward.h"
#include "Settings.h"

class RewardsQueue {
public:
    RewardsQueue(const Settings& settings);
    ~RewardsQueue();

    std::vector<RewardRedemption> getRewardsQueue() const;
    void queueReward(const RewardRedemption& reward);
    void removeReward(const RewardRedemption& reward);

    void playObsSource(const std::string& obsSourceName);
    static std::vector<std::string> enumObsSources();

private:
    boost::asio::awaitable<void> asyncPlayRewardsFromQueue();
    boost::asio::awaitable<RewardRedemption> asyncGetNextReward();
    void playObsSource(OBSSourceAutoRelease source);
    boost::asio::awaitable<void> asyncPlayObsSource(OBSSourceAutoRelease source);
    boost::asio::deadline_timer createDeadlineTimer(obs_source_t* source);
    OBSSourceAutoRelease getObsSource(const RewardRedemption& reward);
    OBSSourceAutoRelease getObsSource(const std::string& sourceName);

    static void startObsSource(obs_source_t* source);
    static void stopObsSource(obs_source_t* source);
    static void setSourceVisible(obs_source_t* source, bool visible);
    static bool isMediaSource(const obs_source_t* source);

    const Settings& settings;

    IoThreadPool rewardsQueueThread;
    mutable std::mutex rewardsQueueMutex;
    boost::asio::deadline_timer rewardsQueueCondVar;
    std::vector<RewardRedemption> rewardsQueue;

    unsigned playObsSourceState = 0;
    std::map<obs_source_t*, unsigned> sourcePlayedByState;
};
