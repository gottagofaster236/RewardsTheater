// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "AuthenticateWithTwitchDialog.h"

#include <fmt/core.h>
#include <obs-module.h>
#include <obs.h>

#include <iostream>
#include <obs.hpp>

#include "ui_AuthenticateWithTwitchDialog.h"

AuthenticateWithTwitchDialog::AuthenticateWithTwitchDialog(QWidget* parent, TwitchAuth& twitchAuth)
    : QDialog(parent), twitchAuth(twitchAuth), ui(std::make_unique<Ui::AuthenticateWithTwitchDialog>()),
      authMessageBox(new QMessageBox(this)) {
    ui->setupUi(this);
    authMessageBox->setWindowTitle(obs_module_text("RewardsTheater"));
    authMessageBox->setIcon(QMessageBox::Icon::Warning);

    connect(
        ui->authenticateInBrowserButton,
        &QPushButton::clicked,
        this,
        &AuthenticateWithTwitchDialog::authenticateInBrowser
    );
    connect(
        ui->authenticateWithAccessTokenButton,
        &QPushButton::clicked,
        this,
        &AuthenticateWithTwitchDialog::authenticateWithAccessToken
    );
    connect(
        authMessageBox, &QMessageBox::finished, this, &AuthenticateWithTwitchDialog::showOurselvesAfterAuthMessageBox
    );

    connect(&twitchAuth, &TwitchAuth::onAuthenticationSuccess, this, &AuthenticateWithTwitchDialog::hide);
    connect(
        &twitchAuth,
        &TwitchAuth::onAuthenticationFailure,
        this,
        &AuthenticateWithTwitchDialog::showAuthenticationFailureMessage
    );
    connect(
        &twitchAuth,
        &TwitchAuth::onAccessTokenAboutToExpire,
        this,
        &AuthenticateWithTwitchDialog::showAccessTokenAboutToExpireMessage
    );

    // Start TwitchAuth only after we have connected our slots
    twitchAuth.startService();
}

AuthenticateWithTwitchDialog::~AuthenticateWithTwitchDialog() = default;

void AuthenticateWithTwitchDialog::authenticateInBrowser() {
    twitchAuth.authenticate();
}

void AuthenticateWithTwitchDialog::authenticateWithAccessToken() {
    twitchAuth.authenticateWithToken(ui->accessTokenEdit->text().toStdString());
}

void AuthenticateWithTwitchDialog::showAuthenticationFailureMessage(TwitchAuth::AuthenticationFailureReason reason) {
    const char* errorLookupString = nullptr;
    switch (reason) {
    case TwitchAuth::AuthenticationFailureReason::AUTH_TOKEN_INVALID:
        errorLookupString = "TwitchAuthenticationFailedInvalid";
        break;
    case TwitchAuth::AuthenticationFailureReason::NETWORK_ERROR:
        errorLookupString = "TwitchAuthenticationFailedNetwork";
        break;
    }
    showAuthenticationMessage(obs_module_text(errorLookupString));
}

void AuthenticateWithTwitchDialog::showAccessTokenAboutToExpireMessage(std::chrono::seconds expiresIn) {
    int expiresInHours = std::ceil(expiresIn.count() / 3600);
    std::string message = fmt::format(fmt::runtime(obs_module_text("TwitchTokenAboutToExpire")), expiresInHours);
    showAuthenticationMessage(message);
}

void AuthenticateWithTwitchDialog::showAuthenticationMessage(const std::string& message) {
    authMessageBox->setText(QString::fromStdString(message));
    for (QAbstractButton* button : authMessageBox->buttons()) {
        authMessageBox->removeButton(button);
    }
    if (isVisible()) {
        authMessageBox->setStandardButtons(QMessageBox::Ok);
    } else {
        authMessageBox->addButton(obs_module_text("LogInAgain"), QMessageBox::AcceptRole);
        authMessageBox->addButton(QMessageBox::Cancel);
    }
    authMessageBox->show();
}

void AuthenticateWithTwitchDialog::showOurselvesAfterAuthMessageBox() {
    if (isVisible()) {
        return;
    }
    if (authMessageBox->buttonRole(authMessageBox->clickedButton()) == QMessageBox::AcceptRole) {
        show();
    }
}
