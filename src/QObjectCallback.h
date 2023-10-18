// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <QObject>
#include <mutex>

/// Calls a method on a QObject, or does nothing if the QObject no longer exists.
class QObjectCallback : public QObject {
    Q_OBJECT

public:
    QObjectCallback(QObject* parent, QObject* receiver, const char* member)
        : QObject(parent), receiver(receiver), member(member) {
        connect(
            receiver, &QObject::destroyed, this, &QObjectCallback::clearReceiver, Qt::ConnectionType::DirectConnection
        );
    }

    template <typename Result>
    void operator()(const char* typeName, const Result& result) {
        qRegisterMetaType<Result>(typeName);
        {
            std::lock_guard guard(receiverMutex);
            if (receiver) {
                QMetaObject::invokeMethod(
                    receiver, member, Qt::ConnectionType::QueuedConnection, QArgument(typeName, result)
                );
            }
        }
        deleteLater();
    }

private slots:
    void clearReceiver() {
        std::lock_guard guard(receiverMutex);
        receiver = nullptr;
    }

private:
    QObject* receiver;
    std::mutex receiverMutex;
    const char* member;
};
