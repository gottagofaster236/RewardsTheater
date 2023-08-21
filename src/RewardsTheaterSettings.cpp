// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardsTheaterSettings.h"

#include "ui_RewardsTheaterSettings.h"

RewardsTheaterSettings::RewardsTheaterSettings(QWidget *parent)
    : QDialog(parent), ui(std::make_unique<Ui::RewardsTheaterSettings>()) {
    ui->setupUi(this);
}

RewardsTheaterSettings::~RewardsTheaterSettings() {}
