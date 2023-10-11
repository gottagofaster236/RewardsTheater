// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <chrono>
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

    std::vector<Reward> getRewardsQueue() const;
    void queueReward(const Reward& reward);
    void removeReward(const Reward& reward);

    void playObsSource(const std::string& obsSourceName);
    static std::vector<std::string> enumObsSources();

private:
    boost::asio::awaitable<void> asyncPlaySourcesFromQueue();
    boost::asio::awaitable<Reward> asyncGetNextReward();
    boost::asio::awaitable<void> asyncPlayObsSource(OBSSourceAutoRelease source);
    OBSSourceAutoRelease getObsSource(const Reward& reward);
    OBSSourceAutoRelease getObsSource(const std::string& sourceName);

    static void startObsSource(obs_source_t* source);
    static void stopObsSource(obs_source_t* source);
    static void setSourceVisible(obs_source_t* source, bool visible);
    static bool isMediaSource(const obs_source_t* source);

    const Settings& settings;

    IoThreadPool rewardsQueueThread;
    mutable std::mutex rewardsQueueMutex;
    boost::asio::deadline_timer rewardsQueueCondVar;
    std::vector<Reward> rewardsQueue;
};
