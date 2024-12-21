// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <map>
#include <memory>
#include <variant>

#include "ErrorMessageBox.h"
#include "OnTopDialog.h"
#include "RewardRedemptionQueueDialog.h"
#include "RewardWidget.h"
#include "RewardsTheaterPlugin.h"
#include "TwitchAuthDialog.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public OnTopDialog {
    Q_OBJECT

public:
    SettingsDialog(RewardsTheaterPlugin& plugin, QWidget* parent);
    ~SettingsDialog() override;

private slots:
    void logInOrLogOut();
    void updateAuthButtonText(const std::optional<std::string>& username);
    void showRewards(const std::variant<std::exception_ptr, std::vector<Reward>>& newRewards);
    void addReward(const Reward& reward);
    void removeReward(const std::string& id);
    void showAddRewardDialog();
    void showUpdateAvailableLink();
    void setRewardPlaybackPaused(int checkState);
    void saveRewardRedemptionQueueEnabled(int checkState);
    void saveIntervalBetweenRewards(double interval);
    void openRewardRedemptionQueue();

private:
    void showRewards();
    void updateRewardWidgets();
    void showRewardWidgets();
    void showRewardLoadException(std::exception_ptr exception);
    void showGithubLink();
    void showRewardsTheaterLink(
        const std::string& linkText,
        const std::string& url,
        std::optional<std::string> linkColor = {}
    );

    RewardsTheaterPlugin& plugin;
    std::unique_ptr<Ui::SettingsDialog> ui;
    TwitchAuthDialog* twitchAuthDialog;
    RewardRedemptionQueueDialog* rewardRedemptionQueueDialog;
    ErrorMessageBox* errorMessageBox;

    std::vector<Reward> rewards;
    std::map<std::string, RewardWidget*> rewardWidgetByRewardId;
};
