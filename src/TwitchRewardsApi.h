// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <boost/json.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "BoostAsio.h"
#include "Reward.h"
#include "TwitchAuth.h"

namespace detail {
class QObjectCallback;
}

class TwitchRewardsApi : public QObject {
    Q_OBJECT

public:
    TwitchRewardsApi(TwitchAuth& twitchAuth, boost::asio::io_context& ioContext);
    ~TwitchRewardsApi();

    void updateRewards();
    void downloadImage(const Reward& reward, QObject* receiver, const char* member);

signals:
    void onRewardsUpdated(const std::vector<Reward>& rewards);

private:
    boost::asio::awaitable<void> asyncUpdateRewards();
    boost::asio::awaitable<std::vector<Reward>> asyncGetRewards();
    static Reward parseReward(const boost::json::value& reward);
    static Reward::Color hexColorToColor(const std::string& hexColor);
    static boost::urls::url getImageUrl(const boost::json::value& reward);
    static std::optional<std::int64_t> getOptionalSetting(const boost::json::value& setting, const std::string& key);

    boost::asio::awaitable<void> asyncDownloadImage(boost::urls::url url, detail::QObjectCallback& callback);

    TwitchAuth& twitchAuth;
    boost::asio::io_context& ioContext;
};

namespace detail {

// Calls a method on a QObject, or does nothing if the QObject no longer exists.
class QObjectCallback : public QObject {
    Q_OBJECT

public:
    QObjectCallback(TwitchRewardsApi* parent, QObject* receiver, const char* member);

    template <typename Result>
    void operator()(const char* typeName, const Result& result) {
        qRegisterMetaType<Result>(typeName);
        {
            std::lock_guard guard(receiverMutex);
            if (receiver) {
                QMetaObject::invokeMethod(
                    receiver, member, Qt::ConnectionType::QueuedConnection, QArgument(typeName, result)
                );
            }
        }
        deleteLater();
    }

private slots:
    void clearReceiver();

private:
    QObject* receiver;
    std::mutex receiverMutex;
    const char* member;
};

}  // namespace detail
