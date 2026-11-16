# ParaView CGNS reader (experimental)

Source: Kitware ParaView v5.6.3, commit `587b66e78632f6d30443c990141c4a99dec74f0a`,
https://github.com/Kitware/ParaView/tree/v5.6.3/ParaViewCore/VTKExtensions .
The CGNSReader, SIL and Core files come from upstream; `vtkCGNSReaderInternal.cxx`
has a local change to discover solution arrays from every zone instead of just
the first; `vtkCGNSReader.cxx` accepts and validates vertex PointRange boundaries
independently of physical BC type. `LICENSE.txt` is the upstream license. `vtkPVVTKExtensions*Module.h` are local static-build export
headers. `vtk_cgns.h` is a local compatibility shim for CGNS 4.1+ removed CGIO
entry points. Integration must preserve upstream notices and be reviewed before
shipping. It requires VTK 8.2 (including vtkParallelCore, vtkFiltersExtraction,
vtkpugixml), CGNS with matching ABI, and HDF5. Do not mix separate CGNS libraries.
