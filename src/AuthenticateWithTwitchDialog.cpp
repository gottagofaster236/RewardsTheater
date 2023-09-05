// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "AuthenticateWithTwitchDialog.h"

#include <obs.h>

#include <iostream>
#include <obs.hpp>

#include "ui_AuthenticateWithTwitchDialog.h"

AuthenticateWithTwitchDialog::AuthenticateWithTwitchDialog(QWidget* parent, TwitchAuth& twitchAuth)
    : QDialog(parent), ui(std::make_unique<Ui::AuthenticateWithTwitchDialog>()), twitchAuth(twitchAuth),
      qErrorMessage(this) {
    ui->setupUi(this);

    connect(
        ui->authenticateInBrowserButton,
        &QPushButton::clicked,
        this,
        &AuthenticateWithTwitchDialog::onAuthenticateInBrowserClicked
    );
    connect(
        ui->authenticateWithAccessTokenButton,
        &QPushButton::clicked,
        this,
        &AuthenticateWithTwitchDialog::onAuthenticateWithAccessTokenClicked
    );
    connect(this, &AuthenticateWithTwitchDialog::hideSignal, this, &AuthenticateWithTwitchDialog::hide);

    twitchAuth.addCallback(*this);
}

AuthenticateWithTwitchDialog::~AuthenticateWithTwitchDialog() {
    twitchAuth.removeCallback(*this);
}

void AuthenticateWithTwitchDialog::onAuthenticationSuccess() {
    // The callback is called from TwitchAuth's internal server thread, so we can't call hide() directly
    // (as it can only be called from the GUI thread).
    threadSafeHide();
}

void AuthenticateWithTwitchDialog::onAuthenticationFailure() {
    qErrorMessage.showMessage("Authentification failed");
}

void AuthenticateWithTwitchDialog::onAuthenticateInBrowserClicked() {
    twitchAuth.authenticate();
}

void AuthenticateWithTwitchDialog::onAuthenticateWithAccessTokenClicked() {
    twitchAuth.authenticateWithToken(ui->accessTokenEdit->text().toStdString());
}

void AuthenticateWithTwitchDialog::threadSafeHide() {
    emit hideSignal();
}
