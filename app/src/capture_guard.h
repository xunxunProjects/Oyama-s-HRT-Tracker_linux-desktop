#pragma once

#include <QObject>
#include <QString>

class CaptureGuard : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool bestEffort READ bestEffort CONSTANT)

public:
    explicit CaptureGuard(QObject *parent = nullptr);
    ~CaptureGuard() override;

    bool active() const { return active_; }
    bool bestEffort() const { return true; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void testInject(const QString &tool);

    void handleCapture(const QString &tool);

signals:
    void activeChanged();
    void captureDetected(const QString &tool);

private:
    struct Impl;
    Impl *impl_;
    bool active_ = false;
};
