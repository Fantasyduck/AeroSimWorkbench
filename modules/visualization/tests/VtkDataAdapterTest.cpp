#include "aerosim/visualization/DataAdapter.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkNew.h>
#include <vtkXMLDataSetWriter.h>
#include <vtkXMLMultiBlockDataWriter.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositeDataSet.h>
#include <vtkInformation.h>
#define CHECK(x) do { if (!(x)) { qCritical() << "check failed:" << #x << "line" << __LINE__; return 1; } } while (0)
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    if (argc != 5) { qCritical() << "expected legacy VTK, flow Tecplot, cell-centered Tecplot and analytical cube files"; return 2; }
    auto adapter=aerosim::visualization::createVtkDataAdapter();
    CHECK(!adapter->accepts(QStringLiteral("file.plt")));
    CHECK(!adapter->read(QStringLiteral("missing.vtk")).ok());
    auto vtk=adapter->read(QString::fromLocal8Bit(argv[1]));
    CHECK(vtk.ok()); CHECK(!vtk.dataset.regions.isEmpty());
    CHECK(vtk.dataset.regions[0].data->GetNumberOfPoints()>0);
    auto cube=adapter->read(QString::fromLocal8Bit(argv[4]));
    CHECK(cube.ok()); CHECK(cube.dataset.regions.size()==1);
    auto *pointData=cube.dataset.regions[0].data->GetPointData();
    CHECK(pointData->GetArray("Pressure")); CHECK(pointData->GetArray("Temperature"));
    CHECK(pointData->GetArray("Phi")); CHECK(pointData->GetArray("Velocity"));
    auto flow=adapter->read(QString::fromLocal8Bit(argv[2]));
    if (!flow.ok()) qCritical() << flow.error;
    CHECK(flow.ok()); CHECK(flow.dataset.regions.size()==1);
    CHECK(flow.dataset.regions[0].data->GetNumberOfPoints()==441);
    CHECK(flow.dataset.regions[0].data->GetNumberOfCells()==800);
    CHECK(flow.dataset.regions[0].data->GetPointData()->GetArray("P"));
    CHECK(flow.dataset.regions[0].data->GetPointData()->GetArray("V"));
    auto cells=adapter->read(QString::fromLocal8Bit(argv[3]));
    if (!cells.ok()) qCritical() << cells.error;
    CHECK(cells.ok()); CHECK(cells.dataset.regions.size()==2);
    CHECK(cells.dataset.regions[0].data->GetCellData()->GetArray("P"));
    CHECK(cells.dataset.regions[1].data->GetPointData()->GetArray("V"));
    QTemporaryDir dir; CHECK(dir.isValid());
    vtkNew<vtkXMLDataSetWriter> xmlWriter;
    // Match the extension to the actual dataset type.
    const QString type=QString::fromLatin1(vtk.dataset.regions[0].data->GetClassName());
    const QString extension=type==QStringLiteral("vtkPolyData") ? QStringLiteral("vtp")
        : type==QStringLiteral("vtkImageData") || type==QStringLiteral("vtkStructuredPoints") ? QStringLiteral("vti")
        : type==QStringLiteral("vtkStructuredGrid") ? QStringLiteral("vts")
        : type==QStringLiteral("vtkRectilinearGrid") ? QStringLiteral("vtr") : QStringLiteral("vtu");
    const QString xmlName=dir.filePath(QStringLiteral("sample.")+extension);
    const QByteArray xmlBytes=QFile::encodeName(xmlName); xmlWriter->SetFileName(xmlBytes.constData());
    xmlWriter->SetInputData(vtk.dataset.regions[0].data); CHECK(xmlWriter->Write());
    auto xml=adapter->read(xmlName); CHECK(xml.ok()); CHECK(xml.dataset.regions[0].data->GetNumberOfPoints()==vtk.dataset.regions[0].data->GetNumberOfPoints());
    vtkNew<vtkMultiBlockDataSet> blocks; blocks->SetNumberOfBlocks(2);
    blocks->SetBlock(0,flow.dataset.regions[0].data); blocks->SetBlock(1,cells.dataset.regions[0].data);
    blocks->GetMetaData(0u)->Set(vtkCompositeDataSet::NAME(),"Flow"); blocks->GetMetaData(1u)->Set(vtkCompositeDataSet::NAME(),"Surface");
    const QString multiName=dir.filePath(QStringLiteral("blocks.vtm")); const QByteArray multiBytes=QFile::encodeName(multiName);
    vtkNew<vtkXMLMultiBlockDataWriter> multiWriter; multiWriter->SetFileName(multiBytes.constData()); multiWriter->SetInputData(blocks); CHECK(multiWriter->Write());
    auto multi=adapter->read(multiName); CHECK(multi.ok()); CHECK(multi.dataset.regions.size()==2); CHECK(multi.dataset.regions[0].name==QStringLiteral("Flow"));
    QFile broken(dir.filePath(QStringLiteral("broken.tec"))); CHECK(broken.open(QIODevice::WriteOnly)); broken.write("\0binary",7); broken.close();
    CHECK(!adapter->read(broken.fileName()).ok());
    QFile unquoted(dir.filePath(QStringLiteral("unquoted.dat"))); CHECK(unquoted.open(QIODevice::WriteOnly));
    unquoted.write("VARIABLES = X, Y, P\nZONE I=2\n0 0 1\n1 0 2\n"); unquoted.close();
    CHECK(!adapter->read(unquoted.fileName()).ok());
    qInfo() << "VTK and Tecplot reader checks passed"; return 0;
}
