// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <thread>

#include "BoostAsio.h"

// A class that performs network I/O operations on a dedicated thread, which can be started with startService().
class IoService {
public:
    IoService();
    ~IoService();
    void startService();

protected:
    boost::asio::io_context ioContext;

private:
    std::thread ioThread;
};
