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
class AuthenticateWithTwitchDialog;
}

class AuthenticateWithTwitchDialog : public QDialog {
    Q_OBJECT

public:
    AuthenticateWithTwitchDialog(QWidget* parent, TwitchAuth& twitchAuth);

    ~AuthenticateWithTwitchDialog();

private slots:
    void authenticateInBrowser();
    void authenticateWithAccessToken();
    void showAuthenticationFailureMessage(std::exception_ptr reason);
    void showAccessTokenAboutToExpireMessage(std::chrono::seconds expiresIn);

private:
    void showAuthenticationMessage(const std::string& message);
    void showOurselvesAfterAuthMessageBox();

private:
    TwitchAuth& twitchAuth;
    std::unique_ptr<Ui::AuthenticateWithTwitchDialog> ui;
    QMessageBox* authMessageBox;
};
