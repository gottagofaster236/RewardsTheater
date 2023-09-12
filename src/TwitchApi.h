// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <functional>
#include <utility>

#include "TwitchAuth.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/json_parser.hpp>
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
    const std::string& clientId,
    boost::asio::io_context& ioContext
);

boost::asio::awaitable<Response> request(
    const std::string& host,
    const std::string& target,
    TwitchAuth& auth,
    boost::asio::io_context& ioContext
);

template <typename Callable, typename... Params>
auto runBlocking(Callable&& callable, Params&&... params) {
    namespace asio = boost::asio;
    asio::io_context ioContext;
    auto awaitable = std::invoke(std::forward<Callable>(callable), std::forward<Params>(params)..., ioContext);

    auto future = asio::co_spawn(ioContext, std::move(awaitable), asio::use_future);
    ioContext.run();
    return future.get();
}

}  // namespace TwitchApi
