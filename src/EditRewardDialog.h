// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <memory>

#include "Reward.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class EditRewardDialog;
}

class EditRewardDialog : public QDialog {
    Q_OBJECT

public:
    EditRewardDialog(const std::optional<std::string>& rewardId, TwitchRewardsApi& twitchRewardsApi, QWidget* parent);
    ~EditRewardDialog();

private:
    std::optional<std::string> rewardId;
    TwitchRewardsApi& twitchRewardsApi;
    std::unique_ptr<Ui::EditRewardDialog> ui;
};
