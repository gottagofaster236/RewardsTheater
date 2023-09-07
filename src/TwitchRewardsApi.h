// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <TwitchAuth.h>

#include <string>

class TwitchRewardsApi {
public:
    TwitchRewardsApi(TwitchAuth& twitchAuth);

private:
    TwitchAuth& twitchAuth;
};
