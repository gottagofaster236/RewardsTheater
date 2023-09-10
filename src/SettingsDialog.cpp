// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "SettingsDialog.h"

#include <fmt/core.h>
#include <obs-module.h>
#include <obs.h>

#include <iostream>
#include <obs.hpp>

#include "Log.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(RewardsTheaterPlugin& plugin, QWidget* parent)
    : QDialog(parent), plugin(plugin), ui(std::make_unique<Ui::SettingsDialog>()),
      authenticateWithTwitchDialog(new AuthenticateWithTwitchDialog(this, plugin.getTwitchAuth())) {
    ui->setupUi(this);
    connect(ui->authButton, &QPushButton::clicked, this, &SettingsDialog::logInOrLogOut);
    connect(ui->openRewardsQueueButton, &QPushButton::clicked, this, &SettingsDialog::openRewardsQueue);
    connect(&plugin.getTwitchAuth(), &TwitchAuth::onUsernameChanged, this, &SettingsDialog::updateAuthButtonText);
}

void SettingsDialog::logInOrLogOut() {
    TwitchAuth& auth = plugin.getTwitchAuth();
    if (auth.isAuthenticated()) {
        auth.logOut();
    } else {
        authenticateWithTwitchDialog->open();
    }
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::openRewardsQueue() {
    log(LOG_INFO, "onOpenRewardsQueueClicked");
}

void SettingsDialog::updateAuthButtonText(const std::optional<std::string>& username) {
    std::string newText;
    if (plugin.getTwitchAuth().isAuthenticated()) {
        std::string usernameShown;
        if (username) {
            usernameShown = username.value();
        } else {
            usernameShown = obs_module_text("ErrorUsername");
        }
        newText = fmt::format(fmt::runtime(obs_module_text("LogOut")), usernameShown);
    } else {
        newText = obs_module_text("LogIn");
    }
    ui->authButton->setText(QString::fromStdString(newText));
}
