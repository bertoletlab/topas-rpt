#!/usr/bin/env bash
set -euo pipefail

# Builds OpenTOPAS with this repository as TOPAS extension set.
#
# Usage:
#   scripts/build_topas_with_extension.sh
#   TOPAS_SOURCE_DIR=/path/to/OpenTOPAS scripts/build_topas_with_extension.sh
#   TOPAS_BUILD_DIR=/tmp/OpenTOPAS-build-topas-rpt scripts/build_topas_with_extension.sh
#   TOPAS_INSTALL_PREFIX=/tmp/OpenTOPAS-install-topas-rpt scripts/build_topas_with_extension.sh --install
#
# Notes:
# - TOPAS extension loader flattens extension files into one directory. Keep unique basenames.
# - This script does not touch your OpenTOPAS checkout unless you pass --install,
#   and even then only writes to TOPAS_INSTALL_PREFIX.
# - -DTOPAS_USE_QT=ON is required: OpenTOPAS's TsSequenceManager references TsQt5
#   unconditionally, but TsQt5.cc (and its AUTOMOC output) is only compiled into the build
#   when TOPAS_USE_QT is ON. Leaving it unset/OFF configures cleanly but fails at link time
#   ("symbol(s) not found for architecture ... TsQt5::...").

INSTALL_AFTER_BUILD=0
if [[ "${1:-}" == "--install" ]]; then
  INSTALL_AFTER_BUILD=1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ -z "${TOPAS_SOURCE_DIR:-}" ]]; then
  echo "ERROR: TOPAS_SOURCE_DIR not set. Point it at your OpenTOPAS checkout."
  exit 2
fi

TOPAS_BUILD_DIR="${TOPAS_BUILD_DIR:-/tmp/OpenTOPAS-build-topas-rpt}"
TOPAS_INSTALL_PREFIX="${TOPAS_INSTALL_PREFIX:-/tmp/OpenTOPAS-install-topas-rpt}"

if [[ -z "${Geant4_DIR:-}" ]]; then
  echo "ERROR: Geant4_DIR not set. Point it at your Geant4 CMake package directory"
  echo "(e.g. /path/to/geant4-install/lib/cmake/Geant4)."
  exit 2
fi

if [[ -z "${GDCM_DIR:-}" ]]; then
  echo "ERROR: GDCM_DIR not set. Point it at your GDCM CMake package directory"
  echo "(e.g. /path/to/gdcm-install/lib/gdcm-2.6)."
  exit 2
fi

if command -v ninja >/dev/null 2>&1; then
  CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
else
  CMAKE_GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
fi

echo "== TOPAS extension build configuration =="
echo "TOPAS_SOURCE_DIR     : ${TOPAS_SOURCE_DIR}"
echo "TOPAS_BUILD_DIR      : ${TOPAS_BUILD_DIR}"
echo "TOPAS_INSTALL_PREFIX : ${TOPAS_INSTALL_PREFIX}"
echo "TOPAS_EXTENSIONS_DIR : ${REPO_ROOT}"
echo "Geant4_DIR           : ${Geant4_DIR}"
echo "GDCM_DIR             : ${GDCM_DIR}"
echo "CMAKE_GENERATOR      : ${CMAKE_GENERATOR}"
echo

cmake -S "${TOPAS_SOURCE_DIR}" -B "${TOPAS_BUILD_DIR}" \
  -G "${CMAKE_GENERATOR}" \
  -DGeant4_DIR="${Geant4_DIR}" \
  -DGDCM_DIR="${GDCM_DIR}" \
  -DTOPAS_EXTENSIONS_DIR="${REPO_ROOT}" \
  -DTOPAS_USE_QT=ON \
  -DCMAKE_INSTALL_PREFIX="${TOPAS_INSTALL_PREFIX}"

cmake --build "${TOPAS_BUILD_DIR}" -j

BUILT_TOPAS_BIN="${TOPAS_BUILD_DIR}/topas"
if [[ ! -x "${BUILT_TOPAS_BIN}" && -x "${TOPAS_BUILD_DIR}/bin/topas" ]]; then
  BUILT_TOPAS_BIN="${TOPAS_BUILD_DIR}/bin/topas"
fi

if [[ "${INSTALL_AFTER_BUILD}" -eq 1 ]]; then
  cmake --install "${TOPAS_BUILD_DIR}"
  echo "Installed topas to: ${TOPAS_INSTALL_PREFIX}"
  echo "Binary: ${TOPAS_INSTALL_PREFIX}/bin/topas"
else
  echo "Build complete."
  echo "Binary (build tree): ${BUILT_TOPAS_BIN}"
fi
