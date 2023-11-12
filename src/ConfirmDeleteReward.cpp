// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "ConfirmDeleteReward.h"

#include <fmt/core.h>
#include <obs-module.h>

ConfirmDeleteReward::ConfirmDeleteReward(const Reward& reward, TwitchRewardsApi& twitchRewardsApi, QWidget* parent)
    : QObject(parent), reward(reward), twitchRewardsApi(twitchRewardsApi), parent(parent),
      errorMessageBox(new ErrorMessageBox(parent)) {
    connect(errorMessageBox, &QMessageBox::buttonClicked, this, &ConfirmDeleteReward::deleteRewardIfYesClicked);
}

ConfirmDeleteReward::~ConfirmDeleteReward() {
    if (errorMessageBox) {
        errorMessageBox->deleteLater();
    }
}

void ConfirmDeleteReward::showConfirmDeleteMessageBox() {
    if (!reward.canManage) {
        showDeleteRewardResult(std::make_exception_ptr(TwitchRewardsApi::NotManageableRewardException()));
        return;
    }
    if (errorMessageBox) {
        errorMessageBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        errorMessageBox->button(QMessageBox::Yes)->setText(obs_module_text("Yes"));
        errorMessageBox->button(QMessageBox::No)->setText(obs_module_text("No"));
        errorMessageBox->show(obs_module_text("ConfirmDeleteReward"));
    }
}

void ConfirmDeleteReward::deleteRewardIfYesClicked(QAbstractButton* buttonClicked) {
    if (errorMessageBox && errorMessageBox->standardButton(buttonClicked) == QMessageBox::Yes) {
        twitchRewardsApi.deleteReward(reward, this, "showDeleteRewardResult");
    }
}

void ConfirmDeleteReward::showDeleteRewardResult(std::exception_ptr result) {
    if (result == nullptr) {
        // Deletion was successful.
        emit onRewardDeleted();
        return;
    }

    if (!errorMessageBox) {
        return;
    }

    std::string message;
    try {
        std::rethrow_exception(result);
    } catch (const TwitchRewardsApi::NotManageableRewardException&) {
        message = obs_module_text("CouldNotDeleteRewardNotManageable");
    } catch (const HttpClient::NetworkException&) {
        message = obs_module_text("CouldNotDeleteRewardNetwork");
    } catch (const std::exception& otherException) {
        message = fmt::format(fmt::runtime(obs_module_text("CouldNotDeleteRewardOther")), otherException.what());
    }
    errorMessageBox->setStandardButtons(QMessageBox::Ok);
    errorMessageBox->show(message);
}
