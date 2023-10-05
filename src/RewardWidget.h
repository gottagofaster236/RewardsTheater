// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QWidget>
#include <memory>

#include "EditRewardDialog.h"
#include "Reward.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class RewardWidget;
}

class RewardWidget : public QWidget {
    Q_OBJECT

public:
    RewardWidget(const Reward& reward, TwitchRewardsApi& twitchRewardsApi, QWidget* parent);
    ~RewardWidget();

    const Reward& getReward() const;
    void setReward(const Reward& newReward);

    Q_INVOKABLE void showImage(const std::string& imageBytes);

    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void showReward();
    void showEditRewardDialog();

private:
    Reward reward;
    TwitchRewardsApi& twitchRewardsApi;

    std::unique_ptr<Ui::RewardWidget> ui;
    EditRewardDialog* editRewardDialog;
};
