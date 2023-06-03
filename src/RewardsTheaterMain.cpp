// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include <obs-module.h>
#include <obs.h>
#include <sstream>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static bool sources_enum_proc(void* data, obs_source_t* source) {
	(void) data;
	blog(LOG_INFO, "Source name: %s", obs_source_get_name(source));
	return true;
}

bool obs_module_load(void) {
	int x = 123;
	std::ostringstream oss;
	oss << x;
	blog(LOG_INFO, "idk 123 = %s", oss.str().c_str());
	obs_enum_sources(&sources_enum_proc, NULL);
	blog(LOG_INFO, "enumeration ended");
	return true;
}

void obs_module_unload() {
	blog(LOG_INFO, "plugin unloaded");
}
