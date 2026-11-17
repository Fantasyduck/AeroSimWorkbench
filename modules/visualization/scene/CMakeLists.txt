# Both viewers share this directory but are independently selectable.
#
# The workbench viewer is built only when the shared CGNS adapter exists: it
# calls createCgnsAdapter() and takes DataAdapter.h from that target's public
# include directory. modules/visualization/data leaves the adapter out when VTK
# or CGNS is missing, so gating here keeps a CGNS-less configure working; app/
# already links this target through a TARGET check and simply skips the viewer.
if(AEROSIM_BUILD_VISUALIZATION)
    if(TARGET aerosim_visualization_data)
        find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets OpenGL Concurrent)
        find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS vtkCommonCore vtkCommonDataModel vtkGUISupportQt vtkInteractionStyle vtkRenderingCore vtkRenderingOpenGL2)
        add_library(aerosim_visualization_scene STATIC src/VtkSceneViewer.cpp)
        target_include_directories(aerosim_visualization_scene PUBLIC include ${VTK_INCLUDE_DIRS})
        target_link_libraries(aerosim_visualization_scene PUBLIC aerosim_visualization_data aerosim_ui Qt5::Widgets Qt5::OpenGL Qt5::Concurrent ${VTK_LIBRARIES})
        if(AEROSIM_BUILD_TESTS)
            add_executable(aerosim_cgns_workbench_test ../tests/CgnsWorkbenchTest.cpp)
            target_link_libraries(aerosim_cgns_workbench_test PRIVATE aerosim_visualization_scene)
            add_executable(aerosim_cgns_region_render_test ../tests/CgnsRegionRenderTest.cpp)
            target_link_libraries(aerosim_cgns_region_render_test PRIVATE aerosim_visualization_scene)
        endif()
    else()
        message(STATUS "aerosim_visualization_scene skipped: no CGNS adapter target "
            "(see modules/visualization/data)")
    endif()
endif()

if(AEROSIM_BUILD_MESH_VIEWER)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS OpenGL)
    find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
        vtkCommonCore vtkCommonDataModel vtkCommonExecutionModel
        vtkFiltersGeometry vtkIOLegacy vtkIOXML
        vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkInteractionStyle)

    set(mesh_root "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_library(aerosim_mesh_boundary STATIC
        "${mesh_root}/src/data/BoundaryLoadService.cpp"
        "${mesh_root}/src/data/BoundaryMeshReader.cpp"
        "${mesh_root}/src/features/BoundarySurfaceExtractor.cpp"
        "${mesh_root}/include/aerosim/visualization/data/BoundaryLoadService.h")
    target_include_directories(aerosim_mesh_boundary PUBLIC "${mesh_root}/include")
    target_include_directories(aerosim_mesh_boundary SYSTEM PUBLIC ${VTK_INCLUDE_DIRS})
    target_link_libraries(aerosim_mesh_boundary PUBLIC Qt5::Core vtkCommonCore vtkCommonDataModel
        PRIVATE vtkCommonExecutionModel vtkFiltersGeometry vtkIOLegacy vtkIOXML)
    if(MSVC)
        target_compile_definitions(aerosim_mesh_boundary PRIVATE
            "$<$<NOT:$<CONFIG:Debug>>:AEROSIM_VTK_DEBUG_ONLY>")
    endif()

    if(TARGET aerosim_visualization_data)
        # Both viewers use the same adapter; do not compile it twice into the app.
        target_link_libraries(aerosim_mesh_boundary PRIVATE aerosim_visualization_data)
        target_compile_definitions(aerosim_mesh_boundary PUBLIC AEROSIM_MESH_HAS_CGNS=1)
    else()
        set(AEROSIM_CGNS_ROOT "" CACHE PATH "Optional CGNS Debug installation or build prefix")
        find_path(AEROSIM_CGNS_INCLUDE_DIR cgnslib.h
            HINTS "${AEROSIM_CGNS_ROOT}/include" "${AEROSIM_CGNS_ROOT}/src")
        find_library(AEROSIM_CGNS_LIBRARY NAMES cgnsdll cgns
            HINTS "${AEROSIM_CGNS_ROOT}/lib" "${AEROSIM_CGNS_ROOT}/src/Debug")
        if(AEROSIM_CGNS_INCLUDE_DIR AND AEROSIM_CGNS_LIBRARY)
            target_sources(aerosim_mesh_boundary PRIVATE "${mesh_root}/data/src/CgnsAdapter.cpp")
            target_include_directories(aerosim_mesh_boundary PRIVATE
                "${mesh_root}/data/include" "${AEROSIM_CGNS_INCLUDE_DIR}")
            target_link_libraries(aerosim_mesh_boundary PRIVATE "${AEROSIM_CGNS_LIBRARY}")
            target_compile_definitions(aerosim_mesh_boundary PUBLIC AEROSIM_MESH_HAS_CGNS=1)
            if(WIN32 AND AEROSIM_CGNS_LIBRARY MATCHES "cgnsdll")
                target_compile_definitions(aerosim_mesh_boundary PRIVATE USE_DLL)
            endif()
        endif()
    endif()

    add_library(aerosim_mesh_scene STATIC
        "${mesh_root}/src/scene/MeshSceneViewer.cpp"
        "${mesh_root}/include/aerosim/visualization/scene/MeshSceneViewer.h")
    target_link_libraries(aerosim_mesh_scene PUBLIC aerosim_mesh_boundary Qt5::Widgets
        PRIVATE Qt5::OpenGL vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkInteractionStyle)

    if(AEROSIM_BUILD_TESTS)
        add_executable(aerosim_mesh_boundary_tests "${mesh_root}/tests/BoundaryTests.cpp")
        target_link_libraries(aerosim_mesh_boundary_tests PRIVATE aerosim_mesh_boundary vtkIOXML)
        target_compile_definitions(aerosim_mesh_boundary_tests PRIVATE
            "AEROSIM_FIXTURE_DIR=\"${PROJECT_SOURCE_DIR}/tests/fixtures/postprocess\"")
        add_test(NAME mesh_boundary_geometry COMMAND aerosim_mesh_boundary_tests)
    endif()
endif()
