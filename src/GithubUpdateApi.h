// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QObject>

#include "BoostAsio.h"

class GithubUpdateApi : public QObject {
    Q_OBJECT

public:
    GithubUpdateApi(boost::asio::io_context& ioContext);
    ~GithubUpdateApi();
    void checkForUpdates();

signals:
    void onUpdateAvailable();

private:
    boost::asio::awaitable<void> asyncCheckForUpdates();
    boost::asio::awaitable<bool> isUpdateAvailable();
    boost::asio::awaitable<std::string> getLatestReleaseVersion();
    std::vector<int> parseVersion(const std::string& versionString);

private:
    boost::asio::io_context& ioContext;
};
