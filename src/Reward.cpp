// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "Reward.h"

#include <iomanip>
#include <sstream>

Color::Color(const std::string& hexColor) {
    if (hexColor.empty()) {
        red = green = blue = 0;
        return;
    }
    std::string withoutHash = hexColor.substr(1);
    std::istringstream iss(withoutHash);
    iss.exceptions(std::istringstream::failbit | std::istringstream::badbit);
    std::uint32_t color;
    iss >> std::hex >> color;

    red = (color >> 16) & 0xff;
    green = (color >> 8) & 0xff;
    blue = color & 0xff;
}

std::string Color::toHex() const {
    std::stringstream stream;
    stream << "#" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(red) << std::setw(2)
           << static_cast<int>(green) << std::setw(2) << static_cast<int>(blue);
    return stream.str();
}

bool Color::operator==(const Color& other) const = default;

bool RewardData::operator==(const RewardData& other) const = default;

bool Reward::operator==(const Reward& other) const = default;

Reward::Reward(
    const std::string& id,
    const std::string& title,
    const std::string& description,
    std::int32_t cost,
    const boost::urls::url& imageUrl,
    bool isEnabled,
    const Color& backgroundColor,
    std::optional<std::int64_t> maxRedemptionsPerStream,
    std::optional<std::int64_t> maxRedemptionsPerUserPerStream,
    std::optional<std::int64_t> globalCooldownSeconds,
    bool canManage
)
    : RewardData{title, description, cost, isEnabled, backgroundColor, maxRedemptionsPerStream, maxRedemptionsPerUserPerStream, globalCooldownSeconds},
      id(id), imageUrl(imageUrl), canManage(canManage) {}

Reward::Reward(const Reward& reward, const RewardData& newRewardData)
    : RewardData(newRewardData), id(reward.id), imageUrl(reward.imageUrl), canManage(reward.canManage) {}

bool RewardRedemption::operator==(const RewardRedemption& other) const = default;
