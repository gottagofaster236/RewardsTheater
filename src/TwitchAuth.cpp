// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchAuth.h"

#include <QDesktopServices>
#include <QUrl>

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
#include <util/base.h>

#include <cstdint>

using namespace std::chrono_literals;

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
            let errorMessage = 'Please paste this access token into OBS: ' + accessToken;
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

TwitchAuth::TwitchAuth(
    Settings& settings, const std::string& clientId, const std::set<std::string>& scopes, std::uint16_t authServerPort
)
    : settings(settings), clientId(clientId), scopes(scopes), authServerPort(authServerPort) {
    std::optional<std::string> savedAccessToken = settings.getTwitchAccessToken();
    if (savedAccessToken) {
        authenticateWithToken(savedAccessToken.value());
    }
    startAuthServer(authServerPort);
    blog(LOG_INFO, "%s", fmt::format("auth url: {}", getAuthUrl()).c_str());
}

TwitchAuth::~TwitchAuth() {
    authServerIoContext.stop();
    authServerThread.join();
}

std::optional<std::string> TwitchAuth::getAccessToken() {
    std::lock_guard<std::mutex> guard(accessTokenMutex);
    return accessToken;
}

std::chrono::seconds TwitchAuth::tokenExpiresIn(const std::string& token) {
    asio::io_context ioContext;
    auto future = asio::co_spawn(ioContext, asyncTokenExpiresIn(token, ioContext), asio::use_future);
    ioContext.run();
    return future.get();
}

asio::awaitable<std::chrono::seconds> TwitchAuth::asyncTokenExpiresIn(
    const std::string token, asio::io_context& ioContext
) {
    using namespace asio;
    ssl::context sslContext{asio::ssl::context::sslv23};
    sslContext.set_default_verify_paths();
    tcp::resolver resolver{ioContext};
    ssl::stream<tcp::socket> stream{ioContext, sslContext};

    const auto host = "id.twitch.tv";
    const auto resolveResults = co_await resolver.async_resolve(host, "https", use_awaitable);
    co_await async_connect(stream.next_layer(), resolveResults.begin(), resolveResults.end(), use_awaitable);
    co_await stream.async_handshake(ssl::stream_base::client, use_awaitable);

    boost::beast::flat_buffer buffer;
    http::request<http::empty_body> req{http::verb::get, "/oauth2/validate", 11};
    req.set(http::field::host, host);
    req.set(http::field::authorization, "OAuth " + token);
    co_await http::async_write(stream, req, use_awaitable);

    http::response<http::dynamic_body> response;
    co_await http::async_read(stream, buffer, response, use_awaitable);

    if (response.result() != http::status::ok) {
        co_return 0s;
    }
    std::string body = boost::beast::buffers_to_string(response.body().data());
    boost::property_tree::ptree jsonTree;
    std::istringstream is{body};
    boost::property_tree::read_json(is, jsonTree);

    // Check that the token has the necessary scopes.
    if (!tokenHasNeededScopes(jsonTree)) {
        blog(LOG_INFO, "Error: Token is missing necessary scopes.");
        co_return 0s;
    }
    co_return std::chrono::seconds{jsonTree.get<int>("expires_in")};
}

bool TwitchAuth::tokenHasNeededScopes(const boost::property_tree::ptree& oauthValidateResponse) {
    std::set<std::string> tokenScopes;
    for (const auto& scope : oauthValidateResponse.get_child("scopes")) {
        tokenScopes.insert(scope.second.get_value<std::string>());
    }
    return tokenScopes == scopes;
}

static void openUrl(const std::string& url) {
    QDesktopServices::openUrl(QUrl(QString::fromStdString(url), QUrl::TolerantMode));
}

void TwitchAuth::authenticate() {
    openUrl(getAuthUrl());
}

void TwitchAuth::authenticateWithToken([[maybe_unused]] const std::string& token) {
    asio::co_spawn(authServerIoContext, asyncAuthenticateWithToken(token, authServerIoContext), asio::detached);
}

asio::awaitable<void> TwitchAuth::asyncAuthenticateWithToken(std::string token, asio::io_context& ioContext) {
    std::chrono::seconds expiresIn;
    try {
        expiresIn = co_await asyncTokenExpiresIn(token, ioContext);
        blog(LOG_INFO, "%s", fmt::format("The token expires in {} seconds", expiresIn.count()).c_str());
    } catch (boost::system::system_error& error) {
        blog(LOG_INFO, "%s", fmt::format("Error in tokenExpiresIn: {}", error.what()).c_str());
        expiresIn = 0s;
    }
    bool isValidToken = expiresIn != 0s;

    {
        std::lock_guard<std::mutex> guard(accessTokenMutex);
        if (isValidToken) {
            accessToken = token;
        } else {
            accessToken = {};
        }
    }

    if (isValidToken) {
        settings.setTwitchAccessToken(token);
        onAuthenticationSuccess();
    } else {
        settings.setTwitchAccessToken("");
        onAuthenticationFailure();
    }
}

void TwitchAuth::addCallback(Callback& callback) {
    std::lock_guard guard(callbacksMutex);
    callbacks.insert(&callback);
}

// Shouldn't be called inside a callback - return false from it instead.
void TwitchAuth::removeCallback(Callback& callback) {
    std::lock_guard guard(callbacksMutex);
    callbacks.erase(&callback);
}

void TwitchAuth::onAuthenticationSuccess() {
    std::lock_guard guard(callbacksMutex);
    for (auto callback = callbacks.begin(); callback != callbacks.end();) {
        (*(callback++))->onAuthenticationSuccess();
    }
}

void TwitchAuth::onAuthenticationFailure() {
    std::lock_guard guard(callbacksMutex);
    for (auto callback = callbacks.begin(); callback != callbacks.end();) {
        (*(callback++))->onAuthenticationFailure();
    }
}

static asio::awaitable<void> runAuthServer(TwitchAuth& auth, asio::io_context& ioContext, std::uint16_t port);

void TwitchAuth::startAuthServer(std::uint16_t port) {
    authServerThread = std::thread([=, this]() {
        try {
            auto authServer = runAuthServer(*this, authServerIoContext, port);
            asio::co_spawn(authServerIoContext, std::move(authServer), asio::detached);
            authServerIoContext.run();
        } catch (const boost::system::system_error& error) {
            blog(LOG_INFO, "%s", fmt::format("Auth server exception: ", error.what()).c_str());
        }
    });
}

static asio::awaitable<void> processRequest(TwitchAuth& auth, tcp::socket socket, asio::io_context& ioContext);

asio::awaitable<void> runAuthServer(TwitchAuth& auth, asio::io_context& ioContext, std::uint16_t port) {
    tcp::acceptor acceptor{ioContext, {tcp::v4(), port}};
    while (true) {
        try {
            tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(ioContext, processRequest(auth, std::move(socket), ioContext), asio::detached);
        } catch (const boost::system::system_error& error) {
            blog(LOG_INFO, "%s", fmt::format("Error: ", error.what()).c_str());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

asio::awaitable<void> processRequest(TwitchAuth& auth, tcp::socket socket, asio::io_context& ioContext) {
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
            co_await auth.asyncAuthenticateWithToken(request.body(), ioContext);
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
