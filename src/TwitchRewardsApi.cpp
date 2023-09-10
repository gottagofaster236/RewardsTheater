// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchRewardsApi.h"

TwitchRewardsApi::TwitchRewardsApi(const TwitchAuth& twitchAuth) : twitchAuth(twitchAuth) {
    (void) this->twitchAuth;
}
