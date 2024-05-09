// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "RewardsTheaterPlugin.h"

#include <fmt/core.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>

#include <QAction>
#include <QMainWindow>
#include <QMessageBox>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <thread>

#include "Log.h"
#include "RewardsTheaterVersion.generated.h"
#include "SettingsDialog.h"

// https://dev.twitch.tv/docs/authentication/register-app/
static const char* const TWITCH_CLIENT_ID = "2u4jgrdekf0pwdpq7cmqcarifv93z3";
// Use several ports to minimize the probability of collision between several running OBS instances.
static constexpr std::array AUTH_SERVER_PORTS = {19910, 19911, 19912, 19913, 19914, 19915, 19916, 19917, 19918, 19919};

static const int MIN_OBS_VERSION = 503316480;
static const char* const MIN_OBS_VERSION_STRING = "30.0.0";

RewardsTheaterPlugin::RewardsTheaterPlugin()
    : settings(obs_frontend_get_global_config()), ioThreadPool(std::max(2u, std::thread::hardware_concurrency())),
      httpClient(ioThreadPool.ioContext), twitchAuth(
                                              settings,
                                              TWITCH_CLIENT_ID,
                                              {"channel:read:redemptions", "channel:manage:redemptions"},
                                              AUTH_SERVER_PORTS[std::random_device()() % AUTH_SERVER_PORTS.size()],
                                              httpClient,
                                              ioThreadPool.ioContext
                                          ),
      twitchRewardsApi(twitchAuth, httpClient, settings, ioThreadPool.ioContext),
      githubUpdateApi(httpClient, ioThreadPool.ioContext), rewardRedemptionQueue(settings, twitchRewardsApi),
      pubsubListener(twitchAuth, rewardRedemptionQueue) {
    log(LOG_INFO, "Loading plugin, version {}", REWARDS_THEATER_VERSION);
    checkMinObsVersion();
    checkRestrictedRegion();

    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());

    obs_frontend_push_ui_translation(obs_module_get_string);
    SettingsDialog* settingsDialog = new SettingsDialog(*this, mainWindow);
    obs_frontend_pop_ui_translation();

    QAction* action = static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(obs_module_text("RewardsTheater")));
    QObject::connect(action, &QAction::triggered, settingsDialog, &SettingsDialog::toggleVisibility);

    twitchAuth.startService();
    githubUpdateApi.checkForUpdates();
}

RewardsTheaterPlugin::~RewardsTheaterPlugin() {
    // Stop the thread pool before destructing the objects that use it,
    // so that no callbacks are called on destructed objects.
    ioThreadPool.stop();
}

Settings& RewardsTheaterPlugin::getSettings() {
    return settings;
}

TwitchAuth& RewardsTheaterPlugin::getTwitchAuth() {
    return twitchAuth;
}

TwitchRewardsApi& RewardsTheaterPlugin::getTwitchRewardsApi() {
    return twitchRewardsApi;
}

GithubUpdateApi& RewardsTheaterPlugin::getGithubUpdateApi() {
    return githubUpdateApi;
}

RewardRedemptionQueue& RewardsTheaterPlugin::getRewardRedemptionQueue() {
    return rewardRedemptionQueue;
}

const char* RewardsTheaterPlugin::UnsupportedObsVersionException::what() const noexcept {
    return "UnsupportedObsVersionException";
}

const char* RewardsTheaterPlugin::RestrictedRegionException::what() const noexcept {
    return "RestrictedRegionException";
}

void RewardsTheaterPlugin::checkMinObsVersion() {
    if (obs_get_version() < MIN_OBS_VERSION) {
        std::string message = fmt::format(
            fmt::runtime(obs_module_text("ObsVersionUnsupported")), MIN_OBS_VERSION_STRING, obs_get_version_string()
        );
        QMessageBox::critical(nullptr, obs_module_text("RewardsTheater"), QString::fromStdString(message));
        throw UnsupportedObsVersionException();
    }
}

void RewardsTheaterPlugin::checkRestrictedRegion() {
    if (std::getenv("container")) {
        // Inside Flatpak
        return;
    }

    if (std::strcmp(obs_get_locale(), "ru-RU") != 0) {
        return;
    }

    const char* restrictionsSkipValue = std::getenv("SLAVA_UKRAINI");
    if (restrictionsSkipValue && std::strcmp(restrictionsSkipValue, "1") == 0) {
        return;
    }

    QMessageBox::critical(
        nullptr,
        "Restricted region",
        "Based on your system settings, it appears you're located in Russia.\n\n"
        "You cannot use RewardsTheater on the territory of a terrorist state.\n\n"
        "If you wish to bypass this check, set the environment variable `SLAVA_UKRAINI=1` in your system settings."
    );
    throw RestrictedRegionException();
}
