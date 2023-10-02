// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/property_tree/json_parser.hpp>
#include <boost/url.hpp>
#include <functional>
#include <utility>

#include "BoostAsio.h"
#include "TwitchAuth.h"

namespace TwitchApi {

struct Response {
    boost::beast::http::status status;
    boost::property_tree::ptree json;
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
