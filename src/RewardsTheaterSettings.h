// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <memory>

namespace Ui {
class RewardsTheaterSettings;
}

class RewardsTheaterSettings : public QDialog {
    Q_OBJECT

public:
    explicit RewardsTheaterSettings(QWidget *parent = nullptr);
	~RewardsTheaterSettings();

private:
    std::unique_ptr<Ui::RewardsTheaterSettings> ui;
};
