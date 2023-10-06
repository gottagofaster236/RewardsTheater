// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <QMessageBox>
#include <map>
#include <memory>
#include <variant>

#include "AuthenticateWithTwitchDialog.h"
#include "RewardWidget.h"
#include "RewardsTheaterPlugin.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(RewardsTheaterPlugin& plugin, QWidget* parent);
    ~SettingsDialog();

public slots:
    void toggleVisibility();
    void showRewards(const std::variant<std::exception_ptr, std::vector<Reward>>& newRewards);

private slots:
    void logInOrLogOut();
    void openRewardsQueue();
    void updateRewards();
    void updateAuthButtonText(const std::optional<std::string>& username);
    void showRewardTheaterLink();

private:
    void updateRewardWidgets();
    void showRewardWidgets();
    void showRewardLoadException(std::exception_ptr exception);

private:
    RewardsTheaterPlugin& plugin;
    std::unique_ptr<Ui::SettingsDialog> ui;
    AuthenticateWithTwitchDialog* authenticateWithTwitchDialog;
    QMessageBox* errorMessageBox;

    std::vector<Reward> rewards;
    std::map<std::string, RewardWidget*> rewardWidgetByRewardId;
    bool updateAvailable = true;  // TODO make false
};
