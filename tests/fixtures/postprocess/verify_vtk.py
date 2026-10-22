#!/usr/bin/env python3
"""Optional VTK runtime check; run with the Demo's Python VTK installation."""
import sys
from pathlib import Path

try:
    import vtk
except ImportError:
    sys.exit('VTK Python bindings are required for this optional check')

root = Path(__file__).resolve().parent
for t in (0, 1, 2):
    reader = vtk.vtkStructuredPointsReader()
    reader.SetFileName(str(root / 'cube_t{:02d}.vtk'.format(t)))
    reader.ReadAllScalarsOn()
    reader.ReadAllVectorsOn()
    reader.Update()
    mesh = reader.GetOutput()
    data = mesh.GetPointData()
    assert mesh.GetDimensions() == (3, 3, 3)
    assert mesh.GetNumberOfPoints() == 27 and mesh.GetNumberOfCells() == 8
    assert {data.GetArrayName(i) for i in range(data.GetNumberOfArrays())} == {
        'Pressure', 'Temperature', 'Phi', 'Velocity'}
    assert data.GetArray('Pressure').GetTuple1(13) == 100000 + 100 * t
    assert data.GetArray('Temperature').GetTuple1(13) == 300
    assert data.GetArray('Phi').GetTuple1(13) == 0
    assert data.GetArray('Velocity').GetTuple3(13) == (1, 0, 0)
    if t == 0:
        iso = vtk.vtkContourFilter()
        iso.SetInputData(mesh)
        iso.SetInputArrayToProcess(0, 0, 0,
                                   vtk.vtkDataObject.FIELD_ASSOCIATION_POINTS, 'Phi')
        iso.SetValue(0, 0)
        iso.Update()
        assert iso.GetOutput().GetNumberOfPoints() > 0
        assert iso.GetOutput().GetBounds() == (0, 0, -1, 1, -1, 1)

        plane = vtk.vtkPlane()
        plane.SetOrigin(0, 0, 0)
        plane.SetNormal(1, 0, 0)
        cut = vtk.vtkCutter()
        cut.SetInputData(mesh)
        cut.SetCutFunction(plane)
        cut.Update()
        assert cut.GetOutput().GetNumberOfPoints() > 0
        assert cut.GetOutput().GetBounds() == (0, 0, -1, 1, -1, 1)

        trace = vtk.vtkStreamTracer()
        trace.SetInputData(mesh)
        trace.SetInputArrayToProcess(0, 0, 0,
                                     vtk.vtkDataObject.FIELD_ASSOCIATION_POINTS,
                                     'Velocity')
        trace.SetStartPosition(-0.9, 0, 0)
        trace.SetIntegrationDirectionToForward()
        trace.SetMaximumPropagation(2)
        trace.Update()
        path = trace.GetOutput()
        assert path.GetNumberOfPoints() > 1
        assert all(path.GetPoint(i)[0] < path.GetPoint(i + 1)[0]
                   for i in range(path.GetNumberOfPoints() - 1))
        assert all(abs(path.GetPoint(i)[1]) < 1e-6 and
                   abs(path.GetPoint(i)[2]) < 1e-6
                   for i in range(path.GetNumberOfPoints()))
print('VTK read OK: 3 frames, 27 points, 8 cells, four arrays; VTK {}'.format(
    vtk.vtkVersion.GetVTKVersion()))
