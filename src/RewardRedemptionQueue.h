// SPDX-License-Identifier: GPL-3.0-only
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
#include <random>
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
    RewardRedemptionQueue(Settings& settings, TwitchRewardsApi& twitchRewardsApi);
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
    void testObsSource(
        const std::string& rewardId,
        const std::string& obsSourceName,
        bool randomPositionEnabled,
        QObject* receiver,
        const char* member
    );

signals:
    void onRewardRedemptionQueueUpdated(const std::vector<RewardRedemption> rewardRedemptionQueue);

private:
    boost::asio::awaitable<void> asyncPlayRewardRedemptionsFromQueue();
    boost::asio::awaitable<RewardRedemption> asyncGetNextRewardRedemption();
    void notifyRewardRedemptionQueueCondVar();
    boost::asio::awaitable<void> popPlayedRewardRedemptionFromQueue(const RewardRedemption& rewardRedemption);

    void playObsSource(const std::string& rewardId, const std::string& obsSourceName, bool randomPositionEnabled);
    void playObsSource(const std::string& rewardId, OBSSourceAutoRelease source, bool randomPositionEnabled);

    boost::asio::awaitable<void> asyncPlayObsSource(
        std::string rewardId,
        OBSSourceAutoRelease source,
        bool randomPositionEnabled
    );

    struct SourcePlayback {
        const unsigned state;
        const std::string rewardId;
        obs_source_t* const source;
        const bool randomPositionEnabled;
        std::size_t playlistIndex;
        std::size_t playlistSize;
    };

    struct MediaStartedCallback {
        boost::asio::io_context& ioContext;
        bool mediaStarted = false;
        bool enabled = true;

        MediaStartedCallback(boost::asio::io_context& ioContext);
        static void setMediaStarted(void* param, calldata_t* data);
    };

    struct MediaEndedCallback {
        boost::asio::io_context& ioContext;
        boost::asio::deadline_timer& deadlineTimer;
        bool mediaEnded = false;
        bool enabled = true;

        MediaEndedCallback(boost::asio::io_context& ioContext, boost::asio::deadline_timer& deadlineTimer);
        static void stopDeadlineTimer(void* param, calldata_t* data);
    };

    boost::asio::awaitable<void> asyncCheckMediaStarted(
        SourcePlayback& sourcePlayback,
        MediaStartedCallback& mediaStartedCallback
    );
    void saveLastVideoSize(SourcePlayback& sourcePlayback);
    boost::posix_time::time_duration getMediaEndDeadline(obs_source_t* source);
    boost::asio::awaitable<void> asyncStopObsSourceIfPlayedByState(
        SourcePlayback& sourcePlayback,
        bool waitForHideTransition
    );
    bool isSourcePlayedByState(const SourcePlayback& sourcePlayback);

    boost::asio::awaitable<void> asyncTestObsSource(
        std::string rewardId,
        std::string obsSourceName,
        bool randomPositionEnabled,
        QObjectCallback& callback
    );
    boost::asio::awaitable<void> asyncTestObsSource(
        const std::string& rewardId,
        const std::string& obsSourceName,
        bool randomPositionEnabled
    );

    OBSSourceAutoRelease getObsSource(const RewardRedemption& rewardRedemption);
    OBSSourceAutoRelease getObsSource(const std::string& sourceName);

    void startObsSource(SourcePlayback& sourcePlayback);
    void startVlcSource(SourcePlayback& sourcePlayback);
    void startMediaSource(obs_source_t* source);

    void showObsSource(SourcePlayback& sourcePlayback);
    boost::asio::awaitable<void> asyncStopObsSource(SourcePlayback& sourcePlayback, bool waitForHideTransition);
    boost::asio::awaitable<void> asyncHideObsSource(SourcePlayback& sourcePlayback, bool waitForHideTransition);
    void restoreSourcePosition(obs_source_t* source);
    static obs_sceneitem_t* findObsSource(obs_scene_t* scene, const obs_source_t* source);

    static void setSourceRandomPosition(
        SourcePlayback& sourcePlayback,
        obs_scene_t* scene,
        obs_scene_item* sceneItem,
        Settings& settings,
        std::default_random_engine& randomEngine
    );
    static vec2 getSourcePosition(obs_scene_t* scene, obs_scene_item* sceneItem);
    static void setSourcePosition(obs_scene_t* scene, obs_scene_item* sceneItem, vec2 position);
    static vec2 getSourceScale(obs_scene_t* scene, obs_scene_item* sceneItem);
    static bool isMediaSource(const obs_source_t* source);
    static bool isVlcSource(const obs_source_t* source);

    Settings& settings;
    TwitchRewardsApi& twitchRewardsApi;

    IoThreadPool rewardRedemptionQueueThread;
    boost::asio::io_context& ioContext;
    std::vector<RewardRedemption> rewardRedemptionQueue;
    bool rewardPlaybackPaused;
    mutable std::mutex rewardRedemptionQueueMutex;
    boost::asio::deadline_timer rewardRedemptionQueueCondVar;

    unsigned playObsSourceState;
    std::map<obs_source_t*, unsigned> sourcePlayedByState;
    std::map<obs_source_t*, std::map<std::string, vec2>> sourcePositionOnScenes;

    std::default_random_engine randomEngine;
};
