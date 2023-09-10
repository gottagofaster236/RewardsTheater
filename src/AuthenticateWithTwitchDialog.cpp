// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "AuthenticateWithTwitchDialog.h"

#include <obs-module.h>
#include <obs.h>

#include <iostream>
#include <obs.hpp>

#include "ui_AuthenticateWithTwitchDialog.h"

AuthenticateWithTwitchDialog::AuthenticateWithTwitchDialog(QWidget* parent, TwitchAuth& twitchAuth)
    : QDialog(parent), ui(std::make_unique<Ui::AuthenticateWithTwitchDialog>()), twitchAuth(twitchAuth) {
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

    connect(&twitchAuth, &TwitchAuth::onAuthenticationSuccess, this, &AuthenticateWithTwitchDialog::hide);
    connect(
        &twitchAuth,
        &TwitchAuth::onAuthenticationFailure,
        this,
        &AuthenticateWithTwitchDialog::showAuthenticationFailureMessage
    );
}

AuthenticateWithTwitchDialog::~AuthenticateWithTwitchDialog() = default;

void AuthenticateWithTwitchDialog::onAuthenticateInBrowserClicked() {
    twitchAuth.authenticate();
}

void AuthenticateWithTwitchDialog::onAuthenticateWithAccessTokenClicked() {
    twitchAuth.authenticateWithToken(ui->accessTokenEdit->text().toStdString());
}

void AuthenticateWithTwitchDialog::showAuthenticationFailureMessage(
    [[maybe_unused]] TwitchAuth::AuthenticationFailureReason reason
) {
    const char* errorLookupString = nullptr;
    switch (reason) {
        case TwitchAuth::AuthenticationFailureReason::AUTH_TOKEN_INVALID:
            errorLookupString = "TwitchAuthenticationFailedInvalid";
            break;
        case TwitchAuth::AuthenticationFailureReason::NETWORK_ERROR:
            errorLookupString = "TwitchAuthenticationFailedNetwork";
            break;
    }
    QMessageBox::warning(this, obs_module_text("RewardsTheater"), obs_module_text(errorLookupString));
}
