// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>

class OnTopDialog : public QDialog {
public:
    explicit OnTopDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public slots:
    void showAndActivate();
};
