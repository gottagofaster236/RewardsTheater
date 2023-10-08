// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <QMessageBox>
#include <chrono>
#include <memory>
#include <thread>

#include "TwitchAuth.h"

namespace Ui {
class TwitchAuthDialog;
}

class TwitchAuthDialog : public QDialog {
    Q_OBJECT

public:
    TwitchAuthDialog(QWidget* parent, TwitchAuth& twitchAuth);

    ~TwitchAuthDialog();

private slots:
    void authenticateWithAccessToken();
    void showAuthenticationFailureMessage(std::exception_ptr reason);
    void showAccessTokenAboutToExpireMessage(std::chrono::seconds expiresIn);

private:
    void showAuthenticationMessage(const std::string& message);
    void showOurselvesAfterAuthMessageBox();

private:
    TwitchAuth& twitchAuth;
    std::unique_ptr<Ui::TwitchAuthDialog> ui;
    QMessageBox* authMessageBox;
};
