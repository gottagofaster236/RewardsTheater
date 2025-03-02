// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "EventsubListener.h"

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

static constexpr auto INITIAL_KEEPALIVE_TIMEOUT = 30s;
static constexpr auto RECONNECT_DELAY = 10s;
static const char* const CHANNEL_POINTS_SUBSCRIPTION_TYPE = "channel.channel_points_custom_reward_redemption.add";

EventsubListener::EventsubListener(
    TwitchAuth& twitchAuth,
    HttpClient& httpClient,
    RewardRedemptionQueue& rewardRedemptionQueue
)
    : twitchAuth(twitchAuth), httpClient(httpClient), rewardRedemptionQueue(rewardRedemptionQueue), eventsubThread(1),
      eventsubUrl("wss://eventsub.wss.twitch.tv/ws"), processedMessageIds{}, sessionId{},
      keepaliveTimeoutTimer(eventsubThread.ioContext), keepaliveTimeout(INITIAL_KEEPALIVE_TIMEOUT),
      usernameCondVar(eventsubThread.ioContext, boost::posix_time::pos_infin) {
    connect(&twitchAuth, &TwitchAuth::onUsernameChanged, this, &EventsubListener::reconnectAfterUsernameChange);
    asio::co_spawn(eventsubThread.ioContext, asyncReconnectToEventsubForever(), asio::detached);
}

EventsubListener::~EventsubListener() {
    eventsubThread.stop();
}

void EventsubListener::reconnectAfterUsernameChange() {
    asio::post(eventsubThread.ioContext, [this] {
        usernameCondVar.cancel();  // Equivalent to notify_all() for a condition variable.
    });
}

const char* EventsubListener::KeepaliveTimeoutException::what() const noexcept {
    return "KeepaliveTimeoutException";
}

const char* EventsubListener::SubscribeToChannelPointsException::what() const noexcept {
    return "SubscribeToChannelPointsException";
}

const char* EventsubListener::ReconnectException::what() const noexcept {
    return "ReconnectException";
}

std::string getMultipleExceptionsMessage(std::exception_ptr e);

asio::awaitable<void> EventsubListener::asyncReconnectToEventsubForever() {
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
            co_await (asyncConnectToEventsub(username) && usernameCondVar.async_wait(asio::use_awaitable));
        } catch (...) {
            log(LOG_ERROR,
                "Exception in asyncReconnectToEventsubForever: {}",
                getMultipleExceptionsMessage(std::current_exception()));
        }

        if (twitchAuth.getUsername() != username) {
            // Disconnected because of a username change - reconnect immediately.
            continue;
        }
        co_await asio::steady_timer(eventsubThread.ioContext, RECONNECT_DELAY).async_wait(asio::use_awaitable);
    }
}

std::string getMultipleExceptionsMessage(std::exception_ptr exceptionPointer) {
    while (true) {
        try {
            std::rethrow_exception(exceptionPointer);
        } catch (const asio::multiple_exceptions& multipleExceptions) {
            exceptionPointer = multipleExceptions.first_exception();
        } catch (const std::exception& e) {
            return e.what();
        }
    }
}

asio::awaitable<void> EventsubListener::asyncConnectToEventsub(const std::string& username) {
    log(LOG_INFO, "Connecting to EventSub URL {} for user {}", eventsubUrl.c_str(), username);
    WebsocketStream ws = co_await asyncConnect();
    keepaliveTimeout = INITIAL_KEEPALIVE_TIMEOUT;
    resetKeepaliveTimeoutTimer();
    co_await (asyncSubscribeAndReadMessages(ws) && asyncMonitorKeepaliveTimeout());
}

asio::awaitable<EventsubListener::WebsocketStream> EventsubListener::asyncConnect() {
    ssl::context sslContext{ssl::context::tlsv12};
    sslContext.set_default_verify_paths();
    tcp::resolver resolver{eventsubThread.ioContext};
    WebsocketStream ws{eventsubThread.ioContext, sslContext};
    const auto resolveResults = co_await resolver.async_resolve(eventsubUrl.host(), "https", asio::use_awaitable);

    co_await asio::async_connect(get_lowest_layer(ws), resolveResults, asio::use_awaitable);
    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), eventsubUrl.host().c_str())) {
        throw boost::system::system_error(
            boost::system::error_code(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()),
            "Failed to set SNI Hostname"
        );
    }
    co_await ws.next_layer().async_handshake(ssl::stream_base::client, asio::use_awaitable);
    co_await ws.async_handshake(eventsubUrl.host(), eventsubUrl.encoded_target(), asio::use_awaitable);
    co_return ws;
}

boost::asio::awaitable<void> EventsubListener::asyncSubscribeAndReadMessages(WebsocketStream& ws) {
    co_await asyncWaitForWelcomeMessage(ws);
    co_await asyncSubscribeToChannelPoints();
    co_await asyncReadMessages(ws);
}

boost::asio::awaitable<void> EventsubListener::asyncMonitorKeepaliveTimeout() {
    while (true) {
        auto expiry = keepaliveTimeoutTimer.expiry();
        try {
            co_await keepaliveTimeoutTimer.async_wait(asio::use_awaitable);
        } catch (const boost::system::system_error&) {
            if (keepaliveTimeoutTimer.expiry() != expiry) {
                // Timeout updated.
                continue;
            } else {
                throw;
            }
        }
        log(LOG_ERROR, "Keepalive timeout expired");
        throw KeepaliveTimeoutException();
    }
}

void EventsubListener::resetKeepaliveTimeoutTimer() {
    auto newExpiry = std::chrono::steady_clock::now() + keepaliveTimeout;
    // expires_at cancels any pending waits. Make sure we don't call it without a reason.
    if (newExpiry != keepaliveTimeoutTimer.expiry()) {
        keepaliveTimeoutTimer.expires_at(newExpiry);
    }
}

asio::awaitable<void> EventsubListener::asyncWaitForWelcomeMessage(WebsocketStream& ws) {
    json::value message;
    do {
        message = co_await asyncReadMessage(ws);
    } while (getMessageType(message) != "session_welcome");

    log(LOG_INFO, "Successfully connected to EventSub");

    const json::value& session = message.at("payload").at("session");
    sessionId = value_to<std::string>(session.at("id"));
    int keepaliveTimeoutSeconds = value_to<int>(session.at("keepalive_timeout_seconds"));
    // Twitch server sends a keepalive message every {keepaliveTimeoutSeconds}.
    // Because it's not 100% precise, sometimes it's a bit less than keepaliveTimeoutSeconds, sometimes a bit more.
    // Therefore just multiply it by two to be safe.
    keepaliveTimeout = std::chrono::seconds(keepaliveTimeoutSeconds * 2);
    resetKeepaliveTimeoutTimer();
}

asio::awaitable<void> EventsubListener::asyncSubscribeToChannelPoints() {
    json::value requestBody{
        {"type", "channel.channel_points_custom_reward_redemption.add"},
        {"version", "1"},
        {"condition",
         {
             {"broadcaster_user_id", twitchAuth.getUserIdOrThrow()},
         }},
        {"transport",
         {
             {"method", "websocket"},
             {"session_id", sessionId},
         }}
    };
    HttpClient::Response response = co_await httpClient.request(
        "api.twitch.tv", "/helix/eventsub/subscriptions", twitchAuth, {}, http::verb::post, requestBody
    );
    if (response.status != http::status::accepted) {
        log(LOG_ERROR, "HTTP status {} in asyncSubscribeToChannelPoints", static_cast<int>(response.status));
        throw SubscribeToChannelPointsException();
    }
}

asio::awaitable<void> EventsubListener::asyncReadMessages(WebsocketStream& ws) {
    while (true) {
        json::value message = co_await asyncReadMessage(ws);
        std::string type = getMessageType(message);

        if (type == "notification") {
            json::value payload = message.at("payload");
            std::string subscriptionType = value_to<std::string>(payload.at("subscription").at("type"));
            if (subscriptionType != CHANNEL_POINTS_SUBSCRIPTION_TYPE) {
                continue;
            }
            json::value event = payload.at("event");
            Reward reward = TwitchRewardsApi::parseEventsubReward(event.at("reward"));
            std::string redemptionId = value_to<std::string>(event.at("id"));
            rewardRedemptionQueue.queueRewardRedemption(RewardRedemption{reward, redemptionId});
        } else if (type == "session_reconnect") {
            throw ReconnectException();
        }
    }
}

asio::awaitable<json::value> EventsubListener::asyncReadMessage(WebsocketStream& ws) {
    while (true) {
        json::value message = co_await asyncReadMessageIgnoringDuplicates(ws);
        std::string messageId;
        try {
            messageId = value_to<std::string>(message.at("metadata").at("message_id"));
        } catch (const boost::system::system_error&) {
            log(LOG_ERROR, "Could not parse message_id");
            co_return message;
        }
        // insert returns a pair of <iterator, whether insertion took place>
        if (processedMessageIds.insert(messageId).second) {
            co_return message;
        }
        // Received a duplicate messsage, skip it and read the next one.
    }
}

asio::awaitable<json::value> EventsubListener::asyncReadMessageIgnoringDuplicates(WebsocketStream& ws) {
    std::string message;
    auto buffer = asio::dynamic_buffer(message);
    co_await ws.async_read(buffer, asio::use_awaitable);
    resetKeepaliveTimeoutTimer();
    if (message.empty()) {
        co_return json::value{};
    }
    co_return json::parse(message);
}

std::string EventsubListener::getMessageType(const boost::json::value& message) {
    return value_to<std::string>(message.at("metadata").at("message_type"));
}

asio::awaitable<void> EventsubListener::asyncSendMessage(WebsocketStream& ws, const json::value& message) {
    std::string messageSerialized = json::serialize(message);
    co_await ws.async_write(asio::buffer(messageSerialized), asio::use_awaitable);
}
