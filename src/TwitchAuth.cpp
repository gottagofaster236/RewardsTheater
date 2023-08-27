// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchAuth.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <boost/asio/ssl.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/url.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <fmt/core.h>

#include <cstdint>
#include <iostream>  // TODO remove

static const char* const AUTH_PAGE_HTML = R"(
    <html>
      <head>
        <title>Twitch Authentication</title>
        <script>
          function onAuthCallback() {
            let message = document.getElementById('message');
            let fragment = document.location.hash;
            let accessTokenParameter = '#access_token=';
            if (!fragment.startsWith(accessTokenParameter)) {
              message.textContent = 'Twitch authentication failed. No access token provided.';
              return;
            }
            let accessToken = fragment.substr(accessTokenParameter.length).split('&')[0];
            let xhr = new XMLHttpRequest();
            xhr.open('POST', '/accessToken');
            xhr.setRequestHeader('Content-Type', 'text/plain');
            let errorMessage = 'Please paste this token into OBS: ' + accessToken;
            xhr.onload = function() {
              if (xhr.status === 200) {
                message.textContent = 'Twitch authentication was successful! You can return to OBS now.';
              } else {
                message.textContent = errorMessage;
              }
            };
            xhr.onerror = function() {
              message.textContent = errorMessage;
            };
            xhr.send(accessToken);
          }
        </script>
      </head>
      <body onload="onAuthCallback() ">
        <h1>RewardsTheater</h1>
        <p id="message">Twitch authentication in progress...</p>
      </body>
    </html>
)";

TwitchAuth::TwitchAuth(const std::string& clientId, const std::set<std::string>& scopes, std::uint16_t authServerPort)
    : clientId(clientId), scopes(scopes), authServerPort(authServerPort) {
    using namespace boost::urls;

    startAuthServer(authServerPort);
    std::cout << "auth url: " << getAuthUrl() << std::endl;
}

TwitchAuth::~TwitchAuth() {
    ioContext.stop();
    authServerThread.join();
}

std::optional<std::string> TwitchAuth::getAccessToken() {
    std::lock_guard<std::mutex> guard(accessTokenMutex);
    return accessToken;
}

static void openUrl(const std::string& url) {
    (void) url;
    // TODO fix this!!!! (Replace with QT I mean)
}

void TwitchAuth::authenticate() {
    openUrl(getAuthUrl());
}

void TwitchAuth::authenticateWithToken(const std::string& token) {
    {
        std::lock_guard<std::mutex> guard(accessTokenMutex);
        accessToken = token;
    }
    std::cout << "Access Token: " << token << std::endl;  // TODO remove
    try {
        auto expiresIn = tokenExpiresIn(token);
        std::cout << "The token expires in " << expiresIn.count() << " seconds" << std::endl;
    } catch (std::exception& ex) {
        std::cout << "Error: " << ex.what() << std::endl;
    }
}

std::chrono::seconds TwitchAuth::tokenExpiresIn(const std::string& token) {
    using namespace asio;
    ssl::context sslContext{asio::ssl::context::sslv23};
    sslContext.set_default_verify_paths();
    tcp::resolver resolver{ioContext};
    ssl::stream<tcp::socket> stream{ioContext, sslContext};

    const auto host = "id.twitch.tv";
    const auto resolveResults = resolver.resolve(host, "https");
    asio::connect(stream.next_layer(), resolveResults.begin(), resolveResults.end());
    stream.handshake(ssl::stream_base::client);

    boost::beast::flat_buffer buffer;
    http::request<http::empty_body> req{http::verb::get, "/oauth2/validate", 11};
    req.set(http::field::host, host);
    req.set(http::field::authorization, "OAuth " + token);
    http::write(stream, req);

    http::response<http::dynamic_body> response;
    http::read(stream, buffer, response);

    if (response.result() != http::status::ok) {
        return std::chrono::seconds{0};
    }
    std::string body = boost::beast::buffers_to_string(response.body().data());
    boost::property_tree::ptree jsonTree;
    std::istringstream is{body};
    boost::property_tree::read_json(is, jsonTree);

    // Check that the token has the necessary scopes.
    if (!tokenHasNeededScopes(jsonTree)) {
        std::cout << "Error: Token is missing necessary scopes." << std::endl;
        return std::chrono::seconds{0};
    }
    return std::chrono::seconds{jsonTree.get<int>("expires_in")};
}

bool TwitchAuth::tokenHasNeededScopes(const boost::property_tree::ptree& oauthValidateResponse) {
    std::set<std::string> tokenScopes;
    for (const auto& scope : oauthValidateResponse.get_child("scopes")) {
        tokenScopes.insert(scope.second.get_value<std::string>());
    }
    return tokenScopes == scopes;
}

asio::awaitable<void> runAuthServer(TwitchAuth* auth, asio::io_context& ioContext, std::uint16_t port);
asio::awaitable<void> processRequest(TwitchAuth* auth, tcp::socket socket);

void TwitchAuth::startAuthServer(std::uint16_t port) {
    authServerThread = std::thread([=, this]() {
        try {
            auto authServer = runAuthServer(this, ioContext, port);
            asio::co_spawn(ioContext, std::move(authServer), asio::detached);
            ioContext.run();
        } catch (const boost::system::system_error& error) {
            std::cout << "Auth server exception: " << error.what() << std::endl;
        }
    });
}

asio::awaitable<void> runAuthServer(TwitchAuth* auth, asio::io_context& ioContext, std::uint16_t port) {
    tcp::acceptor acceptor{ioContext, {tcp::v4(), port}};
    while (true) {
        try {
            tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(ioContext, processRequest(auth, std::move(socket)), asio::detached);
        } catch (const boost::system::system_error& error) {
            std::cout << "Error: " << error.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

asio::awaitable<void> processRequest(TwitchAuth* auth, tcp::socket socket) {
    http::request<http::string_body> request;
    http::response<http::string_body> response;
    boost::beast::flat_buffer buffer;
    co_await http::async_read(socket, buffer, request, asio::use_awaitable);

    if (request.target() == "/") {
        response.set(http::field::content_type, "text/html");
        response.body() = AUTH_PAGE_HTML;
        response.prepare_payload();
    } else if (request.target() == "/accessToken") {
        std::string accessToken = request.body();
        if (accessToken.empty()) {
            response.result(http::status::bad_request);
        } else {
            auth->authenticateWithToken(request.body());
        }
    } else {
        response.body() = "RewardsTheater auth server";
        response.prepare_payload();
    }

    co_await http::async_write(socket, response, asio::use_awaitable);
}

std::string TwitchAuth::getAuthUrl() {
    using namespace boost::urls;

    url authUrl = parse_uri_reference("https://id.twitch.tv/oauth2/authorize").value();
    std::string scopesString = "";
    for (const std::string& scope : scopes) {
        if (!scopesString.empty()) {
            scopesString += ' ';
        }
        scopesString += scope;
    }

    authUrl.set_params({
        {"client_id", clientId},
        {"redirect_uri", fmt::format("http://localhost:{}", authServerPort)},
        {"scope", scopesString},
        {"response_type", "token"},
        {"force_verify", "true"},
    });
    return static_cast<boost::core::basic_string_view<char>>(authUrl);
}
