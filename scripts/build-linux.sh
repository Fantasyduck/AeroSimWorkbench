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
    "-DAEROSIM_BUILD_GUI=${AEROSIM_BUILD_GUI:-ON}"
    "-DAEROSIM_BUILD_STACK_PROBE=${AEROSIM_BUILD_STACK_PROBE:-OFF}"
)
if [[ "${AEROSIM_BUILD_STACK_PROBE:-OFF}" == ON ]]; then
    cmake_args+=("-DVTK_DIR=${AEROSIM_VTK_DIR:-}"
                 "-DSARIBBON_SOURCE_DIR=${AEROSIM_SARIBBON_SOURCE_DIR:-}")
fi
cmake -S "${repo_root}" -B "${build_dir}" "${cmake_args[@]}"

if [[ "${target}" == all ]]; then
    cmake --build "${build_dir}" --parallel "${AEROSIM_JOBS:-6}"
    (cd "${build_dir}" && ctest --output-on-failure)
else
    cmake --build "${build_dir}" --target "${target}" --parallel "${AEROSIM_JOBS:-6}"
fi
