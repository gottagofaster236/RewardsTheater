// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <QObject>
#include <chrono>
#include <exception>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

#include "Settings.h"

/**
 * A class for Twitch authentication using the Implicit grant flow.
 * Read more here: https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#implicit-grant-flow
 */
class TwitchAuth : public QObject {
    Q_OBJECT

public:
    TwitchAuth(
        Settings& settings,
        const std::string& clientId,
        const std::set<std::string>& scopes,
        std::uint16_t authServerPort
    );
    ~TwitchAuth();

    void startService();

    std::optional<std::string> getAccessToken() const;
    std::string getAccessTokenOrThrow() const;
    bool isAuthenticated() const;
    std::optional<std::string> getUserId();
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
    void onAccessTokenAboutToExpire(std::chrono::seconds expiresIn);
    void onUsernameChanged(std::optional<std::string> username);

private:
    void authenticateWithSavedToken();
    boost::asio::awaitable<void> asyncAuthenticateWithToken(std::string token, boost::asio::io_context& ioContext);

    struct ValidateTokenResponse {
        std::chrono::seconds expiresIn{0};
        std::string userId;
    };
    boost::asio::awaitable<ValidateTokenResponse> asyncValidateToken(
        std::string token,
        boost::asio::io_context& ioContext
    );
    bool tokenHasNeededScopes(const boost::property_tree::ptree& validateTokenResponse);

    std::string getAuthUrl();

    boost::asio::awaitable<std::optional<std::string>> asyncGetUsername(boost::asio::io_context& ioContext);

    boost::asio::awaitable<void> asyncRunAuthServer(boost::asio::io_context& ioContext);
    boost::asio::awaitable<void> asyncProcessRequest(
        boost::asio::ip::tcp::socket socket,
        boost::asio::io_context& ioContext
    );

    boost::asio::awaitable<void> asyncValidateTokenPeriodically(boost::asio::io_context& ioContext);
    void emitAccessTokenAboutToExpireIfNeeded(std::chrono::seconds expiresIn);

    std::optional<std::string> accessToken;
    std::optional<std::string> userId;
    mutable std::mutex accessTokenMutex;

    std::thread authThread;
    boost::asio::io_context authIoContext;

    Settings& settings;
    std::string clientId;
    std::set<std::string> scopes;
    std::uint16_t authServerPort;
};
