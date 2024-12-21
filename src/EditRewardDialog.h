// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QCheckBox>
#include <QColorDialog>
#include <QEvent>
#include <QSpinBox>
#include <exception>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <variant>

#include "ConfirmDeleteReward.h"
#include "OnTopDialog.h"
#include "Reward.h"
#include "RewardRedemptionQueue.h"
#include "Settings.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class EditRewardDialog;
}

class EditRewardDialog : public OnTopDialog {
    Q_OBJECT

public:
    EditRewardDialog(
        const std::optional<Reward>& originalReward,
        TwitchAuth& twitchAuth,
        TwitchRewardsApi& twitchRewardsApi,
        RewardRedemptionQueue& rewardRedemptionQueue,
        Settings& settings,
        QWidget* parent
    );
    ~EditRewardDialog() override;

signals:
    void onRewardSaved(const Reward& reward);
    void onRewardDeleted();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void showUploadCustomIconLabel(const std::optional<std::string>& username);
    void showSelectedColor(QColor newSelectedBackgroundColor);
    void showColorDialog();
    void saveReward();
    void showSaveRewardResult(std::variant<std::exception_ptr, Reward> result);
    void updateObsSourceComboBox();
    void testObsSource();
    void showTestObsSourceException(std::exception_ptr exception);
    bool showLoopVideoNotSupportedErrorIfNeeded();

private:
    void showReward(const Reward& reward);
    void showSelectedColor(Color newSelectedBackgroundColor);
    void showGlobalCooldown(std::int64_t globalCooldownSeconds);
    void createConfirmDeleteReward(const Reward& reward);
    void disableInput();
    void showAddReward();
    Color chooseRandomColor();
    void showIcons();
    void showObsSourceComboBoxIcon();
    void showUpdateObsSourcesButtonIcon();
    bool shouldUseWhiteIcons();
    void setObsSourceName(const std::optional<std::string>& obsSourceName);
    std::optional<std::string> getObsSourceName();

    RewardData getRewardData();
    std::optional<std::int64_t> getOptionalSetting(QCheckBox* checkBox, QSpinBox* spinBox);
    std::optional<std::int64_t> getOptionalSetting(QCheckBox* checkBox, std::int64_t spinBoxValue);
    void saveLocalRewardSettings(const std::string& rewardId);
    SourcePlaybackSettings getSourcePlaybackSettings();

    const std::optional<Reward> originalReward;
    TwitchAuth& twitchAuth;
    TwitchRewardsApi& twitchRewardsApi;
    RewardRedemptionQueue& rewardRedemptionQueue;
    Settings& settings;
    std::unique_ptr<Ui::EditRewardDialog> ui;
    QColorDialog* colorDialog;
    ConfirmDeleteReward* confirmDeleteReward;
    ErrorMessageBox* errorMessageBox;

    Color selectedColor;
    std::default_random_engine randomEngine;
};
