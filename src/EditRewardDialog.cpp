// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "EditRewardDialog.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <format>

#include "ui_EditRewardDialog.h"

EditRewardDialog::EditRewardDialog(
    const std::optional<Reward>& rewardOptional,
    TwitchAuth& twitchAuth,
    TwitchRewardsApi& twitchRewardsApi,
    QWidget* parent
)
    : QDialog(parent), twitchAuth(twitchAuth), twitchRewardsApi(twitchRewardsApi),
      ui(std::make_unique<Ui::EditRewardDialog>()) {
    obs_frontend_push_ui_translation(obs_module_get_string);
    ui->setupUi(this);
    obs_frontend_pop_ui_translation();
    setFixedSize(size());
    setAttribute(Qt::WA_DeleteOnClose);

    if (rewardOptional.has_value()) {
        showReward(rewardOptional.value());
    } else {
        showAddRewardUi();
    }
    showUploadCustomIconLabel(twitchAuth.getUsername());

    connect(ui->saveButton, &QPushButton::clicked, this, &EditRewardDialog::saveReward);
    connect(ui->cancelButton, &QPushButton::clicked, this, &EditRewardDialog::close);
    connect(ui->deleteButton, &QPushButton::clicked, this, &EditRewardDialog::deleteReward);
    connect(&twitchAuth, &TwitchAuth::onUsernameChanged, this, &EditRewardDialog::showUploadCustomIconLabel);
}

EditRewardDialog::~EditRewardDialog() = default;

void EditRewardDialog::showReward(const Reward& reward) {
    rewardId = reward.id;

    ui->enabledCheckBox->setChecked(reward.isEnabled);
    ui->titleEdit->setText(QString::fromStdString(reward.title));
    ui->costSpinBox->setValue(reward.cost);

    std::string backgroundColorStyle = std::format("background: {}", reward.backgroundColor.toHex());
    ui->backgroundColorButton->setStyleSheet(QString::fromStdString(backgroundColorStyle));

    ui->limitRedemptionsPerStreamCheckBox->setChecked(reward.maxRedemptionsPerStream.has_value());
    ui->limitRedemptionsPerStreamSpinBox->setValue(reward.maxRedemptionsPerStream.value_or(1));
    ui->limitRedemptionsPerUserPerStreamCheckBox->setChecked(reward.maxRedemptionsPerUserPerStream.has_value());
    ui->limitRedemptionsPerUserPerStreamSpinBox->setValue(reward.maxRedemptionsPerUserPerStream.value_or(1));
    ui->enableGlobalCooldownCheckBox->setChecked(reward.globalCooldownSeconds.has_value());
    ui->enableGlobalCooldownSpinBox->setValue(reward.globalCooldownSeconds.value_or(1));

    if (reward.canManage) {
        ui->cannotEditRewardLabel->hide();
        ui->deleteButton->setEnabled(true);
    } else {
        ui->enabledCheckBox->setEnabled(false);
        ui->titleEdit->setEnabled(false);
        ui->costSpinBox->setEnabled(false);
        ui->backgroundColorButton->setEnabled(false);
        ui->limitRedemptionsPerStreamCheckBox->setEnabled(false);
        ui->limitRedemptionsPerStreamSpinBox->setEnabled(false);
        ui->limitRedemptionsPerUserPerStreamCheckBox->setEnabled(false);
        ui->limitRedemptionsPerUserPerStreamSpinBox->setEnabled(false);
        ui->enableGlobalCooldownCheckBox->setEnabled(false);
        ui->enableGlobalCooldownSpinBox->setEnabled(false);
        ui->deleteButton->setEnabled(false);
    }
}

void EditRewardDialog::showAddRewardUi() {
    setWindowTitle(obs_module_text("AddReward"));
    ui->cannotEditRewardLabel->hide();
    ui->enabledCheckBox->setChecked(true);
    ui->deleteButton->setEnabled(false);
}

void EditRewardDialog::showUploadCustomIconLabel(const std::optional<std::string>& username) {
    if (!username.has_value()) {
        return;
    }
    std::string uploadCustomIconLink = std::format(
        "<a href=\"https://dashboard.twitch.tv/u/{}/viewer-rewards/channel-points/rewards\">{}</a>",
        username.value(),
        obs_module_text("UploadCustomIconHere")
    );
    ui->uploadCustomIconLabel->setText(QString::fromStdString(uploadCustomIconLink));
}

void EditRewardDialog::saveReward() {}

void EditRewardDialog::deleteReward() {}
