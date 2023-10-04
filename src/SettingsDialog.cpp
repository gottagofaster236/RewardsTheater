// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "SettingsDialog.h"

#include <obs-module.h>
#include <obs.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <obs.hpp>

#include "Log.h"
#include "RewardWidget.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(RewardsTheaterPlugin& plugin, QWidget* parent)
    : QDialog(parent), plugin(plugin), ui(std::make_unique<Ui::SettingsDialog>()),
      authenticateWithTwitchDialog(new AuthenticateWithTwitchDialog(this, plugin.getTwitchAuth())) {
    ui->setupUi(this);
    showUpdateAvailableTextIfNeeded();  // TODO remove

    connect(ui->authButton, &QPushButton::clicked, this, &SettingsDialog::logInOrLogOut);
    connect(ui->openRewardsQueueButton, &QPushButton::clicked, this, &SettingsDialog::openRewardsQueue);
    connect(&plugin.getTwitchAuth(), &TwitchAuth::onUsernameChanged, this, &SettingsDialog::updateAuthButtonText);
    connect(&plugin.getTwitchRewardsApi(), &TwitchRewardsApi::onRewardsUpdated, this, &SettingsDialog::showRewards);
}

void SettingsDialog::logInOrLogOut() {
    TwitchAuth& auth = plugin.getTwitchAuth();
    if (auth.isAuthenticated()) {
        auth.logOut();
    } else {
        authenticateWithTwitchDialog->show();
    }
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::toggleVisibility() {
    setVisible(!isVisible());
}

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
        newText = std::vformat(obs_module_text("LogOut"), std::make_format_args(usernameShown));
    } else {
        newText = obs_module_text("LogIn");
    }
    ui->authButton->setText(QString::fromStdString(newText));
}

void SettingsDialog::showRewards(const std::vector<Reward>& rewards) {
    // Remove old rewards
    for (int i = 0; i < ui->rewardsGridLayout->count(); i++) {
        ui->rewardsGridLayout[0].itemAt(i)->widget()->deleteLater();
    }

    std::vector<Reward> rewardsSorted = rewards;
    std::ranges::stable_sort(rewardsSorted, {}, [](const Reward& reward) {
        return reward.cost;
    });

    const int REWARDS_PER_ROW = 4;
    for (int i = 0; i < static_cast<int>(rewards.size()); i++) {
        RewardWidget* rewardWidget = new RewardWidget(rewardsSorted[i], plugin.getTwitchRewardsApi(), ui->rewardsGrid);
        ui->rewardsGridLayout->addWidget(rewardWidget, i / REWARDS_PER_ROW, i % REWARDS_PER_ROW);
    }
}

void SettingsDialog::showUpdateAvailableTextIfNeeded() {
    // TODO make this an asynchronous thing instead.
    if (!isUpdateAvailable()) {
        return;
    }

    const char* updateUrl = "https://github.com/gottagofaster236/RewardsTheater/releases/latest";
    const char* updateAvailableText = obs_module_text("UpdateAvailable");
    std::string updateAvailableLink = std::format(" <a href=\"{}\">{}</a>", updateUrl, updateAvailableText);

    ui->titleLabel->setText(ui->titleLabel->text() + QString::fromStdString(updateAvailableLink));
    ui->titleLabel->setTextFormat(Qt::RichText);
    ui->titleLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->titleLabel->setOpenExternalLinks(true);
}

bool SettingsDialog::isUpdateAvailable() {
    // TODO Create a class like GithubUpdateApi or something that would have a signal a la onUpdateAvailable().
    // Here connect to it and the "update available" text to the string.
    // Inside it query https://api.github.com/repos/gottagofaster236/RewardsTheater/releases/latest
    // And compare it from the version from CMakeLists.txt by splitting the version into three numbers (?)
    // And then comparing lexicographically. Or maybe boost has something like this.
    return true;
}
