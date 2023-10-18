// SPDX-License-Identifier: GPL-3.0-only
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
