// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "SettingsDialog.h"

#include <obs.h>

#include <iostream>
#include <obs.hpp>

#include "Log.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(RewardsTheaterPlugin& plugin, QWidget* parent)
    : QDialog(parent), plugin(plugin), ui(std::make_unique<Ui::SettingsDialog>()),
      authenticateWithTwitchDialog(new AuthenticateWithTwitchDialog(this, plugin.getTwitchAuth())) {
    ui->setupUi(this);
    connect(ui->authButton, &QPushButton::clicked, this, &SettingsDialog::onAuthButtonClicked);
    connect(ui->openRewardsQueueButton, &QPushButton::clicked, this, &SettingsDialog::onOpenRewardsQueueClicked);
}

void SettingsDialog::onAuthButtonClicked() {
    log(LOG_INFO, "onAuthButtonClicked");
    authenticateWithTwitchDialog->open();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::onOpenRewardsQueueClicked() {
    log(LOG_INFO, "onOpenRewardsQueueClicked");
}
