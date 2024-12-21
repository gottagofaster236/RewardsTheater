// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "OnTopDialog.h"

#ifdef __APPLE__
constexpr Qt::WindowFlags additionalWindowFlags = Qt::Tool;
#else
constexpr Qt::WindowFlags additionalWindowFlags{};
#endif

OnTopDialog::OnTopDialog(QWidget* parent, Qt::WindowFlags f) : QDialog(parent, f | additionalWindowFlags) {}

void OnTopDialog::showAndActivate() {
    show();
    activateWindow();
    raise();
}
