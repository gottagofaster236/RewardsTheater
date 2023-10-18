// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "TwitchRewardsApi.h"

#include <QMetaType>
#include <boost/url.hpp>
#include <format>
#include <iomanip>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <variant>

#include "HttpClient.h"
#include "Log.h"

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace json = boost::json;

TwitchRewardsApi::TwitchRewardsApi(TwitchAuth& twitchAuth, HttpClient& httpClient, asio::io_context& ioContext)
    : twitchAuth(twitchAuth), httpClient(httpClient), ioContext(ioContext) {
    connect(&twitchAuth, &TwitchAuth::onUserChanged, this, &TwitchRewardsApi::reloadRewards);
}

TwitchRewardsApi::~TwitchRewardsApi() = default;

void TwitchRewardsApi::createReward(const RewardData& rewardData, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext, asyncCreateReward(rewardData, *(new QObjectCallback(this, receiver, member))), asio::detached
    );
}

void TwitchRewardsApi::updateReward(const Reward& reward, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext, asyncUpdateReward(reward, *(new QObjectCallback(this, receiver, member))), asio::detached
    );
}

void TwitchRewardsApi::reloadRewards() {
    asio::co_spawn(ioContext, asyncReloadRewards(), asio::detached);
}

void TwitchRewardsApi::deleteReward(const Reward& reward, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext, asyncDeleteReward(reward, *(new QObjectCallback(this, receiver, member))), asio::detached
    );
}

void TwitchRewardsApi::downloadImage(const Reward& reward, QObject* receiver, const char* member) {
    asio::co_spawn(
        ioContext, asyncDownloadImage(reward.imageUrl, *(new QObjectCallback(this, receiver, member))), asio::detached
    );
}

void TwitchRewardsApi::updateRedemptionStatus(const RewardRedemption& rewardRedemption, RedemptionStatus status) {
    asio::co_spawn(ioContext, asyncUpdateRedemptionStatus(rewardRedemption, status), asio::detached);
}

Reward TwitchRewardsApi::parsePubsubReward(const json::value& reward) {
    // The format of the reward for PubSub events differs slightly from the Helix one, hence we need another function.
    return Reward{
        value_to<std::string>(reward.at("id")),
        value_to<std::string>(reward.at("title")),
        value_to<std::int32_t>(reward.at("cost")),
        getImageUrl(reward),
        reward.at("is_enabled").as_bool(),
        value_to<std::string>(reward.at("background_color")),
        getOptionalSetting(reward.at("max_per_stream"), "max_per_stream"),
        getOptionalSetting(reward.at("max_per_user_per_stream"), "max_per_user_per_stream"),
        getOptionalSetting(reward.at("global_cooldown"), "global_cooldown_seconds"),
        false,
    };
}

const char* TwitchRewardsApi::EmptyRewardTitleException::what() const noexcept {
    return "EmptyRewardTitleException";
}

const char* TwitchRewardsApi::SameRewardTitleException::what() const noexcept {
    return "SameRewardTitleException";
}

const char* TwitchRewardsApi::RewardCooldownTooLongException::what() const noexcept {
    return "RewardCooldownTooLongException";
}

const char* TwitchRewardsApi::NotManageableRewardException::what() const noexcept {
    return "NotManageableRewardException";
}

const char* TwitchRewardsApi::NotAffiliateException::what() const noexcept {
    return "NotAffiliateException";
}

TwitchRewardsApi::UnexpectedHttpStatusException::UnexpectedHttpStatusException(const boost::json::value& response)
    : message(serialize(response)) {}

const char* TwitchRewardsApi::UnexpectedHttpStatusException::what() const noexcept {
    return message.c_str();
}

asio::awaitable<void> TwitchRewardsApi::asyncCreateReward(RewardData rewardData, QObjectCallback& callback) {
    std::variant<std::exception_ptr, Reward> reward;
    try {
        reward = co_await asyncCreateReward(rewardData);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncCreateReward: {}", exception.what());
        reward = std::current_exception();
    }
    callback("std::variant<std::exception_ptr, Reward>", reward);
}

asio::awaitable<void> TwitchRewardsApi::asyncUpdateReward(Reward reward, QObjectCallback& callback) {
    std::variant<std::exception_ptr, Reward> result;
    try {
        result = co_await asyncUpdateReward(reward);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncUpdateReward: {}", exception.what());
        result = std::current_exception();
    }
    callback("std::variant<std::exception_ptr, Reward>", result);
}

asio::awaitable<void> TwitchRewardsApi::asyncReloadRewards() {
    std::variant<std::exception_ptr, std::vector<Reward>> rewards;
    try {
        rewards = co_await asyncGetRewards();
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncReloadRewards: {}", exception.what());
        rewards = std::current_exception();
    }
    emit onRewardsUpdated(rewards);
}

asio::awaitable<void> TwitchRewardsApi::asyncDeleteReward(Reward reward, QObjectCallback& callback) {
    std::exception_ptr result;
    try {
        co_await asyncDeleteReward(reward);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncDeleteReward: {}", exception.what());
        result = std::current_exception();
    }
    callback("std::exception_ptr", result);
}

asio::awaitable<void> TwitchRewardsApi::asyncDownloadImage(boost::urls::url url, QObjectCallback& callback) {
    try {
        callback("std::string", co_await asyncDownloadImage(url));
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncDownloadImage: {}", exception.what());
    }
}

boost::asio::awaitable<void> TwitchRewardsApi::asyncUpdateRedemptionStatus(
    RewardRedemption rewardRedemption,
    RedemptionStatus status
) {
    try {
        std::string statusString;
        switch (status) {
        case RedemptionStatus::FULFILLED: statusString = "FULFILLED"; break;
        case RedemptionStatus::CANCELED: statusString = "CANCELED"; break;
        }

        HttpClient::Response response = co_await httpClient.request(
            "api.twitch.tv",
            "/helix/channel_points/custom_rewards/redemptions",
            twitchAuth,
            {
                {"id", rewardRedemption.redemptionId},
                {"broadcaster_id", twitchAuth.getUserIdOrThrow()},
                {"reward_id", rewardRedemption.reward.id},
            },
            http::verb::patch,
            {{"status", statusString}}
        );
        if (response.status != http::status::ok) {
            throw UnexpectedHttpStatusException(response.json);
        }
        log(LOG_DEBUG, "Successfully updated redemption status to {}", statusString);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncUpdateRedemptionStatus: {}", exception.what());
    }
}

// https://dev.twitch.tv/docs/api/reference/#create-custom-rewards
asio::awaitable<Reward> TwitchRewardsApi::asyncCreateReward(const RewardData& rewardData) {
    HttpClient::Response response = co_await httpClient.request(
        "api.twitch.tv",
        "/helix/channel_points/custom_rewards",
        twitchAuth,
        {{"broadcaster_id", twitchAuth.getUserIdOrThrow()}},
        http::verb::post,
        rewardDataToJson(rewardData)
    );

    checkForSameRewardTitleException(response.json);
    switch (response.status) {
    case http::status::ok: break;
    case http::status::forbidden: throw NotAffiliateException();
    default: throw UnexpectedHttpStatusException(response.json);
    }

    co_return parseReward(response.json.at("data").at(0), true);
}

boost::asio::awaitable<Reward> TwitchRewardsApi::asyncUpdateReward(const Reward& reward) {
    if (!reward.canManage) {
        throw NotManageableRewardException();
    }

    HttpClient::Response response = co_await httpClient.request(
        "api.twitch.tv",
        "/helix/channel_points/custom_rewards",
        twitchAuth,
        {{"broadcaster_id", twitchAuth.getUserIdOrThrow()}, {"id", reward.id}},
        http::verb::patch,
        rewardDataToJson(reward)
    );

    checkForSameRewardTitleException(response.json);
    if (response.status != http::status::ok) {
        throw UnexpectedHttpStatusException(response.json);
    }
    co_return reward;
}

json::value TwitchRewardsApi::rewardDataToJson(const RewardData& rewardData) {
    if (rewardData.title.empty()) {
        throw EmptyRewardTitleException();
    }

    const int SEVEN_DAYS_SECONDS = 604800;
    if (rewardData.globalCooldownSeconds > SEVEN_DAYS_SECONDS) {
        throw RewardCooldownTooLongException();
    }

    return {
        {"title", rewardData.title},
        {"cost", rewardData.cost},
        {"is_enabled", rewardData.isEnabled},
        {"background_color", rewardData.backgroundColor.toHex()},
        {"is_max_per_stream_enabled", rewardData.maxRedemptionsPerStream.has_value()},
        {"max_per_stream", rewardData.maxRedemptionsPerStream.value_or(1)},
        {"is_max_per_user_per_stream_enabled", rewardData.maxRedemptionsPerUserPerStream.has_value()},
        {"max_per_user_per_stream", rewardData.maxRedemptionsPerUserPerStream.value_or(1)},
        {"is_global_cooldown_enabled", rewardData.globalCooldownSeconds.has_value()},
        {"global_cooldown_seconds", rewardData.globalCooldownSeconds.value_or(1)},
        {"should_redemptions_skip_request_queue", false},
    };
}

void TwitchRewardsApi::checkForSameRewardTitleException(const boost::json::value& response) {
    bool isSameRewardException;
    try {
        std::string errorMessage = value_to<std::string>(response.at("message"));
        isSameRewardException = errorMessage == "CREATE_CUSTOM_REWARD_DUPLICATE_REWARD" ||
                                errorMessage == "UPDATE_CUSTOM_REWARD_DUPLICATE_REWARD";
    } catch (const std::exception&) {
        // Ignore the JSON exception when a key is missing.
        return;
    }

    if (isSameRewardException) {
        throw SameRewardTitleException();
    }
}

// https://dev.twitch.tv/docs/api/reference/#get-custom-reward
asio::awaitable<std::vector<Reward>> TwitchRewardsApi::asyncGetRewards() {
    json::value manageableRewardsJson = co_await asyncGetRewardsRequest(true);
    auto manageableRewardIdsView =
        manageableRewardsJson.at("data").as_array() | std::views::transform([this](const auto& reward) {
            return value_to<std::string>(reward.at("id"));
        });
    std::set<std::string> manageableRewardIds(manageableRewardIdsView.begin(), manageableRewardIdsView.end());

    auto allRewardsJson = co_await asyncGetRewardsRequest(false);
    auto rewards =
        allRewardsJson.at("data").as_array() | std::views::transform([this, &manageableRewardIds](const auto& reward) {
            std::string id = value_to<std::string>(reward.at("id"));
            bool isManageable = manageableRewardIds.contains(id);
            return parseReward(reward, isManageable);
        });
    co_return std::vector<Reward>(rewards.begin(), rewards.end());
}

asio::awaitable<json::value> TwitchRewardsApi::asyncGetRewardsRequest(bool onlyManageableRewards) {
    HttpClient::Response response = co_await httpClient.request(
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

    co_return response.json;
}

Reward TwitchRewardsApi::parseReward(const json::value& reward, bool isManageable) {
    return Reward{
        value_to<std::string>(reward.at("id")),
        value_to<std::string>(reward.at("title")),
        value_to<std::int32_t>(reward.at("cost")),
        getImageUrl(reward),
        reward.at("is_enabled").as_bool(),
        value_to<std::string>(reward.at("background_color")),
        getOptionalSetting(reward.at("max_per_stream_setting"), "max_per_stream"),
        getOptionalSetting(reward.at("max_per_user_per_stream_setting"), "max_per_user_per_stream"),
        getOptionalSetting(reward.at("global_cooldown_setting"), "global_cooldown_seconds"),
        isManageable,
    };
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

asio::awaitable<void> TwitchRewardsApi::asyncDeleteReward(const Reward& reward) {
    if (!reward.canManage) {
        throw NotManageableRewardException();
    }
    HttpClient::Response response = co_await httpClient.request(
        "api.twitch.tv",
        "/helix/channel_points/custom_rewards",
        twitchAuth,
        {{"broadcaster_id", twitchAuth.getUserIdOrThrow()}, {"id", reward.id}},
        http::verb::delete_
    );

    if (response.status != http::status::no_content) {
        throw UnexpectedHttpStatusException(response.json);
    }
}

asio::awaitable<std::string> TwitchRewardsApi::asyncDownloadImage(const boost::urls::url& url) {
    co_return co_await httpClient.downloadFile(url.host(), url.path());
}
