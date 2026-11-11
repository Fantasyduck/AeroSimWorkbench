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
    "cube_t02.vtk|adf4d4de61fdd4cace47bd9c5849a94d9b9d4c8cd7ae269525f10c96e079fdf5"
    "su2_history_steady.csv|082addc2296febe9357c041c5d168b22a4d7a16033274f5a6a478e57bea27754"
    "xcfd_history_tec.tec|0483728bf65c6de1de5b769fafc99426005c4e17d1aa60f67ab922f7577d2bad")

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

# Solver history log: a trimmed export of a real SU2 run. Asserting its shape
# keeps the reader tests meaningful -- they quote specific values from row 1 and
# the last row, so a re-trimmed or re-ordered fixture has to be a deliberate edit.
file(READ "${POSTPROCESS_FIXTURE_DIR}/su2_history_steady.csv" _history)
string(REPLACE "\r\n" "\n" _history "${_history}")
string(REPLACE "\r" "\n" _history "${_history}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/su2_history_normalised.csv" "${_history}")
file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/su2_history_normalised.csv" _history_lines)
list(LENGTH _history_lines _history_line_count)
if(NOT _history_line_count EQUAL 80)
    message(FATAL_ERROR "Expected 80 monitor lines (1 header + 79 data rows), found ${_history_line_count}")
endif()
list(GET _history_lines 0 _history_header)
string(REGEX MATCHALL "," _history_commas "${_history_header}")
list(LENGTH _history_commas _history_comma_count)
if(NOT _history_comma_count EQUAL 16)
    message(FATAL_ERROR "Expected 17 monitor columns, found ${_history_comma_count}+1")
endif()
foreach(_token IN ITEMS "Time_Iter" "Outer_Iter" "Inner_Iter" "rms[Rho]" "CD" "CL" "CEff")
    string(FIND "${_history_header}" "${_token}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "su2_history_steady.csv lacks column ${_token}")
    endif()
endforeach()
list(GET _history_lines 79 _history_last)
# Columns are space-padded fixed-width fields ("          0,           0,         325,..."),
# so the match has to tolerate the padding rather than assume "0,0,325,".
if(NOT _history_last MATCHES ", *0, *325,")
    message(FATAL_ERROR "The last monitor row no longer advances to Inner_Iter 325")
endif()

# Tecplot ASCII history: the format XCFD writes its convergence and residual
# curves in. The reader tests quote the first and last row of each ZONE and rely
# on the Fortran D exponents and the two-ZONE split, so the shape is asserted here
# as well -- a re-trimmed fixture has to be a deliberate edit.
file(READ "${POSTPROCESS_FIXTURE_DIR}/xcfd_history_tec.tec" _tecplot)
string(REPLACE "\r\n" "\n" _tecplot "${_tecplot}")
string(REPLACE "\r" "\n" _tecplot "${_tecplot}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/xcfd_history_normalised.tec" "${_tecplot}")
file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/xcfd_history_normalised.tec" _tecplot_lines)
list(LENGTH _tecplot_lines _tecplot_line_count)
if(NOT _tecplot_line_count EQUAL 14)
    message(FATAL_ERROR "Expected 14 Tecplot lines (3 header + 2 zones x (1 record + 5 rows)), found ${_tecplot_line_count}")
endif()
list(GET _tecplot_lines 1 _tecplot_variables)
foreach(_token IN ITEMS "Iter" "rms[Rho]" "CD" "CL" "CEff")
    string(FIND "${_tecplot_variables}" "\"${_token}\"" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "xcfd_history_tec.tec lacks variable ${_token}")
    endif()
endforeach()
file(STRINGS "${POSTPROCESS_FIXTURE_DIR}/xcfd_history_tec.tec" _tecplot_zones REGEX "^ZONE ")
list(LENGTH _tecplot_zones _tecplot_zone_count)
if(NOT _tecplot_zone_count EQUAL 2)
    message(FATAL_ERROR "Expected two ZONE records (a run continued over a restart), found ${_tecplot_zone_count}")
endif()
foreach(_zone IN LISTS _tecplot_zones)
    if(NOT _zone MATCHES "I=5")
        message(FATAL_ERROR "Every ZONE must declare five steps: ${_zone}")
    endif()
endforeach()
file(STRINGS "${POSTPROCESS_FIXTURE_DIR}/xcfd_history_tec.tec" _tecplot_d REGEX "D[+-]")
list(LENGTH _tecplot_d _tecplot_d_count)
if(NOT _tecplot_d_count EQUAL 10)
    message(FATAL_ERROR "Expected ten Fortran-exponent data rows, found ${_tecplot_d_count}")
endif()

message(STATUS "Postprocess fixtures OK: 9 files, 27 points, 8 cells, 6 boundaries, 3 frames, 4 cases, 80 monitor lines (79 data rows)")
