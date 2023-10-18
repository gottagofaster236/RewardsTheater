// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "RewardRedemptionWidget.h"

#include "RewardWidget.h"
#include "ui_RewardRedemptionWidget.h"

RewardRedemptionWidget::RewardRedemptionWidget(const RewardRedemption& rewardRedemption, QWidget* parent)
    : QWidget(parent), rewardRedemption(rewardRedemption), ui(std::make_unique<Ui::RewardRedemptionWidget>()) {
    ui->setupUi(this);
    ui->titleLabel->setText(QString::fromStdString(rewardRedemption.reward.title));
    connect(ui->deleteButton, &QToolButton::clicked, this, &RewardRedemptionWidget::emitRewardRedemptionRemoved);
}

RewardRedemptionWidget::~RewardRedemptionWidget() = default;

void RewardRedemptionWidget::emitRewardRedemptionRemoved() {
    emit onRewardRedemptionRemoved(rewardRedemption);
}
