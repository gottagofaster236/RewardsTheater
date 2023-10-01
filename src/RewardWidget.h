// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QWidget>
#include <memory>

#include "Reward.h"
#include "TwitchRewardsApi.h"

namespace Ui {
class RewardWidget;
}

class RewardWidget : public QWidget {
    Q_OBJECT

public:
    RewardWidget(const Reward& reward, TwitchRewardsApi& twitchRewardsApi, QWidget* parent = nullptr);
    ~RewardWidget();

private:
    TwitchRewardsApi& twitchRewardsApi;
    std::unique_ptr<Ui::RewardWidget> ui;
};
