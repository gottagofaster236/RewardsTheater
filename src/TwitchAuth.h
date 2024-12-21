// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QObject>
#include <boost/json.hpp>
#include <chrono>
#include <exception>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>

#include "BoostAsio.h"
#include "HttpClient.h"
#include "Settings.h"

/// A class for Twitch authentication using the Implicit grant flow.
/// Read more here: https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#implicit-grant-flow
class TwitchAuth : public QObject {
    Q_OBJECT

public:
    TwitchAuth(
        Settings& settings,
        const std::string& clientId,
        const std::set<std::string>& scopes,
        std::uint16_t authServerPort,
        HttpClient& httpClient,
        boost::asio::io_context& ioContext
    );
    ~TwitchAuth() override;
    void startService();

    std::optional<std::string> getAccessToken() const;
    std::string getAccessTokenOrThrow() const;
    bool isAuthenticated() const;
    std::optional<std::string> getUserId() const;
    std::string getUserIdOrThrow() const;
    std::optional<std::string> getUsername() const;
    const std::string& getClientId() const;

    void authenticate();
    void authenticateWithToken(const std::string& token);
    void logOut();
    void logOutAndEmitAuthenticationFailure();

    class UnauthenticatedException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    class EmptyAccessTokenException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

signals:
    void onAuthenticationSuccess();
    void onAuthenticationFailure(std::exception_ptr reason);
    void onUserChanged();
    void onAccessTokenAboutToExpire(const std::chrono::seconds& expiresIn);
    void onUsernameChanged(const std::optional<std::string>& username);

private:
    void authenticateWithSavedToken();
    boost::asio::awaitable<void> asyncAuthenticateWithToken(std::string token);

    struct ValidateTokenResponse {
        std::chrono::seconds expiresIn{0};
        std::string userId;
    };
    boost::asio::awaitable<ValidateTokenResponse> asyncValidateToken(std::string token);
    bool tokenHasNeededScopes(const boost::json::value& validateTokenResponse);

    boost::asio::awaitable<void> asyncUpdateUsername();
    boost::asio::awaitable<std::optional<std::string>> asyncGetUsername();

    boost::asio::awaitable<void> asyncValidateTokenPeriodically();
    void emitAccessTokenAboutToExpireIfNeeded(std::chrono::seconds expiresIn);

    boost::asio::awaitable<void> asyncRunAuthServer();
    boost::asio::awaitable<void> asyncProcessRequest(boost::asio::ip::tcp::socket socket);
    boost::beast::http::response<boost::beast::http::string_body> getResponse(
        const boost::beast::http::request<boost::beast::http::string_body>& request
    );

    std::string getDoNotShowOnStreamPageUrl();
    std::string getDoNotShowOnStreamPageHtml(const std::string& csrfState);
    std::string getAuthPageUrl(const std::string& csrfState);
    std::string getAuthRedirectPageUrl();
    std::string getAuthRedirectPageHtml();

    std::string generateCsrfState();
    bool isValidCsrfState(const std::string& csrfState);

    Settings& settings;
    std::string clientId;
    std::set<std::string> scopes;
    std::uint16_t authServerPort;
    HttpClient& httpClient;
    boost::asio::io_context& ioContext;

    std::optional<std::string> accessToken;
    std::optional<std::string> userId;
    std::optional<std::string> username;
    mutable std::mutex userMutex;

    std::set<std::string> csrfStates;
    std::default_random_engine randomEngine;
    std::mutex csrfStatesMutex;
};
