// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QMessageBox>
#include <QPointer>
#include <QWidget>
#include <exception>
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

    bool eventFilter(QObject* obj, QEvent* event) override;

public slots:
    void showImage(const std::string& imageBytes);
    void showDeleteRewardResult(std::exception_ptr error);

private slots:
    void deleteReward();

private:
    void showReward();
    void showEditRewardDialog();

private:
    Reward reward;
    TwitchRewardsApi& twitchRewardsApi;

    std::unique_ptr<Ui::RewardWidget> ui;
    QPointer<EditRewardDialog> editRewardDialog;
    QMessageBox* errorMessageBox;
};
