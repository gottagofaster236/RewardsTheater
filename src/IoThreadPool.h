// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <thread>

#include "BoostAsio.h"

class IoThreadPool {
public:
    IoThreadPool(unsigned nThreads);
    ~IoThreadPool();

    void stop();

    boost::asio::io_context ioContext;

private:
    std::vector<std::thread> threads;
};
