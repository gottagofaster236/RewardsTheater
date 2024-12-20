// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once
#include <QObject>
#include <boost/json.hpp>
#include <chrono>
#include <exception>

#include "BoostAsio.h"
#include "IoThreadPool.h"
#include "RewardRedemptionQueue.h"
#include "TwitchAuth.h"

/// Listens to channel points redemptions. Read https://dev.twitch.tv/docs/pubsub/ for API documentation.
class PubsubListener : public QObject {
    Q_OBJECT

public:
    PubsubListener(TwitchAuth& twitchAuth, RewardRedemptionQueue& rewardRedemptionQueue);
    ~PubsubListener();

private slots:
    void reconnectAfterUsernameChange();

private:
    using WebsocketStream = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>;

    class NoPingAnswerException : public std::exception {
        const char* what() const noexcept override;
    };

    boost::asio::awaitable<void> asyncReconnectToPubsubForever();
    boost::asio::awaitable<void> asyncConnectToPubsub(const std::string& username);
    boost::asio::awaitable<WebsocketStream> asyncConnect(const std::string& host);
    boost::asio::awaitable<void> asyncSubscribeToChannelPoints(WebsocketStream& ws);
    boost::asio::awaitable<void> asyncSendPingMessages(WebsocketStream& ws);
    boost::asio::awaitable<void> asyncReadMessages(WebsocketStream& ws);
    static boost::asio::awaitable<boost::json::value> asyncReadMessage(WebsocketStream& ws);
    static boost::asio::awaitable<void> asyncSendMessage(WebsocketStream& ws, const boost::json::value& message);

    TwitchAuth& twitchAuth;
    RewardRedemptionQueue& rewardRedemptionQueue;
    IoThreadPool pubsubThread;
    boost::asio::deadline_timer usernameCondVar;
    std::chrono::steady_clock::time_point lastPongReceivedAt;
};
