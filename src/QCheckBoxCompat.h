// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026, Lev Leontev

#pragma once

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#define checkStateChangedCompat checkStateChanged
#else
#define checkStateChangedCompat stateChanged
#endif
