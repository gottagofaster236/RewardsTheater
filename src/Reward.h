// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/url.hpp>
#include <cstdint>
#include <optional>
#include <string>

struct Color {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

    constexpr Color() : red(0), green(0), blue(0) {}
    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue) : red(red), green(green), blue(blue) {}
    Color(const std::string& hexColor);
    std::string toHex() const;

    bool operator==(const Color& other) const;
};

struct RewardData {
    std::string title;
    std::string description;
    std::int32_t cost;
    bool isEnabled;
    Color backgroundColor;
    std::optional<std::int64_t> maxRedemptionsPerStream;
    std::optional<std::int64_t> maxRedemptionsPerUserPerStream;
    std::optional<std::int64_t> globalCooldownSeconds;

    bool operator==(const RewardData& other) const;
};

struct Reward : RewardData {
    std::string id;
    boost::urls::url imageUrl;
    bool canManage;

    bool operator==(const Reward& other) const;

    Reward(
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
    );

    Reward(const Reward& reward, const RewardData& newRewardData);
};

struct RewardRedemption {
    Reward reward;
    std::string redemptionId;

    bool operator==(const RewardRedemption& other) const;
};
