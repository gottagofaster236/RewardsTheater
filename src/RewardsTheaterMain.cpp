// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <exception>
#include <memory>

#include "Log.h"
#include "RewardsTheaterPlugin.h"
#include "RewardsTheaterVersion.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("RewardsTheater", "en-US")

static std::unique_ptr<RewardsTheaterPlugin> plugin;

static void on_frontend_event(obs_frontend_event event, void* data);

bool obs_module_load() {
    log(LOG_INFO, "Loading plugin, version {}", REWARDS_THEATER_VERSION);
    try {
        plugin = std::make_unique<RewardsTheaterPlugin>();
        obs_frontend_add_event_callback(on_frontend_event, nullptr);
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

void on_frontend_event(obs_frontend_event event, [[maybe_unused]] void* data) {
    if (event == OBS_FRONTEND_EVENT_EXIT) {
        obs_frontend_remove_event_callback(on_frontend_event, nullptr);
        // Unload early to avoid holding up a reference counter to any OBS sources.
        obs_module_unload();
    }
}
