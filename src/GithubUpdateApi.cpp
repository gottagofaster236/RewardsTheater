// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "GithubUpdateApi.h"

#include <sstream>

#include "HttpUtil.h"
#include "Log.h"
#include "RewardsTheaterVersion.generated.h"

namespace asio = boost::asio;

GithubUpdateApi::GithubUpdateApi(asio::io_context& ioContext) : ioContext(ioContext) {}

GithubUpdateApi::~GithubUpdateApi() = default;

void GithubUpdateApi::checkForUpdates() {
    asio::co_spawn(ioContext, asyncCheckForUpdates(), asio::detached);
}

asio::awaitable<void> GithubUpdateApi::asyncCheckForUpdates() {
    try {
        if (co_await isUpdateAvailable()) {
            emit onUpdateAvailable();
        }
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Exception in asyncCheckForUpdates: {}", exception.what());
    }
}

asio::awaitable<bool> GithubUpdateApi::isUpdateAvailable() {
    std::vector<int> ourVersion = parseVersion(REWARDS_THEATER_VERSION);
    std::vector<int> latestReleaseVersion = parseVersion(co_await getLatestReleaseVersion());
    co_return ourVersion < latestReleaseVersion;
}

asio::awaitable<std::string> GithubUpdateApi::getLatestReleaseVersion() {
    HttpUtil::Response response = co_await HttpUtil::request(
        ioContext,
        "api.github.com",
        "/repos/gottagofaster236/RewardsTheater/releases/latest",
        {{"User-Agent", "https://github.com/gottagofaster236/RewardsTheater"}}
    );
    co_return value_to<std::string>(response.json.at("tag_name"));
}

std::vector<int> GithubUpdateApi::parseVersion(const std::string& versionString) {
    std::istringstream iss(versionString);
    iss.exceptions(std::istringstream::failbit | std::istringstream::badbit);
    std::vector<int> version(3);
    char dot;
    iss >> version[0] >> dot >> version[1] >> dot >> version[2];
    return version;
}
