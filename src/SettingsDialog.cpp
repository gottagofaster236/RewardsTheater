// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "SettingsDialog.h"

#include <obs-module.h>
#include <obs.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <obs.hpp>
#include <ranges>

#include "HttpClient.h"
#include "Log.h"
#include "RewardWidget.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(RewardsTheaterPlugin& plugin, QWidget* parent)
    : QDialog(parent), plugin(plugin), ui(std::make_unique<Ui::SettingsDialog>()),
      twitchAuthDialog(new TwitchAuthDialog(this, plugin.getTwitchAuth())), errorMessageBox(new ErrorMessageBox(this)) {
    ui->setupUi(this);
    showGithubLink();

    connect(ui->authButton, &QPushButton::clicked, this, &SettingsDialog::logInOrLogOut);
    connect(ui->openRewardsQueueButton, &QPushButton::clicked, this, &SettingsDialog::openRewardsQueue);
    connect(
        ui->reloadRewardsButton, &QPushButton::clicked, &plugin.getTwitchRewardsApi(), &TwitchRewardsApi::reloadRewards
    );
    connect(ui->addRewardButton, &QPushButton::clicked, this, &SettingsDialog::showAddRewardDialog);

    connect(&plugin.getTwitchAuth(), &TwitchAuth::onUsernameChanged, this, &SettingsDialog::updateAuthButtonText);
    connect(
        &plugin.getTwitchRewardsApi(),
        &TwitchRewardsApi::onRewardsUpdated,
        this,
        qOverload<const std::variant<std::exception_ptr, std::vector<Reward>>&>(&SettingsDialog::showRewards)
    );
    connect(
        &plugin.getGithubUpdateApi(),
        &GithubUpdateApi::onUpdateAvailable,
        this,
        &SettingsDialog::showUpdateAvailableLink
    );
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::toggleVisibility() {
    setVisible(!isVisible());
}

void SettingsDialog::logInOrLogOut() {
    TwitchAuth& auth = plugin.getTwitchAuth();
    if (auth.isAuthenticated()) {
        auth.logOut();
    } else {
        twitchAuthDialog->show();
    }
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

void SettingsDialog::showRewards(const std::variant<std::exception_ptr, std::vector<Reward>>& newRewards) {
    if (std::holds_alternative<std::vector<Reward>>(newRewards)) {
        rewards = std::get<std::vector<Reward>>(newRewards);
    } else {
        rewards = {};
        showRewardLoadException(std::get<std::exception_ptr>(newRewards));
    }
    showRewards();
}

void SettingsDialog::addReward(const Reward& reward) {
    rewards.push_back(reward);
    showRewards();
}

void SettingsDialog::removeReward(const std::string& id) {
    std::erase_if(rewards, [&id](const Reward& reward) {
        return reward.id == id;
    });
    showRewards();
}

void SettingsDialog::showAddRewardDialog() {
    EditRewardDialog* editRewardDialog =
        new EditRewardDialog({}, plugin.getTwitchAuth(), plugin.getTwitchRewardsApi(), this);
    QObject::connect(editRewardDialog, &EditRewardDialog::onRewardSaved, this, &SettingsDialog::addReward);
    editRewardDialog->show();
}

void SettingsDialog::openRewardsQueue() {
    log(LOG_INFO, "onOpenRewardsQueueClicked");
}

void SettingsDialog::showUpdateAvailableLink() {
    showRewardsTheaterLink(
        obs_module_text("UpdateAvailable"), "https://github.com/gottagofaster236/RewardsTheater/releases/latest"
    );
}

void SettingsDialog::showRewards() {
    updateRewardWidgets();
    showRewardWidgets();
}

void SettingsDialog::updateRewardWidgets() {
    auto rewardIdsView = rewards | std::views::transform(&Reward::id);
    std::set<std::string> rewardIds(rewardIdsView.begin(), rewardIdsView.end());

    // Delete widgets for rewards that no longer exist.
    // We can't reuse them, because they may have a child EditRewardDialog.
    for (auto it = rewardWidgetByRewardId.begin(); it != rewardWidgetByRewardId.end();) {
        const std::string& rewardId = it->first;
        RewardWidget* rewardWidget = it->second;
        if (!rewardIds.contains(rewardId)) {
            if (rewardWidget) {
                rewardWidget->deleteLater();
            }
            it = rewardWidgetByRewardId.erase(it);
        } else {
            ++it;
        }
    }

    for (const Reward& reward : rewards) {
        RewardWidget* existingWidget = rewardWidgetByRewardId[reward.id];
        if (existingWidget) {
            existingWidget->setReward(reward);
        } else {
            RewardWidget* rewardWidget =
                new RewardWidget(reward, plugin.getTwitchAuth(), plugin.getTwitchRewardsApi(), ui->rewardsGrid);
            const std::string& id = reward.id;
            QObject::connect(rewardWidget, &RewardWidget::onRewardDeleted, this, [this, id]() {
                removeReward(id);
            });
            rewardWidgetByRewardId[reward.id] = rewardWidget;
        }
    }
}

void SettingsDialog::showRewardWidgets() {
    std::ranges::stable_sort(rewards, {}, [](const Reward& reward) {
        return reward.cost;
    });

    const int REWARDS_PER_ROW = 4;
    for (int i = 0; i < static_cast<int>(rewards.size()); i++) {
        RewardWidget* rewardWidget = rewardWidgetByRewardId[rewards[i].id];
        ui->rewardsGridLayout->addWidget(rewardWidget, i / REWARDS_PER_ROW, i % REWARDS_PER_ROW);
    }
}

void SettingsDialog::showRewardLoadException(std::exception_ptr exception) {
    std::string message;
    try {
        std::rethrow_exception(exception);
    } catch (const TwitchAuth::UnauthenticatedException&) {
        // It's going to be shown by TwitchAuthDialog, anyway.
        return;
    } catch (const TwitchRewardsApi::NotAffiliateException&) {
        message = obs_module_text("CouldNotLoadRewardsNotAffiliate");
    } catch (const HttpClient::NetworkException&) {
        message = obs_module_text("CouldNotLoadRewardsNetwork");
    } catch (const std::exception& otherException) {
        message =
            std::vformat(obs_module_text("CouldNotLoadRewardsOther"), std::make_format_args(otherException.what()));
    }
    errorMessageBox->show(message);
}

void SettingsDialog::showGithubLink() {
    showRewardsTheaterLink(obs_module_text("GitHub"), "https://github.com/gottagofaster236/RewardsTheater");
}

void SettingsDialog::showRewardsTheaterLink(const std::string& linkText, const std::string& url) {
    std::string updateAvailableLink =
        std::format("{} <a href=\"{}\">{}</a>", obs_module_text("RewardsTheater"), url, linkText);
    ui->titleLabel->setText(QString::fromStdString(updateAvailableLink));
}
