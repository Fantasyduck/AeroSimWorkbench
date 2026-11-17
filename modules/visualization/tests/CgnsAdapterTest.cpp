#include "aerosim/visualization/DataAdapter.h"
#include <QCoreApplication>
#include <QDebug>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#define CHECK(x) do { if (!(x)) { qCritical() << "check failed:" << #x << __LINE__; return 1; } } while(false)
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if (argc != 2) { qCritical() << "expected M6 flowfield CGNS path"; return 2; }
    auto adapter=aerosim::visualization::createCgnsAdapter();
    CHECK(adapter->accepts(QStringLiteral("sample.CGNS")));
    CHECK(!adapter->accepts(QStringLiteral("sample.tec")));
    CHECK(!adapter->read(QStringLiteral("this-file-does-not-exist.cgns")).ok());
    const auto result=adapter->read(QString::fromLocal8Bit(argv[1]));
    if(!result.ok()) { qCritical() << result.error; return 1; }
    CHECK(result.dataset.regions.size()==7);
    const auto &volume=result.dataset.regions.first();
    CHECK(volume.dimension==3 && volume.data->GetNumberOfPoints()==107973 && volume.data->GetNumberOfCells()==257817);
    CHECK(volume.data->GetPointData()->GetArray("p_dim"));
    CHECK(volume.data->GetPointData()->GetArray("u_dim"));
    CHECK(volume.data->GetPointData()->GetArray("v_dim"));
    CHECK(volume.data->GetPointData()->GetArray("w_dim"));
    bool foundWing=false;
    for(const auto &region:result.dataset.regions) {
        if(region.name.contains(QStringLiteral("wing_down"))) {
            foundWing=true; CHECK(region.dimension==2 && region.data->GetNumberOfCells()==3083);
            CHECK(region.data->GetNumberOfPoints()==1627);
            CHECK(region.data->GetPointData()->GetArray("p_dim"));
        }
    }
    CHECK(foundWing);
    qInfo() << "M6 CGNS volume, boundaries, point fields and missing-file handling passed";
    return 0;
}
