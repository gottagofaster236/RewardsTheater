// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <memory>
#include <optional>
#include <string>

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
        const std::optional<Reward>& reward,
        TwitchAuth& twitchAuth,
        TwitchRewardsApi& twitchRewardsApi,
        QWidget* parent
    );
    ~EditRewardDialog();

private:
    void showReward(const Reward& reward);
    void showAddRewardUi();
    void showUploadCustomIconLabel(const std::optional<std::string>& username);

    void saveReward();
    void deleteReward();

    std::optional<std::string> rewardId;
    TwitchAuth& twitchAuth;
    TwitchRewardsApi& twitchRewardsApi;
    std::unique_ptr<Ui::EditRewardDialog> ui;
};
