// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchRewardsApi.h"

#include <QMetaType>
#include <boost/url.hpp>
#include <format>
#include <ranges>
#include <sstream>
#include <string>
#include <variant>

#include "HttpUtil.h"
#include "Log.h"

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace json = boost::json;

TwitchRewardsApi::TwitchRewardsApi(TwitchAuth& twitchAuth, asio::io_context& ioContext)
    : twitchAuth(twitchAuth), ioContext(ioContext) {}

TwitchRewardsApi::~TwitchRewardsApi() = default;

void TwitchRewardsApi::getRewards(bool onlyManageableRewards, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext,
        asyncGetRewards(onlyManageableRewards, *(new detail::QObjectCallback(this, receiver, member))),
        asio::detached
    );
}

void TwitchRewardsApi::deleteReward(const std::string& rewardId, QObject* receiver, const char* member) {
    (void) rewardId;
    (void) receiver;
    (void) member;
}

void TwitchRewardsApi::downloadImage(const Reward& reward, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext,
        asyncDownloadImage(reward.imageUrl, *(new detail::QObjectCallback(this, receiver, member))),
        asio::detached
    );
}

TwitchRewardsApi::InvalidRewardParametersException::InvalidRewardParametersException(const json::value& response)
    : message(json::serialize(response)) {}

const char* TwitchRewardsApi::InvalidRewardParametersException::what() const noexcept {
    return message.c_str();
}

const char* TwitchRewardsApi::NotAffiliateException::what() const noexcept {
    return "NotAffiliateException";
}

TwitchRewardsApi::UnexpectedHttpStatusException::UnexpectedHttpStatusException(const boost::json::value& response)
    : message(json::serialize(response)) {}

const char* TwitchRewardsApi::UnexpectedHttpStatusException::what() const noexcept {
    return message.c_str();
}

asio::awaitable<void> TwitchRewardsApi::asyncGetRewards(bool onlyManageableRewards, detail::QObjectCallback& callback) {
    std::variant<std::exception_ptr, std::vector<Reward>> result;
    try {
        result = co_await asyncGetRewards(onlyManageableRewards);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncGetRewards: {}", exception.what());
        result = std::current_exception();
    }
    callback("std::variant<std::exception_ptr, std::vector<Reward>>", result);
}

asio::awaitable<void> TwitchRewardsApi::asyncDeleteReward(std::string rewardId, detail::QObjectCallback& callback) {
    std::exception_ptr result;
    try {
        co_await asyncDeleteReward(rewardId);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncDeleteReward: {}", exception.what());
        result = std::current_exception();
    }
    callback("std::exception_ptr", result);
}

asio::awaitable<void> TwitchRewardsApi::asyncDownloadImage(boost::urls::url url, detail::QObjectCallback& callback) {
    try {
        callback("std::string", co_await asyncDownloadImage(url));
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncDownloadImage: {}", exception.what());
    }
}

// https://dev.twitch.tv/docs/api/reference/#get-custom-reward
asio::awaitable<std::vector<Reward>> TwitchRewardsApi::asyncGetRewards(bool onlyManageableRewards) {
    HttpUtil::Response response = co_await HttpUtil::request(
        ioContext,
        "api.twitch.tv",
        "/helix/channel_points/custom_rewards",
        twitchAuth,
        {
            {"broadcaster_id", twitchAuth.getUserIdOrThrow()},
            {"only_manageable_rewards", std::format("{}", onlyManageableRewards)},
        }
    );

    switch (response.status) {
    case http::status::ok: break;
    case http::status::forbidden: throw NotAffiliateException();
    default: throw UnexpectedHttpStatusException(response.json);
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

Color TwitchRewardsApi::hexColorToColor(const std::string& hexColor) {
    if (hexColor.empty()) {
        return Color{0, 0, 0};
    }
    std::string withoutHash = hexColor.substr(1);
    std::istringstream iss(withoutHash);
    iss.exceptions(std::istringstream::failbit | std::istringstream::badbit);
    std::uint32_t color;
    iss >> std::hex >> color;
    return Color((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
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

asio::awaitable<void> TwitchRewardsApi::asyncDeleteReward(const std::string& rewardId) {
    (void) rewardId;
    return asio::awaitable<void>();
}

asio::awaitable<std::string> TwitchRewardsApi::asyncDownloadImage(const boost::urls::url& url) {
    co_return co_await HttpUtil::downloadFile(ioContext, url.host(), url.path());
}

detail::QObjectCallback::QObjectCallback(TwitchRewardsApi* parent, QObject* receiver, const char* member)
    : QObject(parent), receiver(receiver), member(member) {
    QObject::connect(
        receiver, &QObject::destroyed, this, &QObjectCallback::clearReceiver, Qt::ConnectionType::DirectConnection
    );
}

void detail::QObjectCallback::clearReceiver() {
    std::lock_guard guard(receiverMutex);
    receiver = nullptr;
}
