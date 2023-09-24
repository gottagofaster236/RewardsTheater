// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "IoService.h"

#include "Log.h"

namespace asio = boost::asio;

IoService::IoService() = default;

IoService::~IoService() {
    ioContext.stop();
    ioThread.join();
}

void IoService::startService() {
    if (ioThread.joinable()) {
        return;
    }
    ioThread = std::thread([=, this]() {
        auto workGuard = asio::make_work_guard(ioContext);  // Run forever until stop()
        try {
            ioContext.run();
        } catch (const boost::system::system_error& error) {
            log(LOG_ERROR, "I/O exception: {}", error.what());
        }
    });
}
