#!/usr/bin/env bash
set -euo pipefail

# Builds OpenTOPAS with this repository as TOPAS extension set.
#
# Usage:
#   scripts/build_topas_with_extension.sh
#   TOPAS_SOURCE_DIR=/path/to/OpenTOPAS scripts/build_topas_with_extension.sh
#   TOPAS_BUILD_DIR=/tmp/OpenTOPAS-build-topas-rpt scripts/build_topas_with_extension.sh
#   TOPAS_INSTALL_PREFIX=/tmp/OpenTOPAS-install-topas-rpt scripts/build_topas_with_extension.sh --install
#   TOPAS_USE_QT=OFF scripts/build_topas_with_extension.sh
#
# Notes:
# - TOPAS extension loader flattens extension files into one directory. Keep unique basenames.
# - This script does not touch your OpenTOPAS checkout unless you pass --install,
#   and even then only writes to TOPAS_INSTALL_PREFIX.
# - Qt: TOPAS_USE_QT=auto (default), ON or OFF. auto follows the Geant4 build, read from
#   Geant4_qt_FOUND in ${Geant4_DIR}/Geant4Config.cmake: ON when Geant4 has Qt, OFF when it
#   does not. Both mismatches fail: ON against a Geant4 without Qt does not compile
#   ("qobject.h" not found), and OFF against a Geant4 with Qt does not link ("symbol(s) not
#   found ... TsQt5::..."), since the Geant4 targets still define G4UI_USE_QT for OpenTOPAS.
#   OpenTOPAS's own detection misses some Qt builds of Geant4 (Qt6 builds exporting only
#   Geant4_qt_FOUND), so it is not relied on. ON or OFF overrides the choice.

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

TOPAS_USE_QT="${TOPAS_USE_QT:-auto}"
QT_ARGS=()
case "${TOPAS_USE_QT}" in
  auto)
    GEANT4_CONFIG_FILE="${Geant4_DIR}/Geant4Config.cmake"
    GEANT4_QT="$(sed -n 's/^set(Geant4_qt_FOUND \(.*\))$/\1/p' "${GEANT4_CONFIG_FILE}" 2>/dev/null | head -1 || true)"
    case "$(echo "${GEANT4_QT}" | tr '[:lower:]' '[:upper:]')" in
      ON|TRUE|YES|1)
        QT_ARGS=(-DTOPAS_USE_QT=ON)
        QT_DESCRIPTION="ON (Geant4 built with Qt)"
        ;;
      "")
        QT_DESCRIPTION="left to OpenTOPAS (no Geant4_qt_FOUND in ${GEANT4_CONFIG_FILE})"
        ;;
      *)
        QT_ARGS=(-DTOPAS_USE_QT=OFF)
        QT_DESCRIPTION="OFF (Geant4 built without Qt)"
        ;;
    esac
    ;;
  ON|OFF)
    QT_ARGS=(-DTOPAS_USE_QT="${TOPAS_USE_QT}")
    QT_DESCRIPTION="${TOPAS_USE_QT}"
    ;;
  *)
    echo "ERROR: TOPAS_USE_QT must be auto, ON or OFF (got: ${TOPAS_USE_QT})."
    exit 2
    ;;
esac

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
echo "TOPAS_USE_QT         : ${QT_DESCRIPTION}"
echo

cmake -S "${TOPAS_SOURCE_DIR}" -B "${TOPAS_BUILD_DIR}" \
  -G "${CMAKE_GENERATOR}" \
  -DGeant4_DIR="${Geant4_DIR}" \
  -DGDCM_DIR="${GDCM_DIR}" \
  -DTOPAS_EXTENSIONS_DIR="${REPO_ROOT}" \
  ${QT_ARGS[@]+"${QT_ARGS[@]}"} \
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
