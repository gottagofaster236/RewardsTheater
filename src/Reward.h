// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include <cstdint>
#include <optional>
#include <string>

struct Reward {
    struct Color {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
    };

    const std::string id;
    const std::wstring title;
    const std::int64_t cost;
    const bool isEnabled;
    const Color backgroundColor;
    const std::optional<std::int32_t> maxRedemptionsPerStream;
    const std::optional<std::int32_t> maxRedemptionsPerUserPerStream;
    const std::optional<std::int32_t> globalCooldownSeconds;
};

struct RewardAndSource {
    const Reward reward;
    const std::string obsSourceName;
};
