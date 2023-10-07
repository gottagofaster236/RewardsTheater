// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/json.hpp>
#include <boost/system/system_error.hpp>
#include <boost/url.hpp>
#include <exception>
#include <functional>
#include <map>
#include <utility>

#include "BoostAsio.h"
#include "TwitchAuth.h"

namespace HttpUtil {

struct Response {
    boost::beast::http::status status;
    boost::json::value json;
};

using NetworkException = boost::system::system_error;

class InternalServerErrorException : public std::exception {
public:
    InternalServerErrorException(const std::string& message);
    const char* what() const noexcept override;

private:
    std::string message;
};

boost::asio::awaitable<Response> request(
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path,
    const std::map<std::string, std::string> headers = {},
    std::initializer_list<boost::urls::param_view> urlParams = {}
);

boost::asio::awaitable<Response> request(
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path,
    const std::string& accessToken,
    const std::string& clientId,
    std::initializer_list<boost::urls::param_view> urlParams = {}
);

boost::asio::awaitable<Response> request(
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path,
    TwitchAuth& auth,
    std::initializer_list<boost::urls::param_view> urlParams = {}
);

boost::asio::awaitable<std::string> downloadFile(
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& path
);

}  // namespace HttpUtil
