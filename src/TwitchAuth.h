// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QObject>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <exception>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

#include "BoostAsio.h"
#include "IoService.h"
#include "Settings.h"

/**
 * A class for Twitch authentication using the Implicit grant flow.
 * Read more here: https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#implicit-grant-flow
 */
class TwitchAuth : public QObject, public IoService {
    Q_OBJECT

public:
    TwitchAuth(
        Settings& settings,
        const std::string& clientId,
        const std::set<std::string>& scopes,
        std::uint16_t authServerPort
    );
    ~TwitchAuth();

    std::optional<std::string> getAccessToken() const;
    std::string getAccessTokenOrThrow() const;
    bool isAuthenticated() const;
    std::optional<std::string> getUserId() const;
    std::string getUserIdOrThrow() const;
    const std::string& getClientId() const;

    void authenticate();
    void authenticateWithToken(const std::string& token);
    void logOut();
    void logOutAndEmitAuthenticationFailure();

    class UnauthenticatedException : public std::exception {
    public:
        const char* what() const noexcept override;
    };

    enum class AuthenticationFailureReason {
        AUTH_TOKEN_INVALID,
        NETWORK_ERROR,
    };

signals:
    void onAuthenticationSuccess();
    void onAuthenticationFailure(AuthenticationFailureReason reason);
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
    bool tokenHasNeededScopes(const boost::property_tree::ptree& validateTokenResponse);

    std::string getAuthUrl();

    boost::asio::awaitable<std::optional<std::string>> asyncGetUsername();

    boost::asio::awaitable<void> asyncRunAuthServer();
    boost::asio::awaitable<void> asyncProcessRequest(boost::asio::ip::tcp::socket socket);

    boost::asio::awaitable<void> asyncValidateTokenPeriodically();
    void emitAccessTokenAboutToExpireIfNeeded(std::chrono::seconds expiresIn);

    std::optional<std::string> accessToken;
    std::optional<std::string> userId;
    mutable std::mutex accessTokenMutex;

    Settings& settings;
    std::string clientId;
    std::set<std::string> scopes;
    std::uint16_t authServerPort;
};
