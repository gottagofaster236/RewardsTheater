// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "RewardWidget.h"

#include <fmt/core.h>
#include <obs-module.h>

#include <QBuffer>
#include <QImageReader>
#include <QPixmap>
#include <string>

#include "HttpClient.h"
#include "Log.h"
#include "ui_RewardWidget.h"

RewardWidget::RewardWidget(
    const Reward& reward,
    TwitchAuth& twitchAuth,
    TwitchRewardsApi& twitchRewardsApi,
    RewardRedemptionQueue& rewardRedemptionQueue,
    Settings& settings,
    QWidget* parent
)
    : QWidget(parent), reward(reward), twitchAuth(twitchAuth), twitchRewardsApi(twitchRewardsApi),
      rewardRedemptionQueue(rewardRedemptionQueue), settings(settings), ui(std::make_unique<Ui::RewardWidget>()),
      editRewardDialog(nullptr), confirmDeleteReward(new ConfirmDeleteReward(reward, twitchRewardsApi, this)) {
    ui->setupUi(this);
    setFixedSize(size());

    ui->deleteButton->hide();
    ui->costAndImageFrame->installEventFilter(this);
    showReward();

    connect(
        ui->deleteButton, &QToolButton::clicked, confirmDeleteReward, &ConfirmDeleteReward::showConfirmDeleteMessageBox
    );
    connect(confirmDeleteReward, &ConfirmDeleteReward::onRewardDeleted, this, &RewardWidget::onRewardDeleted);
}

RewardWidget::~RewardWidget() = default;

bool RewardWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == ui->costAndImageFrame) {
        if (event->type() == QEvent::Enter) {
            ui->deleteButton->show();
        } else if (event->type() == QEvent::Leave) {
            ui->deleteButton->hide();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            showEditRewardDialog();
        }
    }
    return false;
}

const Reward& RewardWidget::getReward() const {
    return reward;
}

void RewardWidget::setReward(const Reward& newReward) {
    if (reward == newReward) {
        return;
    }
    reward = newReward;
    showReward();
}

void RewardWidget::showImage(const std::string& imageBytes) {
    QBuffer imageBuffer;
    imageBuffer.setData(imageBytes.data(), static_cast<int>(imageBytes.size()));

    QImageReader imageReader(&imageBuffer, "png");
    QImage image = imageReader.read();
    if (image.isNull()) {
        log(LOG_ERROR, "Could not read image: {}", imageReader.errorString().toStdString());
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
}

void RewardWidget::showReward() {
    ui->costLabel->setText(QString::number(reward.cost));
    ui->titleLabel->setText(QString::fromStdString(reward.title));
    std::string backgroundColorStyle = fmt::format("QFrame {{ background: {} }}", reward.backgroundColor.toHex());
    ui->costAndImageFrame->setStyleSheet(QString::fromStdString(backgroundColorStyle));
    twitchRewardsApi.downloadImage(reward, this, "showImage");
}

void RewardWidget::showEditRewardDialog() {
    if (!editRewardDialog) {
        editRewardDialog =
            new EditRewardDialog(reward, twitchAuth, twitchRewardsApi, rewardRedemptionQueue, settings, this);
        connect(editRewardDialog, &EditRewardDialog::onRewardSaved, this, &RewardWidget::setReward);
        connect(editRewardDialog, &EditRewardDialog::onRewardDeleted, this, &RewardWidget::onRewardDeleted);
    }
    editRewardDialog->showAndActivate();
}
