// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/json.hpp>
#include <boost/system/system_error.hpp>
#include <boost/url.hpp>
#include <exception>
#include <map>

#include "BoostAsio.h"

class TwitchAuth;

class HttpClient {
public:
    HttpClient(boost::asio::io_context& ioContext);
    ~HttpClient();

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
        const std::string& host,
        const std::string& path,
        const std::map<std::string, std::string>& headers = {},
        std::initializer_list<boost::urls::param_view> urlParams = {},
        boost::beast::http::verb method = boost::beast::http::verb::get,
        boost::json::value body = {}
    );

    boost::asio::awaitable<Response> request(
        const std::string& host,
        const std::string& path,
        const std::string& accessToken,
        const std::string& clientId,
        std::initializer_list<boost::urls::param_view> urlParams = {},
        boost::beast::http::verb method = boost::beast::http::verb::get,
        boost::json::value body = {}
    );

    boost::asio::awaitable<Response> request(
        const std::string& host,
        const std::string& path,
        TwitchAuth& auth,
        std::initializer_list<boost::urls::param_view> urlParams = {},
        boost::beast::http::verb method = boost::beast::http::verb::get,
        boost::json::value body = {}
    );

    boost::asio::awaitable<std::string> downloadFile(const std::string& host, const std::string& path);

private:
    boost::asio::awaitable<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> resolveHost(const std::string& host);
    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::dynamic_body>> getResponse(
        const boost::beast::http::request<boost::beast::http::string_body>& request,
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& stream
    );

    boost::asio::io_context& ioContext;
};
