// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QPointer>
#include <QWidget>
#include <memory>

#include "ConfirmDeleteReward.h"
#include "EditRewardDialog.h"
#include "Reward.h"
#include "RewardRedemptionQueue.h"
#include "Settings.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class RewardWidget;
}

class RewardWidget : public QWidget {
    Q_OBJECT

public:
    RewardWidget(
        const Reward& reward,
        TwitchAuth& twitchApi,
        TwitchRewardsApi& twitchRewardsApi,
        RewardRedemptionQueue& rewardRedemptionQueue,
        Settings& settings,
        QWidget* parent
    );
    ~RewardWidget();

    bool eventFilter(QObject* obj, QEvent* event) override;

    const Reward& getReward() const;

signals:
    void onRewardDeleted();

public slots:
    void setReward(const Reward& newReward);

private slots:
    void showImage(const std::string& imageBytes);

private:
    void showReward();
    void showEditRewardDialog();

    Reward reward;
    TwitchAuth& twitchAuth;
    TwitchRewardsApi& twitchRewardsApi;
    RewardRedemptionQueue& rewardRedemptionQueue;
    Settings& settings;

    std::unique_ptr<Ui::RewardWidget> ui;
    QPointer<EditRewardDialog> editRewardDialog;
    ConfirmDeleteReward* confirmDeleteReward;
};
