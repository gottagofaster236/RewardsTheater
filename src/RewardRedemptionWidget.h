// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QWidget>
#include <memory>

#include "Reward.h"

namespace Ui {
class RewardRedemptionWidget;
}

class RewardRedemptionWidget : public QWidget {
    Q_OBJECT

public:
    RewardRedemptionWidget(const RewardRedemption& rewardRedemption, QWidget* parent);
    ~RewardRedemptionWidget();

signals:
    void onRewardRedemptionRemoved(const RewardRedemption& rewardRedemption);

private slots:
    void emitRewardRedemptionRemoved();

private:
    RewardRedemption rewardRedemption;
    std::unique_ptr<Ui::RewardRedemptionWidget> ui;
};
