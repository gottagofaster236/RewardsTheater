// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardWidget.h"

#include <obs-module.h>

#include <QBuffer>
#include <QImageReader>
#include <QPixmap>
#include <format>
#include <string>

#include "HttpClient.h"
#include "Log.h"
#include "ui_RewardWidget.h"

RewardWidget::RewardWidget(
    const Reward& reward,
    TwitchAuth& twitchAuth,
    TwitchRewardsApi& twitchRewardsApi,
    QWidget* parent
)
    : QWidget(parent), reward(reward), twitchAuth(twitchAuth), twitchRewardsApi(twitchRewardsApi),
      ui(std::make_unique<Ui::RewardWidget>()), editRewardDialog(nullptr), errorMessageBox(nullptr) {
    ui->setupUi(this);
    setFixedSize(size());

    ui->deleteButton->hide();
    ui->costAndImageFrame->installEventFilter(this);
    showReward();

    connect(ui->deleteButton, &QToolButton::clicked, this, &RewardWidget::deleteReward);
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

void RewardWidget::deleteReward() {
    twitchRewardsApi.deleteReward(reward, this, "showDeleteRewardResult");
}

void RewardWidget::showDeleteRewardResult(std::exception_ptr error) {
    if (error == nullptr) {
        // Deletion was successful.
        deleteLater();
        return;
    }
    std::string message;
    try {
        std::rethrow_exception(error);
    } catch (const TwitchRewardsApi::NotManageableRewardException&) {
        message = obs_module_text("CouldNotDeleteRewardNotManageable");
    } catch (const HttpClient::NetworkException&) {
        message = obs_module_text("CouldNotDeleteRewardNetwork");
    } catch (const std::exception& otherException) {
        message =
            std::vformat(obs_module_text("CouldNotDeleteRewardOther"), std::make_format_args(otherException.what()));
    }

    if (!errorMessageBox) {
        errorMessageBox =
            new QMessageBox(QMessageBox::Warning, obs_module_text("RewardsTheater"), "", QMessageBox::Ok, this);
    }
    errorMessageBox->setText(QString::fromStdString(message));
    errorMessageBox->show();
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
    twitchRewardsApi.downloadImage(reward, this, "showImage");
}

void RewardWidget::showEditRewardDialog() {
    if (editRewardDialog == nullptr) {
        editRewardDialog = new EditRewardDialog(reward, twitchAuth, twitchRewardsApi, this);
    }
    editRewardDialog->show();
}
