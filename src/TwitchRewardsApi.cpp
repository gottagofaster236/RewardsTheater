// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchRewardsApi.h"

#include "TwitchApi.h"

namespace asio = boost::asio;
namespace http = boost::beast::http;

TwitchRewardsApi::TwitchRewardsApi(TwitchAuth& twitchAuth) : twitchAuth(twitchAuth) {
    (void) this->twitchAuth;
}

std::vector<Reward> TwitchRewardsApi::getRewards() {
    return std::vector<Reward>();
}

asio::awaitable<std::vector<Reward>> TwitchRewardsApi::asyncGetRewards(asio::io_context& context) {
    TwitchApi::Response response =
        co_await TwitchApi::request("api.twitch.tv", "/helix/channel_points/custom_rewards", twitchAuth, context);
    if (response.status != http::status::ok) {
        co_return std::vector<Reward>{};
    }
    co_return std::vector<Reward>();
}
