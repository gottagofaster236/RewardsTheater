// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "HttpClient.h"

#include "BoostAsio.h"
#include "TwitchAuth.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace http = boost::beast::http;

HttpClient::HttpClient(boost::asio::io_context& ioContext) : ioContext(ioContext) {}

HttpClient::~HttpClient() = default;

HttpClient::InternalServerErrorException::InternalServerErrorException(const std::string& message) : message(message) {}

const char* HttpClient::InternalServerErrorException::what() const noexcept {
    return message.c_str();
}

asio::awaitable<HttpClient::Response> HttpClient::request(
    const std::string& host,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    std::initializer_list<boost::urls::param_view> urlParams
) {
    ssl::stream<asio::ip::tcp::socket> stream = co_await resolveHost(host);
    boost::urls::url pathWithParams = boost::urls::parse_origin_form(path).value();
    pathWithParams.set_params(urlParams);
    http::request<http::empty_body> request{http::verb::get, pathWithParams.buffer(), 11};
    request.set(http::field::host, host);
    for (const auto& [headerName, headerValue] : headers) {
        request.set(headerName, headerValue);
    }

    http::response<http::dynamic_body> response = co_await getResponse(request, stream);
    std::string body = boost::beast::buffers_to_string(response.body().data());
    if (response.result() == http::status::internal_server_error) {
        throw HttpClient::InternalServerErrorException(body);
    }
    co_return HttpClient::Response{response.result(), boost::json::parse(body)};
}

asio::awaitable<HttpClient::Response> HttpClient::request(
    const std::string& host,
    const std::string& path,
    const std::string& accessToken,
    const std::string& clientId,
    std::initializer_list<boost::urls::param_view> urlParams
) {
    co_return co_await request(
        host, path, {{"Authorization", "Bearer " + accessToken}, {"Client-Id", clientId}}, urlParams
    );
}

asio::awaitable<HttpClient::Response> HttpClient::request(
    const std::string& host,
    const std::string& path,
    TwitchAuth& auth,
    std::initializer_list<boost::urls::param_view> urlParams
) {
    HttpClient::Response response =
        co_await request(host, path, auth.getAccessTokenOrThrow(), auth.getClientId(), urlParams);
    if (response.status == http::status::unauthorized) {
        auth.logOutAndEmitAuthenticationFailure();
        throw TwitchAuth::UnauthenticatedException();
    }
    co_return response;
}

asio::awaitable<std::string> HttpClient::downloadFile(const std::string& host, const std::string& path) {
    ssl::stream<asio::ip::tcp::socket> stream = co_await resolveHost(host);
    http::request<http::empty_body> request{http::verb::get, path, 11};
    request.set(http::field::host, host);

    http::response<http::dynamic_body> response = co_await getResponse(request, stream);
    if (response.result() != http::status::ok) {
        throw TwitchAuth::UnauthenticatedException();
    }
    co_return boost::beast::buffers_to_string(response.body().data());
}

asio::awaitable<ssl::stream<asio::ip::tcp::socket>> HttpClient::resolveHost(const std::string& host) {
    ssl::context sslContext{ssl::context::tlsv12};
    sslContext.set_default_verify_paths();
    asio::ip::tcp::resolver resolver{ioContext};
    ssl::stream<asio::ip::tcp::socket> stream{ioContext, sslContext};

    const auto resolveResults = co_await resolver.async_resolve(host, "https", asio::use_awaitable);
    co_await asio::async_connect(
        stream.next_layer(), resolveResults.begin(), resolveResults.end(), asio::use_awaitable
    );
    co_await stream.async_handshake(ssl::stream_base::client, asio::use_awaitable);
    co_return stream;
}

asio::awaitable<http::response<http::dynamic_body>> HttpClient::getResponse(
    const http::request<http::empty_body>& request,
    ssl::stream<asio::ip::tcp::socket>& stream
) {
    co_await http::async_write(stream, request, asio::use_awaitable);
    boost::beast::flat_buffer buffer;
    http::response<http::dynamic_body> response;
    co_await http::async_read(stream, buffer, response, asio::use_awaitable);
    co_return response;
}
