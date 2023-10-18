// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "Settings.h"

static const char* const PLUGIN_NAME = "RewardsTheater";
static const char* const REWARD_REDEMPTIONS_QUEUE_ENABLED_KEY = "REWARD_REDEMPTIONS_QUEUE_ENABLED_KEY";
static const char* const INTERVAL_BETWEEN_REWARDS_SECONDS_KEY = "INTERVAL_BETWEEN_REWARDS_SECONDS_KEY";
static const char* const TWITCH_ACCESS_TOKEN_KEY = "TWITCH_ACCESS_TOKEN_KEY";

Settings::Settings(config_t* config) : config(config) {}

bool Settings::isRewardRedemptionQueueEnabled() const {
    config_set_default_bool(config, PLUGIN_NAME, REWARD_REDEMPTIONS_QUEUE_ENABLED_KEY, true);
    return config_get_bool(config, PLUGIN_NAME, REWARD_REDEMPTIONS_QUEUE_ENABLED_KEY);
}

void Settings::setRewardRedemptionQueueEnabled(bool rewardRedemptionQueueEnabled) {
    config_set_bool(config, PLUGIN_NAME, REWARD_REDEMPTIONS_QUEUE_ENABLED_KEY, rewardRedemptionQueueEnabled);
}

double Settings::getIntervalBetweenRewardsSeconds() const {
    config_set_default_double(config, PLUGIN_NAME, INTERVAL_BETWEEN_REWARDS_SECONDS_KEY, 0);
    return config_get_double(config, PLUGIN_NAME, INTERVAL_BETWEEN_REWARDS_SECONDS_KEY);
}

void Settings::setIntervalBetweenRewardsSeconds(double intervalBetweenRewardsSeconds) {
    config_set_double(config, PLUGIN_NAME, INTERVAL_BETWEEN_REWARDS_SECONDS_KEY, intervalBetweenRewardsSeconds);
}

std::optional<std::string> Settings::getObsSourceName(const std::string& rewardId) const {
    config_set_default_string(config, PLUGIN_NAME, rewardId.c_str(), "");
    std::string result = config_get_string(config, PLUGIN_NAME, rewardId.c_str());
    if (result.empty()) {
        return {};
    } else {
        return result;
    }
}

void Settings::setObsSourceName(const std::string& rewardId, const std::optional<std::string>& obsSourceName) {
    const char* value;
    if (obsSourceName) {
        value = obsSourceName.value().c_str();
    } else {
        value = "";
    }
    config_set_string(config, PLUGIN_NAME, rewardId.c_str(), value);
}

std::optional<std::string> Settings::getTwitchAccessToken() const {
    config_set_default_string(config, PLUGIN_NAME, TWITCH_ACCESS_TOKEN_KEY, "");
    std::string result = config_get_string(config, PLUGIN_NAME, TWITCH_ACCESS_TOKEN_KEY);
    if (result.empty()) {
        return {};
    } else {
        return result;
    }
}

void Settings::setTwitchAccessToken(const std::optional<std::string>& accessToken) {
    const char* value;
    if (accessToken) {
        value = accessToken.value().c_str();
    } else {
        value = "";
    }
    config_set_string(config, PLUGIN_NAME, TWITCH_ACCESS_TOKEN_KEY, value);
}
