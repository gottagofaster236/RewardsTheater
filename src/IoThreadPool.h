// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <thread>

#include "BoostAsio.h"

class IoThreadPool {
public:
    IoThreadPool(unsigned nThreads);
    ~IoThreadPool();

    boost::asio::io_context ioContext;

private:
    std::vector<std::thread> threads;
};
