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

    ui->deleteButton->setVisible(false);
    ui->costAndImageFrame->installEventFilter(this);

    ui->costLabel->setText(QString::number(reward.cost));
    ui->titleLabel->setText(QString::fromStdString(reward.title));
    twitchRewardsApi.downloadImage(reward, this, "showImage");
}

RewardWidget::~RewardWidget() = default;

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
            ui->deleteButton->setVisible(true);
        } else if (event->type() == QEvent::Leave) {
            ui->deleteButton->setVisible(false);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            showEditRewardDialog();
        }
    }
    return false;
}

void RewardWidget::showEditRewardDialog() {
    if (editRewardDialog == nullptr) {
        obs_frontend_push_ui_translation(obs_module_get_string);
        editRewardDialog = new EditRewardDialog(reward.id, twitchRewardsApi, this);
        obs_frontend_pop_ui_translation();
    }
    editRewardDialog->setVisible(true);
}
