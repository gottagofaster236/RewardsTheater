// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchRewardsApi.h"

#include <boost/url.hpp>
#include <ranges>
#include <sstream>
#include <string>

#include "Log.h"
#include "TwitchApi.h"

namespace asio = boost::asio;
namespace http = boost::beast::http;

TwitchRewardsApi::TwitchRewardsApi(TwitchAuth& twitchAuth) : twitchAuth(twitchAuth) {
    connect(&twitchAuth, &TwitchAuth::onAuthenticationSuccess, this, &TwitchRewardsApi::updateRewards);
    connect(&twitchAuth, &TwitchAuth::onAuthenticationFailure, this, &TwitchRewardsApi::updateRewards);
}

TwitchRewardsApi::~TwitchRewardsApi() = default;

void TwitchRewardsApi::updateRewards() {
    asio::co_spawn(ioContext, asyncUpdateRewards(), asio::detached);
}

asio::awaitable<void> TwitchRewardsApi::asyncUpdateRewards() {
    try {
        emit onRewardsUpdated(co_await asyncGetRewards());
        co_return;
    } catch (const TwitchAuth::UnauthenticatedException&) {
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Error in asyncUpdateRewards: {}", exception.what());
    }
    emit onRewardsUpdated({});
}

asio::awaitable<std::vector<Reward>> TwitchRewardsApi::asyncGetRewards() {
    std::optional<std::string> userId = twitchAuth.getUserId();
    if (!userId) {
        co_return std::vector<Reward>{};
    }

    boost::urls::url target = boost::urls::parse_origin_form("/helix/channel_points/custom_rewards").value();
    target.set_params({
        {"broadcaster_id", userId.value()},
    });

    TwitchApi::Response response = co_await TwitchApi::request("api.twitch.tv", target.buffer(), twitchAuth, ioContext);
    if (response.status != http::status::ok) {
        co_return std::vector<Reward>{};
    }

    auto rewards = response.json.get_child("data") | std::views::transform([=, this](const auto& rewardElement) {
                       return parseReward(rewardElement.second);
                   });
    co_return std::vector<Reward>(rewards.begin(), rewards.end());
}

Reward TwitchRewardsApi::parseReward(const boost::property_tree::ptree& reward) {
    return Reward{
        reward.get<std::string>("id"),
        reward.get<std::string>("title"),
        reward.get<std::int32_t>("cost"),
        reward.get<bool>("is_enabled"),
        hexColorToColor(reward.get<std::string>("background_color")),
        getOptionalSetting(reward.get_child("max_per_stream_setting"), "max_per_stream"),
        getOptionalSetting(reward.get_child("max_per_user_per_stream_setting"), "max_per_user_per_stream"),
        getOptionalSetting(reward.get_child("global_cooldown_setting"), "global_cooldown_seconds"),
    };
}

Reward::Color TwitchRewardsApi::hexColorToColor(const std::string& hexColor) {
    if (hexColor.empty()) {
        return Reward::Color{0, 0, 0};
    }
    std::string withoutHash = hexColor.substr(1);
    std::istringstream iss(withoutHash);
    iss.exceptions(std::istringstream::failbit | std::istringstream::badbit);
    std::uint32_t color;
    iss >> std::hex >> color;
    return Reward::Color((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
}

std::optional<std::int64_t> TwitchRewardsApi::getOptionalSetting(
    const boost::property_tree::ptree& setting,
    const std::string& key
) {
    if (!setting.get<bool>("is_enabled")) {
        return {};
    }
    return setting.get<std::int64_t>(key);
}
