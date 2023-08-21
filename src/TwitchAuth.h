// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#pragma warning(push)
#pragma warning(disable : 4702)
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/ptree.hpp>
#pragma warning(pop)
#include <chrono>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

namespace http = boost::beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

/**
 * A class for Twitch authentication using the Implicit grant flow.
 * Read more here: https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#implicit-grant-flow
 */
class TwitchAuth {
public:
    TwitchAuth(const std::string& clientId, const std::set<std::string>& scopes, std::uint16_t authServerPort);
    ~TwitchAuth();
    std::optional<std::string> getAccessToken();
    void authenticate();
    void authenticateWithToken(const std::string& token);
    std::chrono::seconds tokenExpiresIn(const std::string& token);

private:
    bool tokenHasNeededScopes(const boost::property_tree::ptree& oauthValidateResponse);
    void startAuthServer(std::uint16_t port);
    std::string getAuthUrl();

    std::optional<std::string> accessToken;
    std::mutex accessTokenMutex;

    std::thread authServerThread;
    boost::asio::io_context ioContext;

    std::string clientId;
    std::set<std::string> scopes;
    std::uint16_t authServerPort;
};
