// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "EditRewardDialog.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <format>

#include "HttpClient.h"
#include "ui_EditRewardDialog.h"

EditRewardDialog::EditRewardDialog(
    const std::optional<Reward>& originalReward,
    TwitchAuth& twitchAuth,
    TwitchRewardsApi& twitchRewardsApi,
    QWidget* parent
)
    : QDialog(parent), originalReward(originalReward), twitchAuth(twitchAuth), twitchRewardsApi(twitchRewardsApi),
      ui(std::make_unique<Ui::EditRewardDialog>()), colorDialog(nullptr), errorMessageBox(nullptr) {
    obs_frontend_push_ui_translation(obs_module_get_string);
    ui->setupUi(this);
    obs_frontend_pop_ui_translation();
    setFixedSize(size());
    setAttribute(Qt::WA_DeleteOnClose);

    if (originalReward.has_value()) {
        showReward(originalReward.value());
    } else {
        showAddReward();
    }
    showUploadCustomIconLabel(twitchAuth.getUsername());

    connect(ui->saveButton, &QPushButton::clicked, this, &EditRewardDialog::saveReward);
    connect(ui->cancelButton, &QPushButton::clicked, this, &EditRewardDialog::close);
    connect(ui->deleteButton, &QPushButton::clicked, this, &EditRewardDialog::deleteReward);
    connect(ui->backgroundColorButton, &QPushButton::clicked, this, &EditRewardDialog::showColorDialog);
    connect(&twitchAuth, &TwitchAuth::onUsernameChanged, this, &EditRewardDialog::showUploadCustomIconLabel);
}

EditRewardDialog::~EditRewardDialog() = default;

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

void EditRewardDialog::showSelectedColor(Color newBackgroundColor) {
    selectedColor = newBackgroundColor;
    std::string backgroundColorStyle = std::format("background: {}", selectedColor.toHex());
    ui->backgroundColorButton->setStyleSheet(QString::fromStdString(backgroundColorStyle));
}

void EditRewardDialog::showColorDialog() {
    if (!colorDialog) {
        colorDialog = new QColorDialog({selectedColor.red, selectedColor.green, selectedColor.blue}, this);
        QObject::connect(
            colorDialog, &QColorDialog::colorSelected, this, qOverload<QColor>(&EditRewardDialog::showSelectedColor)
        );
    }
    colorDialog->show();
}

void EditRewardDialog::saveReward() {
    RewardData rewardData = getRewardData();
    if (!originalReward.has_value()) {
        twitchRewardsApi.createReward(rewardData, this, "showSaveRewardResult");
    } else if (rewardData != static_cast<const RewardData&>(originalReward.value())) {
        Reward newReward(originalReward.value(), rewardData);
        twitchRewardsApi.updateReward(newReward, this, "showSaveRewardResult");
    }
    // TODO also save the OBS source from combobox!
}

void EditRewardDialog::deleteReward() {
    if (!originalReward.has_value()) {
        return;
    }
    twitchRewardsApi.deleteReward(originalReward.value(), this, "showDeleteRewardResult");
}

void EditRewardDialog::showSaveRewardResult(std::variant<std::exception_ptr, Reward> reward) {
    if (std::holds_alternative<Reward>(reward)) {
        emit onRewardSaved(std::get<Reward>(reward));
        close();
        return;
    }

    std::string message;
    try {
        std::rethrow_exception(std::get<std::exception_ptr>(reward));
    } catch (const TwitchRewardsApi::NotAffiliateException&) {
        message = obs_module_text("CouldNotSaveRewardNotAffiliate");
    } catch (const TwitchRewardsApi::NotManageableRewardException&) {
        message = obs_module_text("CouldNotSaveRewardNotManageable");
    } catch (const TwitchRewardsApi::EmptyRewardTitleException&) {
        message = obs_module_text("CouldNotSaveRewardEmptyTitle");
    } catch (const HttpClient::NetworkException&) {
        message = obs_module_text("CouldNotSaveRewardNetwork");
    } catch (const std::exception& exception) {
        message = std::vformat(obs_module_text("CouldNotSaveRewardOther"), std::make_format_args(exception.what()));
    }
    showErrorMessage(message);
}

void EditRewardDialog::showDeleteRewardResult(std::exception_ptr result) {
    if (result == nullptr) {
        // Success
        emit onRewardDeleted();
        close();
        return;
    }

    std::string message;
    try {
        std::rethrow_exception(result);
    } catch (const TwitchRewardsApi::NotManageableRewardException&) {
        message = obs_module_text("CouldNotDeleteRewardNotManageable");
    } catch (const HttpClient::NetworkException&) {
        message = obs_module_text("CouldNotDeleteRewardNetwork");
    } catch (const std::exception& exception) {
        message = std::vformat(obs_module_text("CouldNotDeleteRewardOther"), std::make_format_args(exception.what()));
    }
    showErrorMessage(message);
}

void EditRewardDialog::showReward(const Reward& reward) {
    ui->enabledCheckBox->setChecked(reward.isEnabled);
    ui->titleEdit->setText(QString::fromStdString(reward.title));
    ui->costSpinBox->setValue(reward.cost);
    showSelectedColor(reward.backgroundColor);
    ui->limitRedemptionsPerStreamCheckBox->setChecked(reward.maxRedemptionsPerStream.has_value());
    ui->limitRedemptionsPerStreamSpinBox->setValue(reward.maxRedemptionsPerStream.value_or(1));
    ui->limitRedemptionsPerUserPerStreamCheckBox->setChecked(reward.maxRedemptionsPerUserPerStream.has_value());
    ui->limitRedemptionsPerUserPerStreamSpinBox->setValue(reward.maxRedemptionsPerUserPerStream.value_or(1));
    ui->enableGlobalCooldownCheckBox->setChecked(reward.globalCooldownSeconds.has_value());
    ui->enableGlobalCooldownSpinBox->setValue(reward.globalCooldownSeconds.value_or(1));

    if (reward.canManage) {
        ui->cannotEditRewardLabel->hide();
    } else {
        disableInput();
    }
}

void EditRewardDialog::showSelectedColor(QColor newBackgroundColor) {
    showSelectedColor(Color(newBackgroundColor.red(), newBackgroundColor.green(), newBackgroundColor.blue()));
}

void EditRewardDialog::disableInput() {
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

void EditRewardDialog::showAddReward() {
    setWindowTitle(obs_module_text("AddReward"));
    ui->cannotEditRewardLabel->hide();
    ui->enabledCheckBox->setChecked(true);
    showSelectedColor(Color(209, 179, 255));
    ui->deleteButton->setEnabled(false);
}

RewardData EditRewardDialog::getRewardData() {
    return RewardData{
        ui->titleEdit->text().toStdString(),
        ui->costSpinBox->value(),
        ui->enabledCheckBox->isChecked(),
        selectedColor,
        getOptionalSetting(ui->limitRedemptionsPerStreamCheckBox, ui->limitRedemptionsPerStreamSpinBox),
        getOptionalSetting(ui->limitRedemptionsPerUserPerStreamCheckBox, ui->limitRedemptionsPerUserPerStreamSpinBox),
        getOptionalSetting(ui->enableGlobalCooldownCheckBox, ui->enableGlobalCooldownSpinBox),
    };
}

std::optional<std::int64_t> EditRewardDialog::getOptionalSetting(QCheckBox* checkBox, QSpinBox* spinBox) {
    if (checkBox->isChecked()) {
        return spinBox->value();
    } else {
        return {};
    }
}

void EditRewardDialog::showErrorMessage(const std::string& message) {
    if (!errorMessageBox) {
        errorMessageBox =
            new QMessageBox(QMessageBox::Warning, obs_module_text("RewardsTheater"), "", QMessageBox::Ok, this);
    }
    errorMessageBox->setText(QString::fromStdString(message));
    errorMessageBox->show();
}
