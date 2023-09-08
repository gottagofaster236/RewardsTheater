// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <QMessageBox>
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
    void onAuthenticateInBrowserClicked();
    void onAuthenticateWithAccessTokenClicked();
    void showAuthenticationFailureMessage();

private:
    std::unique_ptr<Ui::AuthenticateWithTwitchDialog> ui;
    TwitchAuth& twitchAuth;
};
