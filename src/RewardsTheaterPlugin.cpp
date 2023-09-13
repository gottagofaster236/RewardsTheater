// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardsTheaterPlugin.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>

#include <QAction>
#include <QMainWindow>
#include <memory>

#include "SettingsDialog.h"

static const char* const TWITCH_CLIENT_ID = "2u4jgrdekf0pwdpq7cmqcarifv93z3";
static const int AUTH_SERVER_PORT = 19918;

RewardsTheaterPlugin::RewardsTheaterPlugin()
    : settings(Settings(obs_frontend_get_global_config())),
      twitchAuth(
          settings,
          TWITCH_CLIENT_ID,
          {"channel:read:redemptions", "channel:manage:redemptions"},
          AUTH_SERVER_PORT
      ),
      twitchRewardsApi(twitchAuth), rewardsQueue(settings) {
    QMainWindow* mainWindow = (QMainWindow*) obs_frontend_get_main_window();
    QAction* action = (QAction*) obs_frontend_add_tools_menu_qaction(obs_module_text("RewardsTheater"));

    obs_frontend_push_ui_translation(obs_module_get_string);
    SettingsDialog* settingsDialog = new SettingsDialog(*this, mainWindow);
    obs_frontend_pop_ui_translation();

    auto menuCallback = [=] { settingsDialog->setVisible(!settingsDialog->isVisible()); };
    action->connect(action, &QAction::triggered, menuCallback);
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

RewardsQueue& RewardsTheaterPlugin::getRewardsQueue() {
    return rewardsQueue;
}
