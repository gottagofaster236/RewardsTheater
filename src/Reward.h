// SPDX-License-Identifier: GPL-2.0-or-later
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

    bool operator==(const Color& other) const = default;
};

struct RewardWithoutId {
    std::string title;
    std::int32_t cost;
    boost::urls::url imageUrl;
    bool isEnabled;
    Color backgroundColor;
    std::optional<std::int64_t> maxRedemptionsPerStream;
    std::optional<std::int64_t> maxRedemptionsPerUserPerStream;
    std::optional<std::int64_t> globalCooldownSeconds;

    bool operator==(const RewardWithoutId& other) const = default;
};

struct Reward : RewardWithoutId {
    std::string id;

    bool operator==(const Reward& other) const = default;

    Reward(
        const std::string& id,
        const std::string& title,
        std::int32_t cost,
        const boost::urls::url& imageUrl,
        bool isEnabled,
        const Color& backgroundColor,
        std::optional<std::int64_t> maxRedemptionsPerStream,
        std::optional<std::int64_t> maxRedemptionsPerUserPerStream,
        std::optional<std::int64_t> globalCooldownSeconds
    )
        : RewardWithoutId{title, cost, imageUrl, isEnabled, backgroundColor, maxRedemptionsPerStream, maxRedemptionsPerUserPerStream, globalCooldownSeconds},
          id(id) {}
};
