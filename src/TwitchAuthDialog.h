// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <chrono>
#include <memory>
#include <thread>

#include "ErrorMessageBox.h"
#include "OnTopDialog.h"
#include "TwitchAuth.h"

namespace Ui {
class TwitchAuthDialog;
}

class TwitchAuthDialog : public OnTopDialog {
    Q_OBJECT

public:
    TwitchAuthDialog(QWidget* parent, TwitchAuth& twitchAuth);

    ~TwitchAuthDialog() override;

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
    ErrorMessageBox* errorMessageBox;
};
