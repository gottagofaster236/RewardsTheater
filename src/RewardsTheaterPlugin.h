// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <exception>

#include "GithubUpdateApi.h"
#include "HttpClient.h"
#include "IoThreadPool.h"
#include "PubsubListener.h"
#include "RewardRedemptionQueue.h"
#include "Settings.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"

class RewardsTheaterPlugin {
public:
    RewardsTheaterPlugin();
    ~RewardsTheaterPlugin();
    Settings& getSettings();
    TwitchAuth& getTwitchAuth();
    TwitchRewardsApi& getTwitchRewardsApi();
    GithubUpdateApi& getGithubUpdateApi();
    RewardRedemptionQueue& getRewardRedemptionQueue();

    class UnsupportedObsVersionException : public std::exception {
        const char* what() const noexcept override;
    };

private:
    void checkMinObsVersion();

    Settings settings;
    IoThreadPool ioThreadPool;
    HttpClient httpClient;
    TwitchAuth twitchAuth;
    TwitchRewardsApi twitchRewardsApi;
    GithubUpdateApi githubUpdateApi;
    RewardRedemptionQueue rewardRedemptionQueue;
    PubsubListener pubsubListener;
};
