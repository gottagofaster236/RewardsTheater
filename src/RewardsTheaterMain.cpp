// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardsTheaterSettings.h"
#include <obs-module.h>
#include <obs.h>
#include <QMainWindow>
#include <QAction>
#include <obs-frontend-api.h>
#include <memory>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("RewardsTheater", "en-US")

bool obs_module_load(void) {
    QMainWindow *mainWindow = (QMainWindow*) obs_frontend_get_main_window();
    QAction *action = (QAction*) obs_frontend_add_tools_menu_qaction(
	    obs_module_text("RewardsTheater"));
    
    obs_frontend_push_ui_translation(obs_module_get_string);
	auto rewardTheaterSettings = new RewardsTheaterSettings(mainWindow);
    obs_frontend_pop_ui_translation();

	auto menuCb = [=]
	{
		rewardTheaterSettings->setVisible(!rewardTheaterSettings->isVisible());
	};

	action->connect(action, &QAction::triggered, menuCb);
    
    return true;
}

void obs_module_unload() {
    
}
