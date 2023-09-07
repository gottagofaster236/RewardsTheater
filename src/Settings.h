// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <util/config-file.h>

#include <cstdint>
#include <optional>
#include <string>

class Settings {
public:
    Settings(config_t* config);

    // Whether to play a reward immediately (possibly simultaneously with other rewards) or put it in a queue
    bool getPutRewardsInQueue() const;
    void setPutRewardsInQueue(bool putRewardsInQueue);

    // How much to wait between two rewards in a queue (if putRewardsInQueue is true)
    std::int32_t getIntervalBetweenRewardsSeconds() const;
    void setIntervalBetweenRewardsSeconds(std::int32_t intervalBetweenRewardsSeconds);

    std::string getObsSourceName(const std::string& rewardId) const;
    void setObsSourceName(const std::string& rewardId, const std::string& obsSourceName);

    std::optional<std::string> getTwitchAccessToken() const;
    void setTwitchAccessToken(const std::optional<std::string>& accessToken);

private:
    config_t* config;
};
