#include "aerosim/visualization/data/BoundaryLoadService.h"
#include "aerosim/visualization/data/BoundaryMeshReader.h"
#include <QFileInfo>
#include <QThread>
#include <QUuid>
#include <exception>
namespace aerosim { namespace visualization {
struct BoundaryLoadService::Job {
    QString id,path,error;
    std::atomic_bool cancelled{false};
    std::shared_ptr<const BoundaryMesh> result;
};
BoundaryLoadService::BoundaryLoadService(QObject* parent):QObject(parent) {}
BoundaryLoadService::~BoundaryLoadService() {
    if(job_) job_->cancelled=true;
    if(thread_) { thread_->disconnect(this); thread_->wait(); delete thread_; }
}
bool BoundaryLoadService::busy() const { return thread_ || !pending_.isEmpty(); }
std::shared_ptr<const BoundaryMesh> BoundaryLoadService::mesh(const QString& id) const { return meshes_.value(id); }
void BoundaryLoadService::load(const QString& path) {
    loadFiles({path});
}
void BoundaryLoadService::loadFiles(const QStringList &paths) {
    Q_ASSERT(QThread::currentThread()==thread());
    const bool alreadyBusy = busy();
    for (const auto &path : paths) {
        if (!path.isEmpty()) pending_.enqueue(path);
    }
    if (!alreadyBusy && !pending_.isEmpty()) {
        emit busyChanged(true);
        startNext();
    }
}
void BoundaryLoadService::startNext() {
    auto job=std::make_shared<Job>(); job_=job;
    job->id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    job->path=pending_.dequeue();
    thread_=QThread::create([job] {
        try { job->result=BoundaryMeshReader::read(job->path,&job->cancelled); }
        catch(const std::exception& e) { job->error=QString::fromUtf8(e.what()); }
        catch(...) { job->error=QString::fromUtf8("未知网格读取错误"); }
    });
    auto* worker=thread_;
    worker->setParent(this); // Also owns completed threads awaiting deleteLater().
    // Only a completion notification crosses threads. The private result is
    // consumed after wait(), never mutated concurrently with the GUI.
    connect(worker,&QThread::finished,this,[this,job,worker] {
        worker->wait();
        if(!job->cancelled && job->result) {
            meshes_.insert(job->id,job->result);
            emit loaded(job->id,job->path,job->result->summary());
        } else {
            const auto reason = job->cancelled ? QString::fromUtf8("已取消导入") : job->error;
            emit failed(job->id, QStringLiteral("%1 — %2").arg(QFileInfo(job->path).fileName(), reason));
        }
        thread_=nullptr;
        job_.reset();
        worker->deleteLater();
        if (pending_.isEmpty()) emit busyChanged(false);
        else startNext();
    },Qt::QueuedConnection);
    emit started(job->id,job->path);
    worker->start();
}
void BoundaryLoadService::remove(const QString& id) {
    Q_ASSERT(QThread::currentThread()==thread());
    if(job_ && job_->id==id) job_->cancelled=true;
    meshes_.remove(id);
}
} }
