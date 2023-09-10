// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include <obs-module.h>

#include <exception>

#include "Log.h"
#include "RewardsTheaterPlugin.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("RewardsTheater", "en-US")

static RewardsTheaterPlugin* plugin = nullptr;

bool obs_module_load(void) {
    try {
        plugin = new RewardsTheaterPlugin();
        return true;
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Error while loading RewardsTheater: {}", exception.what());
        return false;
    }
}

void obs_module_unload() {
    delete plugin;
    plugin = nullptr;
}
