// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <util/config-file.h>

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

private:
    class UnsupportedObsVersionException : public std::exception {
        const char* what() const noexcept override;
    };

    class RestrictedRegionException : public std::exception {
        const char* what() const noexcept override;
    };

    static config_t* getConfig();
    void checkMinObsVersion();
    void checkRestrictedRegion();

    Settings settings;
    IoThreadPool ioThreadPool;
    HttpClient httpClient;
    TwitchAuth twitchAuth;
    TwitchRewardsApi twitchRewardsApi;
    GithubUpdateApi githubUpdateApi;
    RewardRedemptionQueue rewardRedemptionQueue;
    PubsubListener pubsubListener;
};
