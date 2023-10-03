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
namespace json = boost::json;

TwitchRewardsApi::TwitchRewardsApi(TwitchAuth& twitchAuth, asio::io_context& ioContext)
    : twitchAuth(twitchAuth), ioContext(ioContext) {
    connect(&twitchAuth, &TwitchAuth::onUserChanged, this, &TwitchRewardsApi::updateRewards);
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

// https://dev.twitch.tv/docs/api/reference/#get-custom-reward
asio::awaitable<std::vector<Reward>> TwitchRewardsApi::asyncGetRewards() {
    TwitchApi::Response response = co_await TwitchApi::request(
        twitchAuth,
        ioContext,
        "api.twitch.tv",
        "/helix/channel_points/custom_rewards",
        {{"broadcaster_id", twitchAuth.getUserIdOrThrow()}}
    );
    if (response.status != http::status::ok) {
        co_return std::vector<Reward>{};
    }

    auto rewards = response.json.at("data").as_array() | std::views::transform([this](const auto& reward) {
                       return parseReward(reward);
                   });
    co_return std::vector<Reward>(rewards.begin(), rewards.end());
}

Reward TwitchRewardsApi::parseReward(const json::value& reward) {
    return Reward{
        value_to<std::string>(reward.at("id")),
        value_to<std::string>(reward.at("title")),
        value_to<std::int32_t>(reward.at("cost")),
        getImageUrl(reward),
        reward.at("is_enabled").as_bool(),
        hexColorToColor(value_to<std::string>(reward.at("background_color"))),
        getOptionalSetting(reward.at("max_per_stream_setting"), "max_per_stream"),
        getOptionalSetting(reward.at("max_per_user_per_stream_setting"), "max_per_user_per_stream"),
        getOptionalSetting(reward.at("global_cooldown_setting"), "global_cooldown_seconds"),
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

boost::urls::url TwitchRewardsApi::getImageUrl(const json::value& reward) {
    std::string imageUrl;
    if (reward.at("image").is_object()) {
        imageUrl = value_to<std::string>(reward.at("image").at("url_4x"));
    } else {
        imageUrl = value_to<std::string>(reward.at("default_image").at("url_4x"));
    }
    return boost::urls::parse_uri(imageUrl).value();
}

std::optional<std::int64_t> TwitchRewardsApi::getOptionalSetting(const json::value& setting, const std::string& key) {
    if (!setting.at("is_enabled").as_bool()) {
        return {};
    }
    return setting.at(key).as_int64();
}
