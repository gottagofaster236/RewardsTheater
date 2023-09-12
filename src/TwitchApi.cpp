// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchApi.h"

#include "BoostAsio.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace property_tree = boost::property_tree;
namespace http = boost::beast::http;

namespace TwitchApi {

asio::awaitable<Response> request(
    const std::string& host,
    const std::string& target,
    const std::string& accessToken,
    const std::string& clientId,
    asio::io_context& ioContext
) {
    ssl::context sslContext{ssl::context::sslv23};
    sslContext.set_default_verify_paths();
    asio::ip::tcp::resolver resolver{ioContext};
    ssl::stream<asio::ip::tcp::socket> stream{ioContext, sslContext};

    const auto resolveResults = co_await resolver.async_resolve(host, "https", asio::use_awaitable);
    co_await asio::async_connect(
        stream.next_layer(), resolveResults.begin(), resolveResults.end(), asio::use_awaitable
    );
    co_await stream.async_handshake(ssl::stream_base::client, asio::use_awaitable);

    boost::beast::flat_buffer buffer;
    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::authorization, "Bearer " + accessToken);
    req.set("Client-Id", clientId);
    co_await http::async_write(stream, req, asio::use_awaitable);

    http::response<http::dynamic_body> response;
    co_await http::async_read(stream, buffer, response, asio::use_awaitable);
    std::string body = boost::beast::buffers_to_string(response.body().data());
    boost::property_tree::ptree jsonTree;
    std::istringstream is{body};
    boost::property_tree::read_json(is, jsonTree);

    co_return Response{response.result(), jsonTree};
}

boost::asio::awaitable<Response> request(
    const std::string& host,
    const std::string& target,
    TwitchAuth& auth,
    boost::asio::io_context& ioContext
) {
    std::string accessToken = auth.getAccessTokenOrThrow();
    Response response = co_await request(host, target, accessToken, auth.getClientId(), ioContext);
    if (response.status == http::status::unauthorized) {
        auth.logOutAndEmitAuthenticationFailure();
        throw TwitchAuth::UnauthenticatedException();
    }
    co_return response;
}

}  // namespace TwitchApi
