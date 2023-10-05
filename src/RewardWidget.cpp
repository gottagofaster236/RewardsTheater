// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardWidget.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QBuffer>
#include <QImageReader>
#include <QPixmap>
#include <string>

#include "Log.h"
#include "ui_RewardWidget.h"

RewardWidget::RewardWidget(const Reward& reward, TwitchRewardsApi& twitchRewardsApi, QWidget* parent)
    : QWidget(parent), reward(reward), twitchRewardsApi(twitchRewardsApi), ui(std::make_unique<Ui::RewardWidget>()),
      editRewardDialog(nullptr) {
    ui->setupUi(this);
    setFixedSize(size());

    ui->deleteButton->hide();
    ui->costAndImageFrame->installEventFilter(this);
    showReward();
}

RewardWidget::~RewardWidget() = default;

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

void RewardWidget::showReward() {
    ui->costLabel->setText(QString::number(reward.cost));
    ui->titleLabel->setText(QString::fromStdString(reward.title));
    twitchRewardsApi.downloadImage(reward, this, "showImage");
}

void RewardWidget::showEditRewardDialog() {
    if (editRewardDialog == nullptr) {
        obs_frontend_push_ui_translation(obs_module_get_string);
        editRewardDialog = new EditRewardDialog(reward.id, twitchRewardsApi, this);
        editRewardDialog->setAttribute(Qt::WA_DeleteOnClose);
        obs_frontend_pop_ui_translation();
    }
    editRewardDialog->show();
}
