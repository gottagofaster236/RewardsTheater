// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "RewardsTheaterPlugin.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>

#include <QAction>
#include <QMainWindow>
#include <algorithm>
#include <memory>
#include <thread>

#include "SettingsDialog.h"

// https://dev.twitch.tv/docs/authentication/register-app/
static const char* const TWITCH_CLIENT_ID = "2u4jgrdekf0pwdpq7cmqcarifv93z3";
// Use several ports to minimize the probability of collision between several running OBS instances.
static constexpr std::array AUTH_SERVER_PORTS = {19910, 19911, 19912, 19913, 19914, 19915, 19916, 19917, 19918, 19919};

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
      twitchRewardsApi(twitchAuth, httpClient, ioThreadPool.ioContext),
      githubUpdateApi(httpClient, ioThreadPool.ioContext), rewardRedemptionQueue(settings, twitchRewardsApi),
      pubsubListener(twitchAuth, rewardRedemptionQueue) {
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
