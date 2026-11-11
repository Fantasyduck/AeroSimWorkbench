#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${AEROSIM_QT_ROOT:?Set AEROSIM_QT_ROOT to a Qt 5.12.12 installation}"
build_dir="${AEROSIM_BUILD_DIR:-${repo_root}/build-linux}"
configuration="${AEROSIM_CONFIG:-Debug}"
target="${1:-all}"
export LD_LIBRARY_PATH="${AEROSIM_QT_ROOT}/lib${AEROSIM_VTK_LIB:+:${AEROSIM_VTK_LIB}}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

cmake_args=(
    "-DCMAKE_BUILD_TYPE=${configuration}"
    "-DCMAKE_PREFIX_PATH=${AEROSIM_QT_ROOT}"
    "-DSARIBBON_SOURCE_DIR=${AEROSIM_SARIBBON_SOURCE_DIR:-}"
    "-DAEROSIM_BUILD_GUI=${AEROSIM_BUILD_GUI:-ON}"
    "-DAEROSIM_BUILD_STACK_PROBE=${AEROSIM_BUILD_STACK_PROBE:-OFF}"
)
# AEROSIM_BUILD_VISUALIZATION is passed through only when the caller sets it, so
# the project default (visualization follows the GUI) is not overridden with OFF
# from here. Qwt and Eigen are what the chart layer needs: hand their roots over
# whenever they are provided, otherwise CMake names what is missing at configure.
for _aer_opt in AEROSIM_BUILD_VISUALIZATION AEROSIM_QWT_ROOT AEROSIM_EIGEN_ROOT; do
    if [[ -n "${!_aer_opt:-}" ]]; then
        cmake_args+=("-D${_aer_opt}=${!_aer_opt}")
    fi
done
if [[ "${AEROSIM_BUILD_STACK_PROBE:-OFF}" == ON ]]; then
    : "${AEROSIM_VTK_DIR:?Set AEROSIM_VTK_DIR to the VTK 8.2.0 CMake package directory}"
    cmake_args+=("-DVTK_DIR=${AEROSIM_VTK_DIR}")
fi
cmake -S "${repo_root}" -B "${build_dir}" "${cmake_args[@]}"

if [[ "${target}" == all ]]; then
    cmake --build "${build_dir}" --parallel "${AEROSIM_JOBS:-6}"
    (cd "${build_dir}" && ctest --output-on-failure)
else
    cmake --build "${build_dir}" --target "${target}" --parallel "${AEROSIM_JOBS:-6}"
fi
