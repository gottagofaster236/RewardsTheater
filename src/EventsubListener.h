// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once
#include <QObject>
#include <boost/json.hpp>
#include <chrono>
#include <exception>
#include <set>

#include "BoostAsio.h"
#include "HttpClient.h"
#include "IoThreadPool.h"
#include "RewardRedemptionQueue.h"
#include "TwitchAuth.h"

/// Listens to channel points redemptions. Read https://dev.twitch.tv/docs/eventsub/ for API documentation.
class EventsubListener : public QObject {
    Q_OBJECT

public:
    EventsubListener(TwitchAuth& twitchAuth, HttpClient& httpClient, RewardRedemptionQueue& rewardRedemptionQueue);
    ~EventsubListener();

private slots:
    void reconnectAfterUsernameChange();

private:
    using WebsocketStream = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>;

    class KeepaliveTimeoutException : public std::exception {
        const char* what() const noexcept override;
    };

    class SubscribeToChannelPointsException : public std::exception {
        const char* what() const noexcept override;
    };

    class ReconnectException : public std::exception {
        const char* what() const noexcept override;
    };

    boost::asio::awaitable<void> asyncReconnectToEventsubForever();
    boost::asio::awaitable<void> asyncConnectToEventsub(const std::string& username);
    boost::asio::awaitable<WebsocketStream> asyncConnect();
    boost::asio::awaitable<void> asyncSubscribeAndReadMessages(WebsocketStream& ws);
    boost::asio::awaitable<void> asyncMonitorKeepaliveTimeout();
    void resetKeepaliveTimeoutTimer();
    boost::asio::awaitable<void> asyncWaitForWelcomeMessage(WebsocketStream& ws);
    boost::asio::awaitable<void> asyncSubscribeToChannelPoints();
    boost::asio::awaitable<void> asyncReadMessages(WebsocketStream& ws);
    boost::asio::awaitable<boost::json::value> asyncReadMessage(WebsocketStream& ws);
    boost::asio::awaitable<boost::json::value> asyncReadMessageIgnoringDuplicates(WebsocketStream& ws);
    static std::string getMessageType(const boost::json::value& message);
    static boost::asio::awaitable<void> asyncSendMessage(WebsocketStream& ws, const boost::json::value& message);

    TwitchAuth& twitchAuth;
    HttpClient& httpClient;
    RewardRedemptionQueue& rewardRedemptionQueue;
    IoThreadPool eventsubThread;
    boost::urls::url eventsubUrl;
    std::set<std::string> processedMessageIds;
    std::string sessionId;
    boost::asio::steady_timer keepaliveTimeoutTimer;
    std::chrono::seconds keepaliveTimeout;
    boost::asio::deadline_timer usernameCondVar;
};
