// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif
