// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QSpinBox>
#include <exception>
#include <memory>
#include <optional>
#include <string>

#include "ConfirmDeleteReward.h"
#include "Reward.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class EditRewardDialog;
}

class EditRewardDialog : public QDialog {
    Q_OBJECT

public:
    EditRewardDialog(
        const std::optional<Reward>& originalReward,
        TwitchAuth& twitchAuth,
        TwitchRewardsApi& twitchRewardsApi,
        QWidget* parent
    );
    ~EditRewardDialog();

signals:
    void onRewardSaved(const Reward& reward);
    void onRewardDeleted();

private slots:
    void showUploadCustomIconLabel(const std::optional<std::string>& username);
    void showSelectedColor(QColor newSelectedBackgroundColor);
    void showColorDialog();
    void saveReward();
    void showSaveRewardResult(std::variant<std::exception_ptr, Reward> reward);

private:
    void showReward(const Reward& reward);
    void showSelectedColor(Color newSelectedBackgroundColor);
    void createConfirmDeleteReward(const Reward& reward);
    void disableInput();
    void showAddReward();

    RewardData getRewardData();
    std::optional<std::int64_t> getOptionalSetting(QCheckBox* checkBox, QSpinBox* spinBox);

    const std::optional<Reward> originalReward;
    TwitchAuth& twitchAuth;
    TwitchRewardsApi& twitchRewardsApi;
    std::unique_ptr<Ui::EditRewardDialog> ui;
    QColorDialog* colorDialog;
    ConfirmDeleteReward* confirmDeleteReward;
    ErrorMessageBox* errorMessageBox;

    Color selectedColor;
};
