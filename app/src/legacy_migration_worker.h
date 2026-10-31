#pragma once

#include <QObject>
#include <QString>

/// Runs managed legacy-to-vault migration off the GUI thread.  The worker owns
/// its own LocalCluster, Database, Connection, and VaultStore; it never touches
/// AppController models.  All signals are emitted from the worker thread and
/// must be delivered with queued connections.
class LegacyMigrationWorker : public QObject {
    Q_OBJECT

public:
    explicit LegacyMigrationWorker(QObject* parent = nullptr);

public slots:
    void run(const QString& data_dir, const QString& socket_dir,
             const QString& phrase);

signals:
    void progress(const QString& kind, int done, int total);
    void finished();
    void failed(const QString& message, const QString& plaintext_path);
};
