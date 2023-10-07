// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include "GithubUpdateApi.h"
#include "IoThreadPool.h"
#include "RewardsQueue.h"
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
    RewardsQueue& getRewardsQueue();

private:
    Settings settings;
    IoThreadPool ioThreadPool;
    TwitchAuth twitchAuth;
    TwitchRewardsApi twitchRewardsApi;
    GithubUpdateApi githubUpdateApi;
    RewardsQueue rewardsQueue;
};
