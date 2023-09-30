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
    const std::string& accessToken,
    const std::string& clientId,
    asio::io_context& ioContext,
    const std::string& host,
    const std::string& target,
    std::initializer_list<boost::urls::param_view> urlParams
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

    boost::urls::url targetWithParams = boost::urls::parse_origin_form(target).value();
    targetWithParams.set_params(urlParams);
    http::request<http::empty_body> req{http::verb::get, targetWithParams.buffer(), 11};
    req.set(http::field::host, host);
    req.set(http::field::authorization, "Bearer " + accessToken);
    req.set("Client-Id", clientId);
    co_await http::async_write(stream, req, asio::use_awaitable);

    boost::beast::flat_buffer buffer;
    http::response<http::dynamic_body> response;
    co_await http::async_read(stream, buffer, response, asio::use_awaitable);
    std::string body = boost::beast::buffers_to_string(response.body().data());
    boost::property_tree::ptree jsonTree;
    std::istringstream is{body};
    boost::property_tree::read_json(is, jsonTree);

    co_return Response{response.result(), jsonTree};
}

boost::asio::awaitable<Response> request(
    TwitchAuth& auth,
    boost::asio::io_context& ioContext,
    const std::string& host,
    const std::string& target,
    std::initializer_list<boost::urls::param_view> urlParams
) {
    Response response =
        co_await request(auth.getAccessTokenOrThrow(), auth.getClientId(), ioContext, host, target, urlParams);
    if (response.status == http::status::unauthorized) {
        auth.logOutAndEmitAuthenticationFailure();
        throw TwitchAuth::UnauthenticatedException();
    }
    co_return response;
}

}  // namespace TwitchApi
