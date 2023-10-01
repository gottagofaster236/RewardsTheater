// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardWidget.h"

#include <string>

#include "ui_RewardWidget.h"

RewardWidget::RewardWidget(const Reward& reward, TwitchRewardsApi& twitchRewardsApi, QWidget* parent)
    : QWidget(parent), twitchRewardsApi(twitchRewardsApi), ui(std::make_unique<Ui::RewardWidget>()) {
    ui->setupUi(this);
    setFixedSize(size());

    ui->costLabel->setText(QString::number(reward.cost));
    ui->titleLabel->setText(QString::fromStdString(reward.title));
    // TODO load icon from rewards api asynchronously
}

RewardWidget::~RewardWidget() = default;
