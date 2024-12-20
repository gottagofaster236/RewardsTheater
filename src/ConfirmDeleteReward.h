// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include <QAbstractButton>
#include <QObject>
#include <QPointer>

#include "ErrorMessageBox.h"
#include "Reward.h"
#include "TwitchRewardsApi.h"

#pragma once

class ConfirmDeleteReward : public QObject {
    Q_OBJECT

public:
    ConfirmDeleteReward(const Reward& reward, TwitchRewardsApi& twitchRewardsApi, QWidget* parent);
    ~ConfirmDeleteReward() override;

public slots:
    void showConfirmDeleteMessageBox();

signals:
    void onRewardDeleted();

private slots:
    void deleteRewardIfYesClicked(QAbstractButton* buttonClicked);
    void showDeleteRewardResult(std::exception_ptr result);

private:
    Reward reward;
    TwitchRewardsApi& twitchRewardsApi;
    QWidget* parent;
    // Not owned by us, therefore using a QPointer.
    QPointer<ErrorMessageBox> errorMessageBox;
};
