// SPDX-License-Identifier: GPL-2.0-or-later
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
static const std::array<int, 10> AUTH_SERVER_PORTS =
    {19910, 19911, 19912, 19913, 19914, 19915, 19916, 19917, 19918, 19919};

RewardsTheaterPlugin::RewardsTheaterPlugin()
    : settings(Settings(obs_frontend_get_global_config())),
      ioThreadPool(std::max(2u, std::thread::hardware_concurrency())),
      twitchAuth(
          settings,
          TWITCH_CLIENT_ID,
          {"channel:read:redemptions", "channel:manage:redemptions"},
          AUTH_SERVER_PORTS[std::random_device()() % AUTH_SERVER_PORTS.size()],
          ioThreadPool.ioContext
      ),
      twitchRewardsApi(twitchAuth, ioThreadPool.ioContext), rewardsQueue(settings) {
    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());

    obs_frontend_push_ui_translation(obs_module_get_string);
    SettingsDialog* settingsDialog = new SettingsDialog(*this, mainWindow);
    obs_frontend_pop_ui_translation();

    QAction* action = static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(obs_module_text("RewardsTheater")));
    QObject::connect(action, &QAction::triggered, settingsDialog, &SettingsDialog::toggleVisibility);

    twitchAuth.startService();
}

RewardsTheaterPlugin::~RewardsTheaterPlugin() = default;

Settings& RewardsTheaterPlugin::getSettings() {
    return settings;
}

TwitchAuth& RewardsTheaterPlugin::getTwitchAuth() {
    return twitchAuth;
}

TwitchRewardsApi& RewardsTheaterPlugin::getTwitchRewardsApi() {
    return twitchRewardsApi;
}

RewardsQueue& RewardsTheaterPlugin::getRewardsQueue() {
    return rewardsQueue;
}
