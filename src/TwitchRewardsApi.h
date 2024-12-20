// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/json.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "BoostAsio.h"
#include "HttpClient.h"
#include "QObjectCallback.h"
#include "Reward.h"
#include "TwitchAuth.h"

class TwitchRewardsApi : public QObject {
    Q_OBJECT

public:
    TwitchRewardsApi(
        TwitchAuth& twitchAuth,
        HttpClient& httpClient,
        Settings& settings,
        boost::asio::io_context& ioContext
    );
    ~TwitchRewardsApi() override;

    // Calls the receiver with the created reward as std::variant<std::exception_ptr, Reward>.
    void createReward(const RewardData& rewardData, QObject* receiver, const char* member);

    // Calls the receiver with the reward passed as std::variant<std::exception_ptr, Reward>.
    void updateReward(const Reward& reward, QObject* receiver, const char* member);

    /// Loads the rewards and emits onRewardsUpdated.
    void reloadRewards();

    /// Calls the receiver with std::exception_ptr.
    void deleteReward(const Reward& reward, QObject* receiver, const char* member);

    /// Calls the receiver with the downloaded bytes as std::string upon success.
    void downloadImage(const Reward& reward, QObject* receiver, const char* member);

    enum class RedemptionStatus {
        CANCELED,
        FULFILLED,
    };
    void updateRedemptionStatus(const RewardRedemption& rewardRedemption, RedemptionStatus status);

    static Reward parsePubsubReward(const boost::json::value& reward);

    class EmptyRewardTitleException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    class SameRewardTitleException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    class RewardCooldownTooLongException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    class NotManageableRewardException : public std::exception {
        const char* what() const noexcept override;
    };

    class NotAffiliateException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    class RewardNotUpdatedException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    class UnexpectedHttpStatusException : public std::exception {
    public:
        UnexpectedHttpStatusException(const boost::json::value& response);
        const char* what() const noexcept override;

    private:
        std::string message;
    };

signals:
    void onRewardsUpdated(const std::variant<std::exception_ptr, std::vector<Reward>>& newRewards);

private:
    boost::asio::awaitable<void> asyncCreateReward(RewardData rewardData, QObjectCallback& callback);
    boost::asio::awaitable<void> asyncUpdateReward(Reward rewardData, QObjectCallback& callback);
    boost::asio::awaitable<void> asyncReloadRewards();
    boost::asio::awaitable<void> asyncDeleteReward(Reward reward, QObjectCallback& callback);
    boost::asio::awaitable<void> asyncDownloadImage(boost::urls::url url, QObjectCallback& callback);
    boost::asio::awaitable<void> asyncUpdateRedemptionStatus(
        RewardRedemption rewardRedemption,
        RedemptionStatus status
    );

    boost::asio::awaitable<Reward> asyncCreateReward(const RewardData& rewardData);
    boost::asio::awaitable<Reward> asyncUpdateReward(const Reward& reward);
    boost::json::value rewardDataToJson(const RewardData& rewardData);
    void checkForSameRewardTitleException(const boost::json::value& response);

    boost::asio::awaitable<std::vector<Reward>> asyncGetRewards();
    boost::asio::awaitable<boost::json::value> asyncGetRewardsRequest(bool onlyManageableRewards);
    static Reward parseReward(const boost::json::value& reward, bool isManageable);
    static boost::urls::url getImageUrl(const boost::json::value& reward);
    static std::optional<std::int64_t> getOptionalSetting(const boost::json::value& setting, const std::string& key);

    boost::asio::awaitable<void> asyncDeleteReward(const Reward& reward);
    boost::asio::awaitable<std::string> asyncDownloadImage(const boost::urls::url& url);

    TwitchAuth& twitchAuth;
    HttpClient& httpClient;
    Settings& settings;
    boost::asio::io_context& ioContext;
};
