// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QMessageBox>
#include <string>

class ErrorMessageBox : public QMessageBox {
    Q_OBJECT

public:
    ErrorMessageBox(QWidget* parent);
    void show(const std::string& message);
};
