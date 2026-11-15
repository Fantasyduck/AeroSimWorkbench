#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
deps="${AEROSIM_DEPS_ROOT:-${HOME}/.local/share/aerosim-deps}"
qt="${AEROSIM_QT_ROOT:-${deps}/Qt/5.12.12/clang_64}"
ribbon="${AEROSIM_SARIBBON_SOURCE_DIR:-${deps}/SARibbon}"
qwt="${AEROSIM_QWT_ROOT:-${deps}/QWT}"
eigen="${AEROSIM_EIGEN_ROOT:-${deps}/eigen}"
vtk="${AEROSIM_VTK_DIR:-${deps}/vtk/lib/cmake/vtk-8.2}"
cgns="${AEROSIM_CGNS_ROOT:-${deps}/cgns}"
build="${AEROSIM_BUILD_DIR:-${root}/build-macos}"

for file in "${qt}/lib/cmake/Qt5/Qt5Config.cmake" "${ribbon}/src/SARibbonBar/SARibbonBar.h" "${qwt}/lib/libqwtplot.dylib" "${eigen}/Eigen/Dense" "${vtk}/VTKConfig.cmake" "${cgns}/include/cgnslib.h"; do
    if [[ ! -f "${file}" ]]; then
        echo "Missing dependency: ${file}" >&2
        exit 2
    fi
done

cmake -S "${root}" -B "${build}" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="${qt}" -DSARIBBON_SOURCE_DIR="${ribbon}" \
    -DAEROSIM_BUILD_VISUALIZATION=ON -DAEROSIM_BUILD_MESH_VIEWER=OFF \
    -DAEROSIM_QWT_ROOT="${qwt}" -DAEROSIM_EIGEN_ROOT="${eigen}" \
    -DVTK_DIR="${vtk}" -DAEROSIM_CGNS_ROOT="${cgns}"
cmake --build "${build}" --target aerosim_postprocess --parallel "${AEROSIM_JOBS:-6}"
exec "${build}/postprocess/aerosim_postprocess" "$@"
