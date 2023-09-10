// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <fmt/core.h>
#include <util/base.h>

#include <utility>

template <typename... T>
inline void log(decltype(LOG_INFO) logLevel, fmt::format_string<T...> fmt, T&&... args) {
    blog(logLevel, "[RewardsTheater] %s", fmt::format(fmt, std::forward<T>(args)...).c_str());
}
