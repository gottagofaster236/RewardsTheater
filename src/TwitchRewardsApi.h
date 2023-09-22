// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <string>
#include <vector>

#include "BoostAsio.h"
#include "Reward.h"
#include "TwitchAuth.h"

class TwitchRewardsApi {
public:
    TwitchRewardsApi(TwitchAuth& twitchAuth);
    std::vector<Reward> getRewards();

private:
    boost::asio::awaitable<std::vector<Reward>> asyncGetRewards(boost::asio::io_context& context);

    TwitchAuth& twitchAuth;
};
