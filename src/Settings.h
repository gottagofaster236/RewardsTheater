// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <util/config-file.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

class Settings {
public:
    Settings(config_t* config);

    /// Whether to play a reward immediately (possibly simultaneously with other rewards) or put it in a queue.
    bool isRewardRedemptionQueueEnabled() const;
    void setRewardRedemptionQueueEnabled(bool rewardRedemptionQueueEnabled);

    /// How much to wait between two reward redemptions in a queue, isRewardRedemptionQueueEnabled() is true.
    double getIntervalBetweenRewardsSeconds() const;
    void setIntervalBetweenRewardsSeconds(double intervalBetweenRewardsSeconds);

    std::optional<std::string> getTwitchAccessToken() const;
    void setTwitchAccessToken(const std::optional<std::string>& accessToken);

    std::optional<std::string> getObsSourceName(const std::string& rewardId) const;
    void setObsSourceName(const std::string& rewardId, const std::optional<std::string>& obsSourceName);

    bool isRandomPositionEnabled(const std::string& rewardId) const;
    void setRandomPositionEnabled(const std::string& rewardId, bool randomPositionEnabled);

    std::optional<std::pair<std::uint32_t, std::uint32_t>> getLastVideoSize(const std::string& rewardId) const;
    void setLastVideoSize(
        const std::string& rewardId,
        const std::optional<std::pair<std::uint32_t, std::uint32_t>>& lastVideoSize
    );

    void deleteReward(const std::string& rewardId);

private:
    config_t* config;
};
