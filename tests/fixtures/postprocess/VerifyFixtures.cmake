# Data-only integrity and analytical metadata check; works with CMake 3.16+.
# This does not replace a future C++/VTK 8.2 reader or algorithm test.
if(NOT POSTPROCESS_FIXTURE_DIR)
    set(POSTPROCESS_FIXTURE_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

set(_fixtures
    "mesh.json|6588e1e35dad312b973276c2970c548ba06f502d15ab26dcb28feb6d55f2c330"
    "index.json|b878784738ba5ca735051eb595f9d5802d85c6b395b66e0f2dda9b9c2e388954"
    "cases.csv|e5bd80653dc10e779a2744d795de41d05834c3ec977105409295453659e066ac"
    "curves.csv|5dfa4fd5ad4926e1b1843f358c306ab89fe3873bc53b2292454d1d8e26eedc71"
    "cube_t00.vtk|c35d1c2ad00c05ca1e2deb989ef47d302781850cd8fc70b2bce8aa269b947906"
    "cube_t01.vtk|f75e6452c4d7c6d279fe60ed31fe6e7e53e3244ce15e8bc9f6c0d6a8821dbe60"
    "cube_t02.vtk|adf4d4de61fdd4cace47bd9c5849a94d9b9d4c8cd7ae269525f10c96e079fdf5")

foreach(_fixture IN LISTS _fixtures)
    string(REPLACE "|" ";" _parts "${_fixture}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _expected)
    set(_path "${POSTPROCESS_FIXTURE_DIR}/${_name}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Missing postprocess fixture: ${_path}")
    endif()
    # Git may check out these text fixtures with CRLF on Windows. Hash their
    # canonical LF representation so the same data passes on both platforms.
    file(READ "${_path}" _content)
    string(REPLACE "\r\n" "\n" _content "${_content}")
    string(SHA256 _actual "${_content}")
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "Postprocess fixture changed: ${_name}; review its expected values and SHA256")
    endif()
endforeach()

file(STRINGS "${POSTPROCESS_FIXTURE_DIR}/mesh.json" _boundaries REGEX "\"boundaryId\":")
list(LENGTH _boundaries _boundary_count)
if(NOT _boundary_count EQUAL 6)
    message(FATAL_ERROR "Expected six boundary groups, found ${_boundary_count}")
endif()

file(STRINGS "${POSTPROCESS_FIXTURE_DIR}/index.json" _times REGEX "\"timeValue\":")
list(LENGTH _times _frame_count)
if(NOT _frame_count EQUAL 3)
    message(FATAL_ERROR "Expected three time frames, found ${_frame_count}")
endif()
foreach(_time IN ITEMS 0 1 2)
    list(GET _times ${_time} _entry)
    if(NOT _entry MATCHES "\"timeValue\": ${_time}")
        message(FATAL_ERROR "Frame ${_time} has the wrong physical time")
    endif()
endforeach()

file(STRINGS "${POSTPROCESS_FIXTURE_DIR}/cases.csv" _cases REGEX "^case-")
list(LENGTH _cases _case_count)
if(NOT _case_count EQUAL 4)
    message(FATAL_ERROR "Expected four analytical cases, found ${_case_count}")
endif()
list(GET _cases 2 _case_02)
if(NOT _case_02 STREQUAL "case-02,5,0,10,1000,2,0.5,170,0,130,0,20,0")
    message(FATAL_ERROR "case-02 six-component expectation changed")
endif()

file(STRINGS "${POSTPROCESS_FIXTURE_DIR}/curves.csv" _curve_02 REGEX ",case-02,")
if(NOT _curve_02 MATCHES ",0\\.06500$")
    message(FATAL_ERROR "case-02 analytical curve expectation changed")
endif()

foreach(_frame IN ITEMS 00 01 02)
    set(_path "${POSTPROCESS_FIXTURE_DIR}/cube_t${_frame}.vtk")
    file(READ "${_path}" _vtk)
    foreach(_token IN ITEMS "DATASET STRUCTURED_POINTS" "DIMENSIONS 3 3 3"
                             "POINT_DATA 27" "SCALARS Pressure" "SCALARS Temperature"
                             "SCALARS Phi" "VECTORS Velocity")
        string(FIND "${_vtk}" "${_token}" _at)
        if(_at EQUAL -1)
            message(FATAL_ERROR "${_path} lacks ${_token}")
        endif()
    endforeach()
endforeach()

message(STATUS "Postprocess fixtures OK: 7 files, 27 points, 8 cells, 6 boundaries, 3 frames, 4 cases")
