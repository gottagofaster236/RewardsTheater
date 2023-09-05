// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QDialog>
#include <QErrorMessage>
#include <memory>
#include <thread>

#include "TwitchAuth.h"

namespace Ui {
class AuthenticateWithTwitchDialog;
}

class AuthenticateWithTwitchDialog : public QDialog, private TwitchAuth::Callback {
    Q_OBJECT

public:
    AuthenticateWithTwitchDialog(QWidget* parent, TwitchAuth& twitchAuth);

    ~AuthenticateWithTwitchDialog();

    void onAuthenticationSuccess() override;
    void onAuthenticationFailure() override;

signals:
    void hideSignal();

private slots:
    void onAuthenticateInBrowserClicked();
    void onAuthenticateWithAccessTokenClicked();

private:
    // Calls hide() on GUI thread.
    void threadSafeHide();

    std::unique_ptr<Ui::AuthenticateWithTwitchDialog> ui;
    TwitchAuth& twitchAuth;
    QErrorMessage qErrorMessage;
};
