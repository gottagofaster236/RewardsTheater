// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QObject>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <obs.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "IoThreadPool.h"
#include "Reward.h"
#include "Settings.h"
#include "TwitchRewardsApi.h"

class RewardRedemptionQueue : public QObject {
    Q_OBJECT

public:
    RewardRedemptionQueue(const Settings& settings, TwitchRewardsApi& twitchRewardsApi);
    ~RewardRedemptionQueue();

    std::vector<RewardRedemption> getRewardRedemptionQueue() const;
    void queueRewardRedemption(const RewardRedemption& rewardRedemption);
    void removeRewardRedemption(const RewardRedemption& rewardRedemption);

    static std::vector<std::string> enumObsSources();

    bool isRewardPlaybackPaused() const;
    void setRewardPlaybackPaused(bool paused);

    class ObsSourceNotFoundException : public std::exception {
    public:
        ObsSourceNotFoundException(const std::string& obsSourceName);
        const char* what() const noexcept override;
        const std::string obsSourceName;
    };

    class ObsSourceNoVideoException : public std::exception {
    public:
        ObsSourceNoVideoException(const std::string& obsSourceName);
        const char* what() const noexcept override;
        const std::string obsSourceName;
    };

    /// Plays back the source as a test. Calls the receiver with an std::exception_ptr if an exception happens.
    void testObsSource(const std::string& obsSourceName, QObject* receiver, const char* member);

signals:
    void onRewardRedemptionQueueUpdated(const std::vector<RewardRedemption> rewardRedemptionQueue);

private:
    boost::asio::awaitable<void> asyncPlayRewardRedemptionsFromQueue();
    boost::asio::awaitable<RewardRedemption> asyncGetNextRewardRedemption();
    void notifyRewardRedemptionQueueCondVar();
    boost::asio::awaitable<void> popPlayedRewardRedemptionFromQueue(const RewardRedemption& rewardRedemption);

    void playObsSource(const std::string& obsSourceName);
    void playObsSource(OBSSourceAutoRelease source);

    boost::asio::awaitable<void> asyncPlayObsSource(OBSSourceAutoRelease source);
    boost::posix_time::time_duration getMediaEndDeadline(obs_source_t* source);

    boost::asio::awaitable<void> asyncTestObsSource(std::string obsSourceName, QObjectCallback& callback);
    boost::asio::awaitable<void> asyncTestObsSource(const std::string& obsSourceName);

    OBSSourceAutoRelease getObsSource(const RewardRedemption& rewardRedemption);
    OBSSourceAutoRelease getObsSource(const std::string& sourceName);

    static void startObsSource(obs_source_t* source);
    static void stopObsSource(obs_source_t* source);
    static void setSourceVisible(obs_source_t* source, bool visible);
    static bool isMediaSource(const obs_source_t* source);

    const Settings& settings;
    TwitchRewardsApi& twitchRewardsApi;

    IoThreadPool rewardRedemptionQueueThread;
    boost::asio::io_context& ioContext;
    std::vector<RewardRedemption> rewardRedemptionQueue;
    bool rewardPlaybackPaused;
    mutable std::mutex rewardRedemptionQueueMutex;
    boost::asio::deadline_timer rewardRedemptionQueueCondVar;

    unsigned playObsSourceState;
    std::map<obs_source_t*, unsigned> sourcePlayedByState;
};
