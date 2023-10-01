// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <util/base.h>

#include <format>
#include <utility>

template <typename... T>
inline void log(decltype(LOG_DEBUG) logLevel, std::format_string<T...> fmt, T&&... args) {
    blog(logLevel, "[RewardsTheater] %s", std::format(fmt, std::forward<T>(args)...).c_str());
}
