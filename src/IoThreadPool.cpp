// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#include "IoThreadPool.h"

#include "Log.h"

IoThreadPool::IoThreadPool(unsigned nThreads) {
    auto runIoContext = [this]() {
        auto workGuard = boost::asio::make_work_guard(ioContext);  // Run forever until stop()
        try {
            ioContext.run();
        } catch (const std::exception& exception) {
            log(LOG_ERROR, "Exception in IoThreadPool: {}", exception.what());
        }
    };

    for (unsigned i = 0; i < nThreads; i++) {
        threads.emplace_back(runIoContext);
    }
}

IoThreadPool::~IoThreadPool() {
    stop();
}

void IoThreadPool::stop() {
    ioContext.stop();
    for (std::thread& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}
