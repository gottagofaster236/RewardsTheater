// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "PubsubListener.h"

#include <fmt/core.h>

#include "Log.h"
#include "TwitchRewardsApi.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace json = boost::json;
using tcp = asio::ip::tcp;

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

static const auto RECONNECT_DELAY = 10s;
static const auto PING_PERIOD = 15s;
static const char* const CHANNEL_POINTS_TOPIC = "channel-points-channel-v1";

PubsubListener::PubsubListener(TwitchAuth& twitchAuth, RewardRedemptionQueue& rewardRedemptionQueue)
    : twitchAuth(twitchAuth), rewardRedemptionQueue(rewardRedemptionQueue), pubsubThread(1),
      usernameCondVar(pubsubThread.ioContext, boost::posix_time::pos_infin),
      lastPongReceivedAt(std::chrono::steady_clock::now()) {
    connect(&twitchAuth, &TwitchAuth::onUsernameChanged, this, &PubsubListener::reconnectAfterUsernameChange);
    asio::co_spawn(pubsubThread.ioContext, asyncReconnectToPubsubForever(), asio::detached);
}

PubsubListener::~PubsubListener() {
    pubsubThread.stop();
}

void PubsubListener::reconnectAfterUsernameChange() {
    asio::post(pubsubThread.ioContext, [this] {
        usernameCondVar.cancel();  // Equivalent to notify_all() for a condition variable.
    });
}

const char* PubsubListener::NoPingAnswerException::what() const noexcept {
    return "NoPingAnswerException";
}

asio::awaitable<void> PubsubListener::asyncReconnectToPubsubForever() {
    while (true) {
        std::optional<std::string> usernameOptional = twitchAuth.getUsername();
        if (!usernameOptional.has_value()) {
            try {
                co_await usernameCondVar.async_wait(asio::use_awaitable);
            } catch (const boost::system::system_error&) {
                // Username updated.
            }
            continue;
        }
        std::string username = usernameOptional.value();

        try {
            co_await (asyncConnectToPubsub(username) && usernameCondVar.async_wait(asio::use_awaitable));
        } catch (const std::exception& e) {
            log(LOG_ERROR, "Exception in asyncReconnectToPubsubForever: {}", e.what());
        }

        if (twitchAuth.getUsername() != username) {
            // Disconnected because of a username change - reconnect immediately.
            continue;
        }
        co_await asio::steady_timer(pubsubThread.ioContext, RECONNECT_DELAY).async_wait(asio::use_awaitable);
    }
}

asio::awaitable<void> PubsubListener::asyncConnectToPubsub(const std::string& username) {
    log(LOG_INFO, "Connecting to PubSub for user {}", username);
    WebsocketStream ws = co_await asyncConnect("pubsub-edge.twitch.tv");
    co_await asyncSubscribeToChannelPoints(ws);
    co_await (asyncSendPingMessages(ws) && asyncReadMessages(ws));
}

asio::awaitable<PubsubListener::WebsocketStream> PubsubListener::asyncConnect(const std::string& host) {
    ssl::context sslContext{ssl::context::tlsv12};
    sslContext.set_default_verify_paths();
    tcp::resolver resolver{pubsubThread.ioContext};
    WebsocketStream ws{pubsubThread.ioContext, sslContext};
    const auto resolveResults = co_await resolver.async_resolve(host, "https", asio::use_awaitable);

    co_await asio::async_connect(get_lowest_layer(ws), resolveResults, asio::use_awaitable);
    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
        throw boost::system::system_error(
            boost::system::error_code(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()),
            "Failed to set SNI Hostname"
        );
    }
    co_await ws.next_layer().async_handshake(ssl::stream_base::client, asio::use_awaitable);
    co_await ws.async_handshake(host, "/", asio::use_awaitable);
    co_return ws;
}

asio::awaitable<void> PubsubListener::asyncSubscribeToChannelPoints(WebsocketStream& ws) {
    std::string topicName = fmt::format("{}.{}", CHANNEL_POINTS_TOPIC, twitchAuth.getUserIdOrThrow());
    json::value message{
        {"type", "LISTEN"},
        {
            "data",
            {
                {
                    "topics",
                    {topicName},
                },
                {"auth_token", twitchAuth.getAccessTokenOrThrow()},
            },
        },
    };
    co_await asyncSendMessage(ws, message);
}

asio::awaitable<void> PubsubListener::asyncSendPingMessages(WebsocketStream& ws) {
    json::value message{{"type", "PING"}};
    while (true) {
        auto sentPingAt = std::chrono::steady_clock::now();
        co_await asyncSendMessage(ws, message);
        co_await asio::steady_timer(pubsubThread.ioContext, PING_PERIOD).async_wait(asio::use_awaitable);
        if (lastPongReceivedAt < sentPingAt) {
            throw NoPingAnswerException();
        }
    }
}

asio::awaitable<void> PubsubListener::asyncReadMessages(WebsocketStream& ws) {
    while (true) {
        json::value message = co_await asyncReadMessage(ws);
        std::string type = value_to<std::string>(message.at("type"));
        if (type == "PONG") {
            lastPongReceivedAt = std::chrono::steady_clock::now();
        } else if (type == "MESSAGE") {
            std::string topic = value_to<std::string>(message.at("data").at("topic"));
            if (!topic.starts_with(CHANNEL_POINTS_TOPIC)) {
                continue;
            }

            json::value rewardMessage = json::parse(value_to<std::string>(message.at("data").at("message")));
            std::string rewardMessageType = value_to<std::string>(rewardMessage.at("type"));
            if (rewardMessageType != "reward-redeemed") {
                continue;
            }

            json::value redemption = rewardMessage.at("data").at("redemption");
            Reward reward = TwitchRewardsApi::parsePubsubReward(redemption.at("reward"));
            std::string redemptionId = value_to<std::string>(redemption.at("id"));
            rewardRedemptionQueue.queueRewardRedemption(RewardRedemption{reward, redemptionId});
        }
    }
}

asio::awaitable<json::value> PubsubListener::asyncReadMessage(WebsocketStream& ws) {
    std::string message;
    auto buffer = asio::dynamic_buffer(message);
    co_await ws.async_read(buffer, asio::use_awaitable);
    if (message.empty()) {
        co_return json::value{};
    }
    co_return json::parse(message);
}

asio::awaitable<void> PubsubListener::asyncSendMessage(WebsocketStream& ws, const json::value& message) {
    std::string messageSerialized = json::serialize(message);
    co_await ws.async_write(asio::buffer(messageSerialized), asio::use_awaitable);
}
