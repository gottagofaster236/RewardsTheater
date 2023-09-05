// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct Reward {
    struct Color {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;

        bool operator==(const Color& other) const = default;
    };

    std::string id;
    std::wstring title;
    std::int64_t cost;
    bool isEnabled;
    Color backgroundColor;
    std::optional<std::int32_t> maxRedemptionsPerStream;
    std::optional<std::int32_t> maxRedemptionsPerUserPerStream;
    std::optional<std::int32_t> globalCooldownSeconds;

    bool operator==(const Reward& other) const = default;
};

struct RewardAndSource {
    Reward reward;
    std::string obsSourceName;

    bool operator==(const RewardAndSource& other) const = default;
};
