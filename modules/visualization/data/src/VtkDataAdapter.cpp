#include "aerosim/visualization/DataAdapter.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <vtkCallbackCommand.h>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkCompositeDataSet.h>
#include <vtkDataArray.h>
#include <vtkDataSetAttributes.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkInformation.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTecplotReader.h>
#include <vtkXMLGenericDataObjectReader.h>
#include <vtkXMLMultiBlockDataReader.h>
#include <cmath>

namespace aerosim { namespace visualization {
namespace {
void vtkError(vtkObject *, unsigned long, void *client, void *message) {
    auto *error = static_cast<QString *>(client);
    if (error->isEmpty()) *error = message ? QString::fromLocal8Bit(static_cast<const char *>(message)).trimmed() : QStringLiteral("VTK reader failed");
}
void fields(RegionData &region, vtkDataSetAttributes *attributes, const QString &association) {
    for (int i=0; i<attributes->GetNumberOfArrays(); ++i) {
        auto *array=attributes->GetArray(i);
        if (!array || !array->GetName() || !array->GetNumberOfTuples()) continue;
        FieldInfo field; field.name=QString::fromUtf8(array->GetName()); field.association=association;
        field.components=array->GetNumberOfComponents();
        double range[2]; array->GetRange(range, field.components==1 ? 0 : -1);
        field.hasRange=std::isfinite(range[0]) && std::isfinite(range[1]) && range[0]<=range[1];
        if (field.hasRange) { field.minimum=range[0]; field.maximum=range[1]; }
        region.fields.append(field);
    }
}
bool collect(vtkDataObject *object, const QString &id, const QString &name, ImportedData &out, QString &error) {
    if (auto *blocks=vtkMultiBlockDataSet::SafeDownCast(object)) {
        for (unsigned int i=0; i<blocks->GetNumberOfBlocks(); ++i) {
            auto *block=blocks->GetBlock(i); if (!block) continue;
            QString label=QStringLiteral("Block %1").arg(i+1);
            if (blocks->HasMetaData(i) && blocks->GetMetaData(i)->Has(vtkCompositeDataSet::NAME()))
                label=QString::fromUtf8(blocks->GetMetaData(i)->Get(vtkCompositeDataSet::NAME()));
            if (!collect(block,id+QStringLiteral("/")+QString::number(i),name.isEmpty()?label:name+QStringLiteral(" / ")+label,out,error)) return false;
        }
        return true;
    }
    auto *data=vtkDataSet::SafeDownCast(object);
    if (!data) { error=QStringLiteral("Unsupported VTK block type: ")+name; return false; }
    if (!data->GetNumberOfPoints() || !data->GetNumberOfCells()) { error=QStringLiteral("Empty dataset: ")+name; return false; }
    RegionData region; region.id=id; region.name=name.isEmpty()?QStringLiteral("Data"):name; region.data=data;
    for (vtkIdType i=0; i<data->GetNumberOfCells(); ++i) region.dimension=qMax(region.dimension,data->GetCell(i)->GetCellDimension());
    region.kind=region.dimension==3 ? QStringLiteral("volume") : QStringLiteral("section");
    fields(region,data->GetPointData(),QStringLiteral("point")); fields(region,data->GetCellData(),QStringLiteral("cell"));
    out.regions.append(region); return true;
}
class VtkDataAdapter final : public DataAdapter {
public:
    bool accepts(const QString &path) const override {
        const QString suffix=QFileInfo(path).suffix().toLower();
        return QStringList({QStringLiteral("vtk"),QStringLiteral("vti"),QStringLiteral("vtu"),QStringLiteral("vts"),QStringLiteral("vtr"),QStringLiteral("vtp"),QStringLiteral("vtm"),QStringLiteral("dat"),QStringLiteral("tec")}).contains(suffix);
    }
    ImportResult read(const QString &path) const override {
        ImportResult result; result.dataset.path=path;
        if (!accepts(path)) { result.error=QStringLiteral("Unsupported format: ")+path; return result; }
        if (!QFileInfo(path).isFile()) { result.error=QStringLiteral("File does not exist: ")+path; return result; }
        const QByteArray filename=QFile::encodeName(path); const QString suffix=QFileInfo(path).suffix().toLower();
        vtkNew<vtkCallbackCommand> observer; observer->SetClientData(&result.error); observer->SetCallback(vtkError);
        vtkSmartPointer<vtkDataObject> output;
        if (suffix==QStringLiteral("tec") || suffix==QStringLiteral("dat")) {
            QFile file(path); if (!file.open(QIODevice::ReadOnly)) { result.error=file.errorString(); return result; }
            if (file.peek(512).contains('\0')) { result.error=QStringLiteral("Tecplot binary files are not supported; use ASCII Tecplot"); return result; }
            // VTK 8.2 may loop indefinitely while parsing unquoted variable names.
            // Refuse this subset before calling it instead of hanging the worker.
            const QString header=QString::fromLatin1(file.peek(1024*1024));
            const QRegularExpression variables(QStringLiteral("\\bVARIABLES\\s*=\\s*([^\\s])"), QRegularExpression::CaseInsensitiveOption);
            const auto match=variables.match(header);
            if (match.hasMatch() && match.captured(1)!=QStringLiteral("\"")) {
                result.error=QStringLiteral("Tecplot VARIABLES must use quoted names with VTK 8.2; unquoted names are not supported"); return result;
            }
            vtkNew<vtkTecplotReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer); reader->SetFileName(filename.constData());
            reader->UpdateInformation();
            for (int i=0; i<reader->GetNumberOfDataArrays(); ++i) reader->SetDataArrayStatus(reader->GetDataArrayName(i),1);
            reader->Update(); output=reader->GetOutput();
        } else if (suffix==QStringLiteral("vtk")) {
            vtkNew<vtkGenericDataObjectReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer); reader->SetFileName(filename.constData());
            reader->ReadAllScalarsOn(); reader->ReadAllVectorsOn(); reader->ReadAllTensorsOn(); reader->ReadAllFieldsOn(); reader->Update(); output=reader->GetOutput();
        } else if (suffix==QStringLiteral("vtm")) {
            vtkNew<vtkXMLMultiBlockDataReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer); reader->SetFileName(filename.constData()); reader->Update(); output=reader->GetOutput();
        } else {
            vtkNew<vtkXMLGenericDataObjectReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer); reader->SetFileName(filename.constData()); reader->Update(); output=reader->GetOutput();
        }
        if (result.error.isEmpty() && (!output || !collect(output,QStringLiteral("0"),QString(),result.dataset,result.error))) {
            if (result.error.isEmpty()) result.error=QStringLiteral("Unable to read dataset: ")+path;
        }
        if (result.error.isEmpty() && result.dataset.regions.isEmpty()) result.error=QStringLiteral("File contains no readable datasets: ")+path;
        if (!result.error.isEmpty()) result.dataset.regions.clear();
        return result;
    }
};
}
std::unique_ptr<DataAdapter> createVtkDataAdapter() { return std::unique_ptr<DataAdapter>(new VtkDataAdapter); }
} }
