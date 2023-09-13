// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "Settings.h"

static const char* const PLUGIN_NAME = "RewardsName";
static const char* const PUT_REWARDS_IN_QUEUE_KEY = "PUT_REWARDS_IN_QUEUE_KEY";
static const char* const INTERVAL_BETWEEN_REWARDS_SECONDS_KEY = "INTERVAL_BETWEEN_REWARDS_SECONDS_KEY";
static const char* const TWITCH_ACCESS_TOKEN_KEY = "TWITCH_ACCESS_TOKEN_KEY";

Settings::Settings(config_t* config) : config(config) {}

bool Settings::getPutRewardsInQueue() const {
    config_set_default_bool(config, PLUGIN_NAME, PUT_REWARDS_IN_QUEUE_KEY, true);
    return config_get_bool(config, PLUGIN_NAME, PUT_REWARDS_IN_QUEUE_KEY);
}

void Settings::setPutRewardsInQueue(bool putRewardsInQueue) {
    config_set_bool(config, PLUGIN_NAME, PUT_REWARDS_IN_QUEUE_KEY, putRewardsInQueue);
}

std::int32_t Settings::getIntervalBetweenRewardsSeconds() const {
    config_set_default_int(config, PLUGIN_NAME, INTERVAL_BETWEEN_REWARDS_SECONDS_KEY, 0);
    return static_cast<std::int32_t>(config_get_int(config, PLUGIN_NAME, INTERVAL_BETWEEN_REWARDS_SECONDS_KEY));
}

void Settings::setIntervalBetweenRewardsSeconds(std::int32_t intervalBetweenRewardsSeconds) {
    config_set_int(config, PLUGIN_NAME, INTERVAL_BETWEEN_REWARDS_SECONDS_KEY, intervalBetweenRewardsSeconds);
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
