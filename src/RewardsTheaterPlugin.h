// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include "Settings.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"

class RewardsTheaterPlugin {
public:
    RewardsTheaterPlugin();
    Settings& getSettings();
    TwitchAuth& getTwitchAuth();
    TwitchRewardsApi& getTwitchRewardsApi();

private:
    Settings settings;
    TwitchAuth twitchAuth;
    TwitchRewardsApi twitchRewardsApi;
};
