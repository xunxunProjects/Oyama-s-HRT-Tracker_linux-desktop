#include "capture_guard.h"

#include <oyama/security/capture.h>
#include <oyama/vault.h>

#include <QMetaObject>
#include <QTimer>

struct CaptureGuard::Impl {
    oyama_capture_guard *guard = nullptr;
};

namespace {
void c_callback(const char *tool, int /*pid*/, void *user_data) {
    auto *self = static_cast<CaptureGuard *>(user_data);
    QString t = tool ? QString::fromUtf8(tool) : QString();
    QMetaObject::invokeMethod(self, [self, t]() { self->handleCapture(t); },
                              Qt::QueuedConnection);
}
}  // namespace

CaptureGuard::CaptureGuard(QObject *parent) : QObject(parent), impl_(new Impl) {
    oyama_error err{};
    if (oyama_capture_guard_create(&impl_->guard, this,
                                   reinterpret_cast<oyama_capture_cb>(c_callback),
                                   &err) != OYAMA_VAULT_OK) {
        delete impl_;
        impl_ = nullptr;
    }
}

CaptureGuard::~CaptureGuard() {
    if (impl_) {
        if (impl_->guard) {
            oyama_capture_guard_stop(impl_->guard);
            oyama_capture_guard_destroy(impl_->guard);
        }
        delete impl_;
    }
}

void CaptureGuard::start() {
    if (impl_ && impl_->guard) {
        oyama_capture_guard_start(impl_->guard);
    }
}

void CaptureGuard::stop() {
    if (impl_ && impl_->guard) {
        oyama_capture_guard_stop(impl_->guard);
    }
}

void CaptureGuard::testInject(const QString &tool) {
    handleCapture(tool);
}

void CaptureGuard::handleCapture(const QString &tool) {
    active_ = true;
    emit activeChanged();
    emit captureDetected(tool);
    // auto-hide after 2s
    QTimer::singleShot(2000, this, [this]() {
        active_ = false;
        emit activeChanged();
    });
}
