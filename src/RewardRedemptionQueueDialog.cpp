// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "RewardRedemptionQueueDialog.h"

#include "RewardRedemptionWidget.h"
#include "ui_RewardRedemptionQueueDialog.h"

RewardRedemptionQueueDialog::RewardRedemptionQueueDialog(RewardRedemptionQueue& rewardRedemptionQueue, QWidget* parent)
    : OnTopDialog(parent), rewardRedemptionQueue(rewardRedemptionQueue),
      ui(std::make_unique<Ui::RewardRedemptionQueueDialog>()) {
    ui->setupUi(this);
    ui->rewardRedemptionsLayout->setAlignment(Qt::AlignTop);

    connect(
        &rewardRedemptionQueue,
        &RewardRedemptionQueue::onRewardRedemptionQueueUpdated,
        this,
        &RewardRedemptionQueueDialog::showRewardRedemptions,
        Qt::QueuedConnection
    );
    connect(ui->closeButton, &QPushButton::clicked, this, &RewardRedemptionQueueDialog::close);

    showRewardRedemptions(rewardRedemptionQueue.getRewardRedemptionQueue());
}

RewardRedemptionQueueDialog::~RewardRedemptionQueueDialog() = default;

void RewardRedemptionQueueDialog::showRewardRedemptions(const std::vector<RewardRedemption>& rewardRedemptions) {
    for (int i = 0; i < ui->rewardRedemptionsLayout->count(); i++) {
        ui->rewardRedemptionsLayout->itemAt(i)->widget()->deleteLater();
    }

    for (const auto& rewardRedemption : rewardRedemptions) {
        auto rewardRedemptionWidget = new RewardRedemptionWidget(rewardRedemption, this);
        connect(
            rewardRedemptionWidget,
            &RewardRedemptionWidget::onRewardRedemptionRemoved,
            &rewardRedemptionQueue,
            &RewardRedemptionQueue::removeRewardRedemption
        );
        ui->rewardRedemptionsLayout->addWidget(rewardRedemptionWidget);
    }
}
