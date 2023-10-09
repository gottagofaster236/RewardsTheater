// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QPointer>
#include <QWidget>
#include <exception>
#include <memory>

#include "EditRewardDialog.h"
#include "Reward.h"
#include "TwitchAuth.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class RewardWidget;
}

class RewardWidget : public QWidget {
    Q_OBJECT

public:
    RewardWidget(const Reward& reward, TwitchAuth& twitchApi, TwitchRewardsApi& twitchRewardsApi, QWidget* parent);
    ~RewardWidget();

    bool eventFilter(QObject* obj, QEvent* event) override;

    const Reward& getReward() const;

signals:
    void onRewardDeleted();

public slots:
    void setReward(const Reward& newReward);

private slots:
    void showImage(const std::string& imageBytes);
    void deleteReward();
    void showDeleteRewardResult(std::exception_ptr error);
    void emitRewardDeletedAndDeleteWidget();

private:
    void showReward();
    void showEditRewardDialog();

private:
    Reward reward;
    TwitchAuth& twitchAuth;
    TwitchRewardsApi& twitchRewardsApi;

    std::unique_ptr<Ui::RewardWidget> ui;
    QPointer<EditRewardDialog> editRewardDialog;
    ErrorMessageBox* errorMessageBox;
};
