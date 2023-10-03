// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include "RewardsQueue.h"
#include "Settings.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"
#include "IoThreadPool.h"

class RewardsTheaterPlugin {
public:
    RewardsTheaterPlugin();
    ~RewardsTheaterPlugin();
    Settings& getSettings();
    TwitchAuth& getTwitchAuth();
    TwitchRewardsApi& getTwitchRewardsApi();
    RewardsQueue& getRewardsQueue();

private:
    Settings settings;
    IoThreadPool ioThreadPool;
    TwitchAuth twitchAuth;
    TwitchRewardsApi twitchRewardsApi;
    RewardsQueue rewardsQueue;
};
