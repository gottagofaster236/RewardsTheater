// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <util/config-file.h>

#include <optional>
#include <string>

class Settings {
public:
    Settings(config_t* config);

    /// Whether to play a reward immediately (possibly simultaneously with other rewards) or put it in a queue.
    bool isRewardRedemptionQueueEnabled() const;
    void setRewardRedemptionQueueEnabled(bool rewardRedemptionQueueEnabled);

    /// How much to wait between two reward redemptions in a queue, isRewardRedemptionQueueEnabled() is true.
    double getIntervalBetweenRewardsSeconds() const;
    void setIntervalBetweenRewardsSeconds(double intervalBetweenRewardsSeconds);

    std::optional<std::string> getObsSourceName(const std::string& rewardId) const;
    void setObsSourceName(const std::string& rewardId, const std::optional<std::string>& obsSourceName);

    std::optional<std::string> getTwitchAccessToken() const;
    void setTwitchAccessToken(const std::optional<std::string>& accessToken);

private:
    config_t* config;
};
