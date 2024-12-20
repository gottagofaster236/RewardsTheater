// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2024 by Lain Bailey <lain@obsproject.com>
// Copyright (c) 2024, Lev Leontev

#ifdef _WIN32
#include <windows.h>
#endif

#include <util/bmem.h>
#include <util/platform.h>

#include "LibVlc.h"
#include "Log.h"

LibVlc::LibVlc() {
    if (!load_libvlc_module()) {
        log(LOG_WARNING, "Couldn't find VLC installation, can't play rewards that use VLC Video Source");
        throw VlcLibraryLoadingError();
    }

    if (!load_vlc_funcs()) {
        throw VlcLibraryLoadingError();
    }

    log(LOG_INFO, "VLC {} found", libvlc_get_version());
}

LibVlc::~LibVlc() {
    unload();
}

const std::optional<LibVlc> LibVlc::createSafe() {
    try {
        return std::make_optional<LibVlc>();
    } catch (const VlcLibraryLoadingError &) {
        return {};
    }
}

const char *LibVlc::VlcLibraryLoadingError::what() const noexcept {
    return "VlcLibraryLoadingError";
}

bool LibVlc::load_libvlc_module() {
#ifdef _WIN32
    char *path_utf8 = NULL;
    wchar_t path[1024];
    LSTATUS status;
    DWORD size;
    HKEY key;

    memset(path, 0, 1024 * sizeof(wchar_t));

    status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VideoLAN\\VLC", &key);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    size = 1024;
    status = RegQueryValueExW(key, L"InstallDir", NULL, NULL, (LPBYTE) path, &size);
    if (status == ERROR_SUCCESS) {
        wcscat(path, L"\\libvlc.dll");
        os_wcs_to_utf8_ptr(path, 0, &path_utf8);
        libvlc_module = os_dlopen(path_utf8);
        bfree(path_utf8);
    }

    RegCloseKey(key);
#else

/* According to otoolo -L, this is what libvlc.dylib wants. */
#ifdef __APPLE__
#define LIBVLC_DIR "/Applications/VLC.app/Contents/MacOS/"
#define LIBVLC_CORE_FILE LIBVLC_DIR "lib/libvlccore.dylib"
#define LIBVLC_FILE LIBVLC_DIR "lib/libvlc.5.dylib"
    setenv("VLC_PLUGIN_PATH", LIBVLC_DIR "plugins", false);
    libvlc_core_module = os_dlopen(LIBVLC_CORE_FILE);

    if (!libvlc_core_module) {
        return false;
    }
#else
#define LIBVLC_FILE "libvlc.so.5"
#endif
    libvlc_module = os_dlopen(LIBVLC_FILE);

#endif

    return libvlc_module != NULL;
}

bool LibVlc::load_vlc_funcs() {
#define LOAD_VLC_FUNC(func) \
    do { \
        func = reinterpret_cast<decltype(func)>(os_dlsym(libvlc_module, #func)); \
        if (!func) { \
            log(LOG_WARNING, \
                "Could not find VLC " \
                "function {}, VLC loading failed", \
                #func); \
            return false; \
        } \
    } while (false)

    LOAD_VLC_FUNC(libvlc_get_version);
    LOAD_VLC_FUNC(libvlc_media_list_player_play_item_at_index);
    return true;
}

void LibVlc::unload() {
#ifdef __APPLE__
    if (libvlc_core_module) {
        os_dlclose(libvlc_core_module);
    }
#endif
    if (libvlc_module) {
        os_dlclose(libvlc_module);
    }
}
