// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "EditRewardDialog.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QPalette>
#include <QPixmap>
#include <algorithm>
#include <array>
#include <format>

#include "HttpClient.h"
#include "ui_EditRewardDialog.h"

constexpr std::array DEFAULT_COLORS{
    Color{0, 199, 172},
    Color{250, 179, 255},
    Color{189, 0, 120},
    Color{255, 105, 5},
    Color{31, 105, 255},
    Color{189, 168, 255},
    Color{145, 71, 255},
    Color{250, 30, 210},
    Color{86, 189, 230},
    Color{66, 21, 97},
};

EditRewardDialog::EditRewardDialog(
    const std::optional<Reward>& originalReward,
    TwitchAuth& twitchAuth,
    TwitchRewardsApi& twitchRewardsApi,
    RewardRedemptionQueue& rewardRedemptionQueue,
    Settings& settings,
    QWidget* parent
)
    : QDialog(parent), originalReward(originalReward), twitchAuth(twitchAuth), twitchRewardsApi(twitchRewardsApi),
      rewardRedemptionQueue(rewardRedemptionQueue), settings(settings), ui(std::make_unique<Ui::EditRewardDialog>()),
      colorDialog(nullptr), confirmDeleteReward(nullptr), errorMessageBox(new ErrorMessageBox(this)),
      randomEngine(std::random_device()()) {
    obs_frontend_push_ui_translation(obs_module_get_string);
    ui->setupUi(this);
    obs_frontend_pop_ui_translation();

    setFixedSize(size());
    setAttribute(Qt::WA_DeleteOnClose);

    updateObsSourceComboBox();
    if (originalReward.has_value()) {
        showReward(originalReward.value());
    } else {
        showAddReward();
    }
    showUploadCustomIconLabel(twitchAuth.getUsername());
    showIcons();

    connect(ui->saveButton, &QPushButton::clicked, this, &EditRewardDialog::saveReward);
    connect(ui->cancelButton, &QPushButton::clicked, this, &EditRewardDialog::close);
    connect(ui->backgroundColorButton, &QPushButton::clicked, this, &EditRewardDialog::showColorDialog);
    connect(ui->updateObsSourcesButton, &QToolButton::clicked, this, &EditRewardDialog::updateObsSourceComboBox);
    connect(ui->playNowButton, &QToolButton::clicked, this, &EditRewardDialog::playObsSourceNow);
    connect(&twitchAuth, &TwitchAuth::onUsernameChanged, this, &EditRewardDialog::showUploadCustomIconLabel);
}

EditRewardDialog::~EditRewardDialog() = default;

void EditRewardDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange) {
        showIcons();
    }
}

void EditRewardDialog::showUploadCustomIconLabel(const std::optional<std::string>& username) {
    if (!username.has_value()) {
        return;
    }
    std::string uploadCustomIconLink = std::format(
        "<a href=\"https://dashboard.twitch.tv/u/{}/viewer-rewards/channel-points/rewards\">{}</a>",
        username.value(),
        obs_module_text("UploadCustomIconHere")
    );
    ui->uploadCustomIconLabel->setText(QString::fromStdString(uploadCustomIconLink));
}

void EditRewardDialog::showSelectedColor(Color newBackgroundColor) {
    selectedColor = newBackgroundColor;
    std::string backgroundColorStyle = std::format("background: {}", selectedColor.toHex());
    ui->backgroundColorButton->setStyleSheet(QString::fromStdString(backgroundColorStyle));
}

void EditRewardDialog::showColorDialog() {
    if (!colorDialog) {
        colorDialog = new QColorDialog({selectedColor.red, selectedColor.green, selectedColor.blue}, this);
        connect(
            colorDialog, &QColorDialog::colorSelected, this, qOverload<QColor>(&EditRewardDialog::showSelectedColor)
        );
    }
    colorDialog->show();
}

void EditRewardDialog::saveReward() {
    RewardData rewardData = getRewardData();
    if (!originalReward.has_value()) {
        twitchRewardsApi.createReward(rewardData, this, "showSaveRewardResult");
    } else if (rewardData != static_cast<const RewardData&>(originalReward.value())) {
        Reward newReward(originalReward.value(), rewardData);
        twitchRewardsApi.updateReward(newReward, this, "showSaveRewardResult");
    } else {
        settings.setObsSourceName(originalReward.value().id, getObsSourceName());
        close();
    }
}

void EditRewardDialog::showSaveRewardResult(std::variant<std::exception_ptr, Reward> result) {
    if (std::holds_alternative<Reward>(result)) {
        Reward reward = std::get<Reward>(result);
        settings.setObsSourceName(reward.id, getObsSourceName());
        emit onRewardSaved(reward);
        close();
        return;
    }

    std::string message;
    try {
        std::rethrow_exception(std::get<std::exception_ptr>(result));
    } catch (const TwitchRewardsApi::NotAffiliateException&) {
        message = obs_module_text("CouldNotSaveRewardNotAffiliate");
    } catch (const TwitchRewardsApi::NotManageableRewardException&) {
        message = obs_module_text("CouldNotSaveRewardNotManageable");
    } catch (const TwitchRewardsApi::EmptyRewardTitleException&) {
        message = obs_module_text("CouldNotSaveRewardEmptyTitle");
    } catch (const HttpClient::NetworkException&) {
        message = obs_module_text("CouldNotSaveRewardNetwork");
    } catch (const std::exception& exception) {
        message = std::vformat(obs_module_text("CouldNotSaveRewardOther"), std::make_format_args(exception.what()));
    }
    errorMessageBox->show(message);
}

void EditRewardDialog::updateObsSourceComboBox() {
    std::vector<std::string> obsSources = RewardRedemptionQueue::enumObsSources();
    QString oldObsSource = ui->obsSourceComboBox->currentData().toString();

    ui->obsSourceComboBox->clear();
    ui->obsSourceComboBox->addItem(obs_module_text("NotSelected"), QString());
    for (const std::string& obsSourceString : obsSources) {
        QString obsSource = QString::fromStdString(obsSourceString);
        ui->obsSourceComboBox->addItem(obsSource, obsSource);

        if (obsSource == oldObsSource) {
            ui->obsSourceComboBox->setCurrentIndex(ui->obsSourceComboBox->count() - 1);
        }
    }

    showObsSourceComboBoxIcon();
}

void EditRewardDialog::playObsSourceNow() {
    std::optional<std::string> obsSourceName = getObsSourceName();
    if (obsSourceName.has_value()) {
        rewardRedemptionQueue.playObsSource(obsSourceName.value());
    }
}

void EditRewardDialog::showReward(const Reward& reward) {
    ui->enabledCheckBox->setChecked(reward.isEnabled);
    ui->titleEdit->setText(QString::fromStdString(reward.title));
    ui->costSpinBox->setValue(reward.cost);
    showSelectedColor(reward.backgroundColor);
    ui->limitRedemptionsPerStreamCheckBox->setChecked(reward.maxRedemptionsPerStream.has_value());
    ui->limitRedemptionsPerStreamSpinBox->setValue(reward.maxRedemptionsPerStream.value_or(1));
    ui->limitRedemptionsPerUserPerStreamCheckBox->setChecked(reward.maxRedemptionsPerUserPerStream.has_value());
    ui->limitRedemptionsPerUserPerStreamSpinBox->setValue(reward.maxRedemptionsPerUserPerStream.value_or(1));
    ui->enableGlobalCooldownCheckBox->setChecked(reward.globalCooldownSeconds.has_value());
    ui->enableGlobalCooldownSpinBox->setValue(reward.globalCooldownSeconds.value_or(1));
    setObsSourceName(settings.getObsSourceName(reward.id));

    if (reward.canManage) {
        ui->cannotEditRewardLabel->hide();
        createConfirmDeleteReward(reward);
    } else {
        disableInput();
    }
}

void EditRewardDialog::showSelectedColor(QColor newBackgroundColor) {
    showSelectedColor(Color(newBackgroundColor.red(), newBackgroundColor.green(), newBackgroundColor.blue()));
}

void EditRewardDialog::createConfirmDeleteReward(const Reward& reward) {
    confirmDeleteReward = new ConfirmDeleteReward(reward, twitchRewardsApi, this);
    connect(
        ui->deleteButton, &QPushButton::clicked, confirmDeleteReward, &ConfirmDeleteReward::showConfirmDeleteMessageBox
    );
    connect(confirmDeleteReward, &ConfirmDeleteReward::onRewardDeleted, this, &EditRewardDialog::onRewardDeleted);
}

void EditRewardDialog::disableInput() {
    ui->enabledCheckBox->setEnabled(false);
    ui->titleEdit->setEnabled(false);
    ui->costSpinBox->setEnabled(false);
    ui->backgroundColorButton->setEnabled(false);
    ui->limitRedemptionsPerStreamCheckBox->setEnabled(false);
    ui->limitRedemptionsPerStreamSpinBox->setEnabled(false);
    ui->limitRedemptionsPerUserPerStreamCheckBox->setEnabled(false);
    ui->limitRedemptionsPerUserPerStreamSpinBox->setEnabled(false);
    ui->enableGlobalCooldownCheckBox->setEnabled(false);
    ui->enableGlobalCooldownSpinBox->setEnabled(false);
    ui->deleteButton->setEnabled(false);
}

void EditRewardDialog::showAddReward() {
    setWindowTitle(obs_module_text("AddReward"));
    ui->cannotEditRewardLabel->hide();
    ui->enabledCheckBox->setChecked(true);
    showSelectedColor(chooseRandomColor());
    ui->deleteButton->setEnabled(false);
}

Color EditRewardDialog::chooseRandomColor() {
    Color result;
    std::ranges::sample(DEFAULT_COLORS, &result, 1, randomEngine);
    return result;
}

void EditRewardDialog::showIcons() {
    showObsSourceComboBoxIcon();
    showUpdateObsSourcesButtonIcon();
}

void EditRewardDialog::showObsSourceComboBoxIcon() {
    QIcon icon;
    if (shouldUseWhiteIcons()) {
        icon = QIcon(":/icons/media-white.svg");
    } else {
        icon = QIcon(":/icons/media-dark.svg");
    }
    for (int i = 0; i < ui->obsSourceComboBox->count(); i++) {
        ui->obsSourceComboBox->setItemIcon(i, icon);
    }
}

void EditRewardDialog::showUpdateObsSourcesButtonIcon() {
    QIcon icon;
    if (shouldUseWhiteIcons()) {
        icon = QIcon(":/icons/reload-white.svg");
    } else {
        icon = QIcon(":/icons/reload-dark.svg");
    }
    ui->updateObsSourcesButton->setIcon(icon);
}

bool EditRewardDialog::shouldUseWhiteIcons() {
    QColor textColor = palette().color(QPalette::ButtonText);
    return textColor.valueF() > 0.5f;
}

void EditRewardDialog::setObsSourceName(const std::optional<std::string>& obsSourceNameString) {
    QString obsSourceName;
    if (obsSourceNameString.has_value()) {
        obsSourceName = QString::fromStdString(obsSourceNameString.value());
    }
    int obsSourceIndex = ui->obsSourceComboBox->findData(obsSourceName);
    if (obsSourceIndex != -1) {
        ui->obsSourceComboBox->setCurrentIndex(obsSourceIndex);
    }
}

std::optional<std::string> EditRewardDialog::getObsSourceName() {
    QString obsSourceName = ui->obsSourceComboBox->currentData().toString();
    if (obsSourceName.isEmpty()) {
        return {};
    } else {
        return obsSourceName.toStdString();
    }
}

RewardData EditRewardDialog::getRewardData() {
    return RewardData{
        ui->titleEdit->text().toStdString(),
        ui->costSpinBox->value(),
        ui->enabledCheckBox->isChecked(),
        selectedColor,
        getOptionalSetting(ui->limitRedemptionsPerStreamCheckBox, ui->limitRedemptionsPerStreamSpinBox),
        getOptionalSetting(ui->limitRedemptionsPerUserPerStreamCheckBox, ui->limitRedemptionsPerUserPerStreamSpinBox),
        getOptionalSetting(ui->enableGlobalCooldownCheckBox, ui->enableGlobalCooldownSpinBox),
    };
}

std::optional<std::int64_t> EditRewardDialog::getOptionalSetting(QCheckBox* checkBox, QSpinBox* spinBox) {
    if (checkBox->isChecked()) {
        return spinBox->value();
    } else {
        return {};
    }
}
