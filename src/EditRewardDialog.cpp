#include "EditRewardDialog.h"

#include "ui_EditRewardDialog.h"

EditRewardDialog::EditRewardDialog(
    const std::optional<std::string>& rewardId,
    TwitchRewardsApi& twitchRewardsApi,
    QWidget* parent
)
    : QDialog(parent), rewardId(rewardId), twitchRewardsApi(twitchRewardsApi),
      ui(std::make_unique<Ui::EditRewardDialog>()) {
    ui->setupUi(this);
    (void) this->rewardId;
    (void) this->twitchRewardsApi;
}

EditRewardDialog::~EditRewardDialog() {}
