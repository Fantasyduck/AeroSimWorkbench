#pragma once
#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
#include <QObject>
#include <QHash>
#include <QQueue>
#include <QStringList>
class QThread;
namespace aerosim { namespace visualization {
class BoundaryLoadService final : public QObject {
    Q_OBJECT
public:
    explicit BoundaryLoadService(QObject* parent=nullptr);
    ~BoundaryLoadService() override;
    std::shared_ptr<const BoundaryMesh> mesh(const QString& id) const;
    bool busy() const;
public slots:
    void load(const QString& path);
    void loadFiles(const QStringList &paths);
    void remove(const QString& id);
signals:
    void started(const QString& id,const QString& path);
    void loaded(const QString& id,const QString& path,const QString& summary);
    void failed(const QString& id,const QString& message);
    void busyChanged(bool busy);
private:
    struct Job;
    void startNext();
    QQueue<QString> pending_;
    std::shared_ptr<Job> job_;
    QThread* thread_=nullptr;
    QHash<QString,std::shared_ptr<const BoundaryMesh>> meshes_;
};
} }
