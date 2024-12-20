// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2024 by Lain Bailey <lain@obsproject.com>
// Copyright (c) 2024, Lev Leontev

#pragma once

#include <exception>
#include <optional>

typedef void libvlc_media_list_player_t;

class LibVlc {
public:
    LibVlc();
    ~LibVlc();
    LibVlc(const LibVlc&) = delete;

    static const std::optional<LibVlc> createSafe();

    const char* (*libvlc_get_version)();
    int (*libvlc_media_list_player_play_item_at_index)(libvlc_media_list_player_t* p_mlp, int i_index);

    class VlcLibraryLoadingError : public std::exception {
        const char* what() const noexcept override;
    };

private:
    bool load_libvlc_module();
    bool load_vlc_funcs();
    void unload();

    void* libvlc_module = nullptr;
#ifdef __APPLE__
    void* libvlc_core_module = nullptr;
#endif
};
