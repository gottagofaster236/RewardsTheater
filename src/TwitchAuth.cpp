// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchAuth.h"

#include <QDesktopServices>
#include <QUrl>

#include "Log.h"
#include "TwitchApi.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/url.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <fmt/core.h>

#include <cstdint>

using namespace std::chrono_literals;
namespace http = boost::beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

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

static const auto TOKEN_VALIDATE_PERIOD = 30min;
static const auto MINIMUM_TOKEN_TIME_LEFT = 48h;

TwitchAuth::TwitchAuth(
    Settings& settings,
    const std::string& clientId,
    const std::set<std::string>& scopes,
    std::uint16_t authServerPort
)
    : settings(settings), clientId(clientId), scopes(scopes), authServerPort(authServerPort) {
    // All of these are not run before the call to startService()
    asio::co_spawn(authIoContext, asyncRunAuthServer(authIoContext), asio::detached);
    asio::co_spawn(authIoContext, asyncValidateTokenPeriodically(authIoContext), asio::detached);
    authenticateWithSavedToken();
}

TwitchAuth::~TwitchAuth() {
    authIoContext.stop();
    authThread.join();
}

void TwitchAuth::startService() {
    if (authThread.joinable()) {
        return;
    }
    authThread = std::thread([=, this]() {
        try {
            authIoContext.run();
        } catch (const boost::system::system_error& error) {
            log(LOG_INFO, "Auth server exception: {}", error.what());
        }
    });
}

std::optional<std::string> TwitchAuth::getAccessToken() const {
    std::lock_guard guard(accessTokenMutex);
    return accessToken;
}

bool TwitchAuth::isAuthenticated() const {
    std::lock_guard guard(accessTokenMutex);
    return accessToken.has_value();
}

const std::string& TwitchAuth::getClientId() const {
    return clientId;
}

static void openUrl(const std::string& url);

void TwitchAuth::authenticate() {
    openUrl(getAuthUrl());
}

void openUrl(const std::string& url) {
    log(LOG_INFO, "Opening url {}", url);
    QDesktopServices::openUrl(QUrl(QString::fromStdString(url), QUrl::TolerantMode));
}

void TwitchAuth::authenticateWithToken([[maybe_unused]] const std::string& token) {
    asio::co_spawn(authIoContext, asyncAuthenticateWithToken(token, authIoContext), asio::detached);
}

void TwitchAuth::logOut() {
    {
        std::lock_guard guard(accessTokenMutex);
        accessToken = {};
    }
    settings.setTwitchAccessToken({});
    emit onUsernameChanged({});
}

void TwitchAuth::authenticateWithSavedToken() {
    std::optional<std::string> savedAccessToken = settings.getTwitchAccessToken();
    if (savedAccessToken) {
        authenticateWithToken(savedAccessToken.value());
    }
}

asio::awaitable<void> TwitchAuth::asyncAuthenticateWithToken(std::string token, asio::io_context& ioContext) {
    std::chrono::seconds expiresIn;
    AuthenticationFailureReason failureReason = AuthenticationFailureReason::AUTH_TOKEN_INVALID;

    try {
        expiresIn = co_await asyncTokenExpiresIn(token, ioContext);
    } catch (const boost::system::system_error& error) {
        log(LOG_ERROR, "Network error in tokenExpiresIn: {}", error.what());
        expiresIn = 0s;
        failureReason = AuthenticationFailureReason::NETWORK_ERROR;
    } catch (const boost::property_tree::ptree_error& error) {
        log(LOG_ERROR, "Error parsing json in tokenExpiresIn: {}", error.what());
        expiresIn = 0s;
        failureReason = AuthenticationFailureReason::NETWORK_ERROR;
    }

    if (expiresIn == 0s) {
        if (failureReason == AuthenticationFailureReason::AUTH_TOKEN_INVALID) {
            logOut();
        }
        emit onAuthenticationFailure(failureReason);
        co_return;
    }

    {
        std::lock_guard guard(accessTokenMutex);
        accessToken = token;
    }
    settings.setTwitchAccessToken(token);
    emit onAuthenticationSuccess();
    emitAccessTokenAboutToExpireIfNeeded(expiresIn);
    std::optional<std::string> username = co_await asyncGetUsername(ioContext);
    emit onUsernameChanged(username);
}

asio::awaitable<std::chrono::seconds> TwitchAuth::asyncTokenExpiresIn(std::string token, asio::io_context& ioContext) {
    TwitchApi::Response validateResponse =
        co_await TwitchApi::request("id.twitch.tv", "/oauth2/validate", token, clientId, ioContext);

    if (validateResponse.status == http::status::unauthorized) {
        co_return 0s;
    }

    // Check that the token has the necessary scopes.
    if (!tokenHasNeededScopes(validateResponse.json)) {
        log(LOG_ERROR, "Error: Token is missing necessary scopes.");
        co_return 0s;
    }
    co_return std::chrono::seconds{validateResponse.json.get<int>("expires_in")};
}

bool TwitchAuth::tokenHasNeededScopes(const boost::property_tree::ptree& oauthValidateResponse) {
    std::set<std::string> tokenScopes;
    for (const auto& scope : oauthValidateResponse.get_child("scopes")) {
        tokenScopes.insert(scope.second.get_value<std::string>());
    }
    return tokenScopes == scopes;
}

std::string TwitchAuth::getAuthUrl() {
    boost::urls::url authUrl = boost::urls::parse_uri_reference("https://id.twitch.tv/oauth2/authorize").value();
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

asio::awaitable<std::optional<std::string>> TwitchAuth::asyncGetUsername(asio::io_context& ioContext) {
    std::optional<std::string> tokenOptional = getAccessToken();
    if (!tokenOptional) {
        co_return std::nullopt;
    }
    const std::string& token = tokenOptional.value();
    TwitchApi::Response response =
        co_await TwitchApi::request("api.twitch.tv", "/helix/users", token, clientId, ioContext);
    if (response.status != http::status::ok) {
        co_return std::nullopt;
    }
    co_return response.json.get<std::string>("data..display_name");
}

asio::awaitable<void> TwitchAuth::asyncRunAuthServer(asio::io_context& ioContext) {
    tcp::acceptor acceptor{ioContext, {tcp::v4(), authServerPort}};
    while (true) {
        try {
            tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(ioContext, asyncProcessRequest(std::move(socket), ioContext), asio::detached);
        } catch (const boost::system::system_error& error) {
            log(LOG_INFO, "Error: {}", error.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

asio::awaitable<void> TwitchAuth::asyncProcessRequest(tcp::socket socket, asio::io_context& ioContext) {
    http::request<http::string_body> request;
    http::response<http::string_body> response;
    boost::beast::flat_buffer buffer;
    co_await http::async_read(socket, buffer, request, asio::use_awaitable);

    if (request.target() == "/") {
        response.set(http::field::content_type, "text/html");
        response.body() = AUTH_PAGE_HTML;
        response.prepare_payload();
    } else if (request.target() == "/accessToken") {
        std::string responseAccessToken = request.body();
        if (responseAccessToken.empty()) {
            response.result(http::status::bad_request);
        } else {
            co_await asyncAuthenticateWithToken(responseAccessToken, ioContext);
        }
    } else {
        response.body() = "RewardsTheater auth server";
        response.prepare_payload();
    }

    co_await http::async_write(socket, response, asio::use_awaitable);
}

asio::awaitable<void> TwitchAuth::asyncValidateTokenPeriodically(asio::io_context& ioContext) {
    for (;; co_await asio::steady_timer(ioContext, TOKEN_VALIDATE_PERIOD).async_wait(asio::use_awaitable)) {
        try {
            std::optional<std::string> tokenOptional = getAccessToken();
            if (!tokenOptional) {
                continue;
            }
            const std::string& token = tokenOptional.value();
            auto expiresIn = co_await asyncTokenExpiresIn(token, ioContext);
            emitAccessTokenAboutToExpireIfNeeded(expiresIn);
        } catch (boost::system::system_error&) {
            continue;
        } catch (boost::property_tree::ptree_error&) {
            continue;
        }
    }
}

void TwitchAuth::emitAccessTokenAboutToExpireIfNeeded(std::chrono::seconds expiresIn) {
    log(LOG_INFO, "Twitch auth token expires in {} seconds", expiresIn.count());
    if (expiresIn == 0s) {
        logOut();
        emit onAuthenticationFailure(AuthenticationFailureReason::AUTH_TOKEN_INVALID);
    } else if (expiresIn < MINIMUM_TOKEN_TIME_LEFT) {
        emit onAccessTokenAboutToExpire(expiresIn);
    }
}
