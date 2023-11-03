// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include <obs-module.h>

#include <exception>
#include <memory>

#include "Log.h"
#include "RewardsTheaterPlugin.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("RewardsTheater", "en-US")

static std::unique_ptr<RewardsTheaterPlugin> plugin;

bool obs_module_load(void) {
    try {
        plugin = std::make_unique<RewardsTheaterPlugin>();
        return true;
    } catch (const std::exception& exception) {
        log(LOG_ERROR, "Error while loading RewardsTheater: {}", exception.what());
        return false;
    } catch (...) {
        log(LOG_ERROR, "Unknown error while loading RewardsTheater.");
        return false;
    }
}

void obs_module_unload() {
    plugin = nullptr;
}
