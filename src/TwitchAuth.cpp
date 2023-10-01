// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "TwitchAuth.h"

#include <QDesktopServices>
#include <QUrl>
#include <algorithm>
#include <boost/property_tree/json_parser.hpp>
#include <boost/url.hpp>
#include <cstdint>
#include <format>
#include <ranges>

#include "BoostAsio.h"
#include "Log.h"
#include "TwitchApi.h"

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
    asio::co_spawn(ioContext, asyncRunAuthServer(), asio::detached);
    asio::co_spawn(ioContext, asyncValidateTokenPeriodically(), asio::detached);
    authenticateWithSavedToken();
}

TwitchAuth::~TwitchAuth() = default;

std::optional<std::string> TwitchAuth::getAccessToken() const {
    std::lock_guard guard(accessTokenMutex);
    return accessToken;
}

std::string TwitchAuth::getAccessTokenOrThrow() const {
    std::lock_guard guard(accessTokenMutex);
    if (!accessToken) {
        throw UnauthenticatedException();
    } else {
        return accessToken.value();
    }
}

bool TwitchAuth::isAuthenticated() const {
    std::lock_guard guard(accessTokenMutex);
    return accessToken.has_value();
}

std::optional<std::string> TwitchAuth::getUserId() const {
    std::lock_guard guard(accessTokenMutex);
    return userId;
}

std::string TwitchAuth::getUserIdOrThrow() const {
    std::lock_guard guard(accessTokenMutex);
    if (!userId) {
        throw UnauthenticatedException();
    } else {
        return userId.value();
    }
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

void TwitchAuth::authenticateWithToken(const std::string& token) {
    asio::co_spawn(ioContext, asyncAuthenticateWithToken(token), asio::detached);
}

void TwitchAuth::logOut() {
    {
        std::lock_guard guard(accessTokenMutex);
        accessToken = {};
        userId = {};
    }
    settings.setTwitchAccessToken({});
    emit onUsernameChanged({});
}

void TwitchAuth::logOutAndEmitAuthenticationFailure() {
    logOut();
    emit onAuthenticationFailure(AuthenticationFailureReason::AUTH_TOKEN_INVALID);
}

const char* TwitchAuth::UnauthenticatedException::what() const noexcept {
    return "Unauthenticated from Twitch";
}

void TwitchAuth::authenticateWithSavedToken() {
    std::optional<std::string> savedAccessToken = settings.getTwitchAccessToken();
    if (savedAccessToken) {
        authenticateWithToken(savedAccessToken.value());
    }
}

asio::awaitable<void> TwitchAuth::asyncAuthenticateWithToken(std::string token) {
    ValidateTokenResponse validateTokenResponse;
    AuthenticationFailureReason failureReason = AuthenticationFailureReason::AUTH_TOKEN_INVALID;

    try {
        validateTokenResponse = co_await asyncValidateToken(token);
    } catch (const boost::system::system_error& error) {
        log(LOG_ERROR, "Network error in tokenExpiresIn: {}", error.what());
        failureReason = AuthenticationFailureReason::NETWORK_ERROR;
    } catch (const boost::property_tree::ptree_error& error) {
        log(LOG_ERROR, "Error parsing json in tokenExpiresIn: {}", error.what());
        failureReason = AuthenticationFailureReason::NETWORK_ERROR;
    }

    if (validateTokenResponse.expiresIn == 0s) {
        if (failureReason == AuthenticationFailureReason::AUTH_TOKEN_INVALID) {
            logOut();
        }
        emit onAuthenticationFailure(failureReason);
        co_return;
    }

    {
        std::lock_guard guard(accessTokenMutex);
        accessToken = token;
        userId = validateTokenResponse.userId;
    }
    settings.setTwitchAccessToken(token);
    emit onAuthenticationSuccess();
    emitAccessTokenAboutToExpireIfNeeded(validateTokenResponse.expiresIn);
    std::optional<std::string> username = co_await asyncGetUsername();
    emit onUsernameChanged(username);
}

asio::awaitable<TwitchAuth::ValidateTokenResponse> TwitchAuth::asyncValidateToken(std::string token) {
    TwitchApi::Response validateTokenResponse =
        co_await TwitchApi::request(token, clientId, ioContext, "id.twitch.tv", "/oauth2/validate");

    if (validateTokenResponse.status == http::status::unauthorized) {
        co_return TwitchAuth::ValidateTokenResponse{};
    }

    // Check that the token has the necessary scopes.
    if (!tokenHasNeededScopes(validateTokenResponse.json)) {
        log(LOG_ERROR, "Error: Token is missing necessary scopes.");
        co_return TwitchAuth::ValidateTokenResponse{};
    }
    co_return TwitchAuth::ValidateTokenResponse{
        std::chrono::seconds{validateTokenResponse.json.get<int>("expires_in")},
        validateTokenResponse.json.get<std::string>("user_id"),
    };
}

bool TwitchAuth::tokenHasNeededScopes(const boost::property_tree::ptree& validateTokenResponse) {
    auto tokenScopesView =
        validateTokenResponse.get_child("scopes") | std::views::transform([](const auto& scopeElement) {
            const boost::property_tree::ptree& scope = scopeElement.second;
            return scope.get_value<std::string>();
        });
    std::set tokenScopes(tokenScopesView.begin(), tokenScopesView.end());
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
        {"redirect_uri", std::format("http://localhost:{}", authServerPort)},
        {"scope", scopesString},
        {"response_type", "token"},
        {"force_verify", "true"},
    });
    return authUrl.buffer();
}

asio::awaitable<std::optional<std::string>> TwitchAuth::asyncGetUsername() {
    TwitchApi::Response response = co_await TwitchApi::request(*this, ioContext, "api.twitch.tv", "/helix/users");
    if (response.status != http::status::ok) {
        co_return std::nullopt;
    }
    co_return response.json.get<std::string>("data..display_name");
}

asio::awaitable<void> TwitchAuth::asyncRunAuthServer() {
    tcp::acceptor acceptor{ioContext, {tcp::v4(), authServerPort}};
    while (true) {
        try {
            tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(ioContext, asyncProcessRequest(std::move(socket)), asio::detached);
        } catch (const std::exception& exception) {
            log(LOG_INFO, "Error: {}", exception.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

asio::awaitable<void> TwitchAuth::asyncProcessRequest(tcp::socket socket) {
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
            co_await asyncAuthenticateWithToken(responseAccessToken);
        }
    } else {
        response.body() = "RewardsTheater auth server";
        response.prepare_payload();
    }

    co_await http::async_write(socket, response, asio::use_awaitable);
}

asio::awaitable<void> TwitchAuth::asyncValidateTokenPeriodically() {
    for (;; co_await asio::steady_timer(ioContext, TOKEN_VALIDATE_PERIOD).async_wait(asio::use_awaitable)) {
        try {
            std::optional<std::string> tokenOptional = getAccessToken();
            if (!tokenOptional) {
                continue;
            }
            const std::string& token = tokenOptional.value();
            auto validateTokenResponse = co_await asyncValidateToken(token);
            emitAccessTokenAboutToExpireIfNeeded(validateTokenResponse.expiresIn);
        } catch (const std::exception& exception) {
            log(LOG_ERROR, "Error in asyncValidateTokenPeriodically: {}", exception.what());
            continue;
        }
    }
}

void TwitchAuth::emitAccessTokenAboutToExpireIfNeeded(std::chrono::seconds expiresIn) {
    log(LOG_INFO, "Twitch auth token expires in {} seconds", expiresIn.count());
    if (expiresIn == 0s) {
        logOutAndEmitAuthenticationFailure();
    } else if (expiresIn < MINIMUM_TOKEN_TIME_LEFT) {
        emit onAccessTokenAboutToExpire(expiresIn);
    }
}
