// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/beast.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace TwitchApi {

struct Response {
    boost::beast::http::status status;
    boost::property_tree::ptree json;
};

boost::asio::awaitable<Response> request(
    const std::string& host,
    const std::string& target,
    const std::string& accessToken,
    boost::asio::io_context& ioContext
);

}  // namespace TwitchApi
