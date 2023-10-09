// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "ErrorMessageBox.h"

#include <obs-module.h>

ErrorMessageBox::ErrorMessageBox(QWidget* parent)
    : QMessageBox(QMessageBox::Warning, obs_module_text("RewardsTheater"), "", QMessageBox::Ok, parent) {}

void ErrorMessageBox::show(const std::string& message) {
    setText(QString::fromStdString(message));
    QMessageBox::show();
}
