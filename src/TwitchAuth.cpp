// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "TwitchAuth.h"

#include <fmt/core.h>
#include <obs-module.h>

#include <QDesktopServices>
#include <QUrl>
#include <algorithm>
#include <boost/url.hpp>
#include <cstdint>
#include <ranges>

#include "BoostAsio.h"
#include "HttpClient.h"
#include "Log.h"

using namespace std::chrono_literals;
namespace http = boost::beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

static const auto TOKEN_VALIDATE_PERIOD = 30min;
static const auto MINIMUM_TOKEN_TIME_LEFT = 48h;

TwitchAuth::TwitchAuth(
    Settings& settings,
    const std::string& clientId,
    const std::set<std::string>& scopes,
    std::uint16_t authServerPort,
    HttpClient& httpClient,
    asio::io_context& ioContext
)
    : settings(settings), clientId(clientId), scopes(scopes), authServerPort(authServerPort), httpClient(httpClient),
      ioContext(ioContext), randomEngine(std::random_device()()) {}

TwitchAuth::~TwitchAuth() = default;

void TwitchAuth::startService() {
    asio::co_spawn(ioContext, asyncRunAuthServer(), asio::detached);
    asio::co_spawn(ioContext, asyncValidateTokenPeriodically(), asio::detached);
    authenticateWithSavedToken();
}

std::optional<std::string> TwitchAuth::getAccessToken() const {
    std::lock_guard guard(userMutex);
    return accessToken;
}

std::string TwitchAuth::getAccessTokenOrThrow() const {
    std::lock_guard guard(userMutex);
    if (!accessToken) {
        throw UnauthenticatedException();
    } else {
        return accessToken.value();
    }
}

bool TwitchAuth::isAuthenticated() const {
    std::lock_guard guard(userMutex);
    return accessToken.has_value();
}

std::optional<std::string> TwitchAuth::getUserId() const {
    std::lock_guard guard(userMutex);
    return userId;
}

std::string TwitchAuth::getUserIdOrThrow() const {
    std::lock_guard guard(userMutex);
    if (!userId) {
        throw UnauthenticatedException();
    } else {
        return userId.value();
    }
}

std::optional<std::string> TwitchAuth::getUsername() const {
    std::lock_guard guard(userMutex);
    return username;
}

const std::string& TwitchAuth::getClientId() const {
    return clientId;
}

static void openUrl(const std::string& url);

void TwitchAuth::authenticate() {
    openUrl(getDoNotShowOnStreamPageUrl());
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
        std::lock_guard guard(userMutex);
        accessToken = {};
        userId = {};
        username = {};
    }
    settings.setTwitchAccessToken({});
    emit onUserChanged();
    emit onUsernameChanged({});
}

void TwitchAuth::logOutAndEmitAuthenticationFailure() {
    logOut();
    emit onAuthenticationFailure(std::make_exception_ptr(UnauthenticatedException()));
}

const char* TwitchAuth::UnauthenticatedException::what() const noexcept {
    return "UnauthenticatedException";
}

const char* TwitchAuth::EmptyAccessTokenException::what() const noexcept {
    return "EmptyAccessTokenException";
}

void TwitchAuth::authenticateWithSavedToken() {
    std::optional<std::string> savedAccessToken = settings.getTwitchAccessToken();
    if (savedAccessToken) {
        authenticateWithToken(savedAccessToken.value());
    }
}

asio::awaitable<void> TwitchAuth::asyncAuthenticateWithToken(std::string token) {
    ValidateTokenResponse validateTokenResponse;
    std::exception_ptr failureReason;

    try {
        validateTokenResponse = co_await asyncValidateToken(token);
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in tokenExpiresIn: {}", exception.what());
        failureReason = std::current_exception();
    }

    if (validateTokenResponse.expiresIn == 0s) {
        if (failureReason == nullptr) {
            logOut();
            failureReason = std::make_exception_ptr(UnauthenticatedException());
        }
        emit onAuthenticationFailure(failureReason);
        co_return;
    }

    {
        std::lock_guard guard(userMutex);
        accessToken = token;
        userId = validateTokenResponse.userId;
    }
    settings.setTwitchAccessToken(token);
    emit onAuthenticationSuccess();
    emitAccessTokenAboutToExpireIfNeeded(validateTokenResponse.expiresIn);
    emit onUserChanged();
    co_await asyncUpdateUsername();
}

asio::awaitable<TwitchAuth::ValidateTokenResponse> TwitchAuth::asyncValidateToken(std::string token) {
    if (token.empty()) {
        throw EmptyAccessTokenException();
    }

    HttpClient::Response validateTokenResponse =
        co_await httpClient.request("id.twitch.tv", "/oauth2/validate", token, clientId);

    if (validateTokenResponse.status == http::status::unauthorized) {
        co_return TwitchAuth::ValidateTokenResponse{};
    }

    // Check that the token has the necessary scopes.
    if (!tokenHasNeededScopes(validateTokenResponse.json)) {
        log(LOG_ERROR, "Error: Token is missing necessary scopes.");
        co_return TwitchAuth::ValidateTokenResponse{};
    }
    co_return TwitchAuth::ValidateTokenResponse{
        std::chrono::seconds{value_to<int>(validateTokenResponse.json.at("expires_in"))},
        value_to<std::string>(validateTokenResponse.json.at("user_id")),
    };
}

bool TwitchAuth::tokenHasNeededScopes(const json::value& validateTokenResponse) {
    auto tokenScopesView = validateTokenResponse.at("scopes").as_array() | std::views::transform([](const auto& scope) {
                               return value_to<std::string>(scope);
                           });
    std::set tokenScopes(tokenScopesView.begin(), tokenScopesView.end());
    return tokenScopes == scopes;
}

asio::awaitable<void> TwitchAuth::asyncUpdateUsername() {
    std::optional<std::string> newUsername = co_await asyncGetUsername();
    {
        std::lock_guard guard(userMutex);
        if (username == newUsername) {
            co_return;
        }
        username = newUsername;
    }
    emit onUsernameChanged(newUsername);
}

asio::awaitable<std::optional<std::string>> TwitchAuth::asyncGetUsername() {
    try {
        HttpClient::Response response = co_await httpClient.request("api.twitch.tv", "/helix/users", *this);
        if (response.status != http::status::ok) {
            co_return std::nullopt;
        }
        co_return value_to<std::string>(response.json.at("data").at(0).at("display_name"));
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncGetUsername: {}", exception.what());
        co_return std::nullopt;
    }
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
            log(LOG_ERROR, "Exception in asyncValidateTokenPeriodically: {}", exception.what());
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

asio::awaitable<void> TwitchAuth::asyncRunAuthServer() {
    tcp::acceptor acceptor{ioContext, {tcp::v4(), authServerPort}};
    while (true) {
        bool exceptionThrown = false;
        try {
            tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(ioContext, asyncProcessRequest(std::move(socket)), asio::detached);
        } catch (const std::exception& exception) {
            log(LOG_ERROR, "Error: {}", exception.what());
            exceptionThrown = true;
        }

        if (exceptionThrown) {
            // co_await isn't allowed in a catch clause.
            // Wait in order to avoid a busy while loop.
            co_await asio::steady_timer(ioContext, 1s).async_wait(asio::use_awaitable);
        }
    }
}

asio::awaitable<void> TwitchAuth::asyncProcessRequest(tcp::socket socket) {
    http::request<http::string_body> request;
    boost::beast::flat_buffer buffer;
    try {
        co_await http::async_read(socket, buffer, request, asio::use_awaitable);
        http::response<http::string_body> response = getResponse(request);
        co_await http::async_write(socket, response, asio::use_awaitable);
    } catch (std::exception& exception) {
        log(LOG_ERROR, "Error: {}", exception.what());
    }
}

http::response<http::string_body> TwitchAuth::getResponse(const http::request<http::string_body>& request) {
    http::response<http::string_body> response;

    boost::urls::url target = boost::urls::parse_origin_form(request.target()).value();
    std::string path = target.path();
    auto params = target.params();
    auto csrfStateIterator = params.find("state");
    std::string csrfState;
    if (csrfStateIterator != params.end()) {
        csrfState = (*csrfStateIterator).value;
    }

    if (path == "/doNotShowOnStream") {
        response.set(http::field::content_type, "text/html");
        response.body() = getDoNotShowOnStreamPageHtml(csrfState);
        response.prepare_payload();
    } else if (path == "/authRedirect") {
        response.set(http::field::content_type, "text/html");
        response.body() = getAuthRedirectPageHtml();
        response.prepare_payload();
    } else if (path == "/accessToken") {
        std::string responseAccessToken = request.body();
        if (!isValidCsrfState(csrfState) || responseAccessToken.empty()) {
            response.result(http::status::bad_request);
            return response;
        }
        asio::co_spawn(ioContext, asyncAuthenticateWithToken(responseAccessToken), asio::detached);
    } else {
        response.body() = "RewardsTheater auth server";
        response.prepare_payload();
    }
    return response;
}

std::string TwitchAuth::getDoNotShowOnStreamPageUrl() {
    boost::urls::url authUrl = boost::urls::parse_uri("http://localhost/doNotShowOnStream").value();
    authUrl.set_port_number(authServerPort);
    authUrl.set_params({{"state", generateCsrfState()}});
    return authUrl.buffer();
}

std::string TwitchAuth::getDoNotShowOnStreamPageHtml(const std::string& csrfState) {
    static constexpr auto doNotShowOnStreamTemplate = R"(
        <!DOCTYPE html>
        <html>
        <head>
          <meta charset="UTF-8">
          <title>{}</title>  <!-- RewardsTheater -->
          <style>
            body {{
              text-align: center;
              font-size: 24px;
              color: white;
              background-color: #181818;
            }}
            .red-bold {{
              color: red;
              font-weight: bold;
              font-size: 32px;
            }}
            .purple-button {{
              background-color: purple;
              color: white;
              padding: 15px 30px;
              border: none;
              cursor: pointer;
              border-radius: 15px;
              text-decoration: none;
            }}
          </style>
        </head>
        <body>
          <p class="red-bold">{}</p>  <!-- DoNotShowOnStream -->
          <a href="{}" class="purple-button">{}</a>  <!-- getAuthPageUrl(), AuthenticateWithTwitch -->
        </body>
        </html>
    )";
    return fmt::format(
        doNotShowOnStreamTemplate,
        obs_module_text("RewardsTheater"),
        obs_module_text("DoNotShowOnStream"),
        getAuthPageUrl(csrfState),
        obs_module_text("AuthenticateWithTwitch")
    );
}

std::string TwitchAuth::getAuthPageUrl(const std::string& csrfState) {
    boost::urls::url authUrl = boost::urls::parse_uri("https://id.twitch.tv/oauth2/authorize").value();
    std::string scopesString = "";
    for (const std::string& scope : scopes) {
        if (!scopesString.empty()) {
            scopesString += ' ';
        }
        scopesString += scope;
    }

    authUrl.set_params({
        {"client_id", clientId},
        {"force_verify", "true"},
        {"redirect_uri", getAuthRedirectPageUrl()},
        {"response_type", "token"},
        {"scope", scopesString},
        {"state", csrfState},
    });
    return authUrl.buffer();
}

std::string TwitchAuth::getAuthRedirectPageUrl() {
    return fmt::format("http://localhost:{}/authRedirect", authServerPort);
}

std::string TwitchAuth::getAuthRedirectPageHtml() {
    static constexpr auto authRedirectPageTemplate = R"(
        <html>
          <head>
            <meta charset="UTF-8">
            <title>{}</title>  <!-- RewardsTheater -->
            <style>
              body {{
                text-align: center;
                font-size: 24px;
                color: white;
                background-color: #181818;
              }}
            </style>
            <script>
              function onAuthCallback() {{
                let message = document.getElementById('message');
                let fragment = window.location.hash;
                window.location.hash = '';  // Hide the access token in case if the user opens the page again.
                let fragmentParsed = new URLSearchParams(fragment.substring(1));
                let accessToken = fragmentParsed.get('access_token');
                let csrfState = fragmentParsed.get('state');
                if (accessToken == null || csrfState == null) {{
                  message.textContent = '{}';  // TwitchAuthenticationFailedNoAccessToken
                  return;
                }}
                let xhr = new XMLHttpRequest();
                xhr.open('POST', '/accessToken?state=' + csrfState);
                xhr.setRequestHeader('Content-Type', 'text/plain');
                xhr.onload = function() {{
                  if (xhr.status === 200) {{
                    message.textContent = '{}';  // TwitchAuthenticationSuccessful
                  }} else {{
                    message.textContent = '{}';  // TwitchAuthenticationFailedTryAgain
                  }}
                }};
                xhr.onerror = function() {{
                  message.textContent = '{}' + accessToken;  // PleasePasteThisToken
                }};
                xhr.send(accessToken);
              }}
            </script>
          </head>
          <body onload="onAuthCallback() ">
            <h1>{}</h1>  <!-- RewardsTheater -->
            <p id="message">{}</p>  <!-- TwitchAuthenticationInProgress -->
          </body>
        </html>
    )";
    return fmt::format(
        authRedirectPageTemplate,
        obs_module_text("RewardsTheater"),
        obs_module_text("TwitchAuthenticationFailedNoAccessToken"),
        obs_module_text("TwitchAuthenticationSuccessful"),
        obs_module_text("TwitchAuthenticationFailedTryAgain"),
        obs_module_text("PleasePasteThisToken"),
        obs_module_text("RewardsTheater"),
        obs_module_text("TwitchAuthenticationInProgress")
    );
}

std::string TwitchAuth::generateCsrfState() {
    std::lock_guard guard(csrfStatesMutex);

    std::string csrfState;
    auto inserter = std::back_inserter(csrfState);
    std::string allowedChars = "0123456789abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 32; i++) {
        std::ranges::sample(allowedChars, inserter, 1, randomEngine);
    }

    csrfStates.insert(csrfState);

    return csrfState;
}

bool TwitchAuth::isValidCsrfState(const std::string& csrfState) {
    std::lock_guard guard(csrfStatesMutex);
    if (csrfStates.contains(csrfState)) {
        csrfStates.erase(csrfState);
        return true;
    }
    return false;
}
