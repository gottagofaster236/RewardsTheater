// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/json.hpp>
#include <boost/url.hpp>
#include <functional>
#include <utility>

#include "BoostAsio.h"
#include "TwitchAuth.h"

namespace HttpUtil {

struct Response {
    boost::beast::http::status status;
    boost::json::value json;
};

boost::asio::awaitable<Response> request(
    const std::string& accessToken,
    const std::string& clientId,
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path,
    std::initializer_list<boost::urls::param_view> urlParams = {}
);

boost::asio::awaitable<Response> request(
    TwitchAuth& auth,
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path,
    std::initializer_list<boost::urls::param_view> urlParams = {}
);

boost::asio::awaitable<std::string> downloadFile(
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path
);

}  // namespace HttpUtil
