// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/property_tree/ptree.hpp>
#include <string>
#include <vector>

#include "BoostAsio.h"
#include "Reward.h"
#include "TwitchAuth.h"

class TwitchRewardsApi : public QObject {
    Q_OBJECT

public:
    TwitchRewardsApi(TwitchAuth& twitchAuth, boost::asio::io_context& ioContext);
    ~TwitchRewardsApi();

    void updateRewards();

signals:
    void onRewardsUpdated(const std::vector<Reward>& rewards);

private:
    boost::asio::awaitable<void> asyncUpdateRewards();
    boost::asio::awaitable<std::vector<Reward>> asyncGetRewards();

    Reward parseReward(const boost::property_tree::ptree& reward);
    static Reward::Color hexColorToColor(const std::string& hexColor);
    static std::optional<std::int64_t> getOptionalSetting(
        const boost::property_tree::ptree& setting,
        const std::string& key
    );

    TwitchAuth& twitchAuth;
    boost::asio::io_context& ioContext;
};
