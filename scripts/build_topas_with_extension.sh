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
# - Qt: TOPAS_USE_QT=auto (default), ON or OFF (CMake's spellings, any case, are accepted).
#   auto follows the Geant4 build, read from ${Geant4_DIR}/Geant4Config.cmake: with Qt it
#   passes -DTOPAS_USE_QT=ON, and -DTOPAS_USE_QT6=ON when that Qt is Qt6; without Qt it passes
#   both OFF (which also clears an ON left in a reused build directory). A mismatch fails:
#   ON against a Geant4 without Qt does not compile ("qobject.h" not found), and a Geant4 with
#   Qt that OpenTOPAS does not recognise (Qt6 builds exporting only Geant4_qt_FOUND) does not
#   link on TsQt5 symbols, since the Geant4 targets still define G4UI_USE_QT.
#   ON forces Qt (Qt6 when Geant4 uses it). OFF cannot remove Qt from a Geant4 that OpenTOPAS
#   itself detects as built with Qt: OpenTOPAS turns it back on.

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
case "$(echo "${TOPAS_USE_QT}" | tr '[:lower:]' '[:upper:]')" in
  AUTO) QT_MODE=auto ;;
  ON|TRUE|YES|Y|1) QT_MODE=ON ;;
  OFF|FALSE|NO|N|0) QT_MODE=OFF ;;
  *)
    echo "ERROR: TOPAS_USE_QT must be auto, ON or OFF (got: ${TOPAS_USE_QT})."
    exit 2
    ;;
esac

# Qt of the Geant4 build: GEANT4_QT is ON, OFF or empty (unknown); GEANT4_QT_MAJOR is 5, 6 or empty.
GEANT4_CONFIG_FILE="${Geant4_DIR}/Geant4Config.cmake"
GEANT4_QT=""
GEANT4_QT_MAJOR=""
if [[ -r "${GEANT4_CONFIG_FILE}" ]]; then
  case "$(sed -n 's/^set(Geant4_qt_FOUND \(.*\))$/\1/p' "${GEANT4_CONFIG_FILE}" | head -1 | tr '[:lower:]' '[:upper:]')" in
    ON|TRUE|YES|Y|1) GEANT4_QT=ON ;;
    "") ;;
    *) GEANT4_QT=OFF ;;
  esac
  GEANT4_QT_MAJOR="$(sed -n 's/.*find_dependency(Qt\([56]\)Core.*/\1/p' "${GEANT4_CONFIG_FILE}" | head -1)"
fi

QT_ARGS=()
case "${QT_MODE}" in
  auto)
    if [[ "${GEANT4_QT}" == ON ]]; then
      if [[ "${GEANT4_QT_MAJOR}" == 6 ]]; then
        QT_ARGS=(-DTOPAS_USE_QT=ON -DTOPAS_USE_QT6=ON)
        QT_DESCRIPTION="ON, Qt6 (Geant4 built with Qt6)"
      else
        QT_ARGS=(-DTOPAS_USE_QT=ON -DTOPAS_USE_QT6=OFF)
        QT_DESCRIPTION="ON (Geant4 built with Qt${GEANT4_QT_MAJOR:-5})"
      fi
    elif [[ "${GEANT4_QT}" == OFF ]]; then
      QT_ARGS=(-DTOPAS_USE_QT=OFF -DTOPAS_USE_QT6=OFF)
      QT_DESCRIPTION="OFF (Geant4 built without Qt)"
    elif [[ ! -r "${GEANT4_CONFIG_FILE}" ]]; then
      echo "WARNING: cannot read ${GEANT4_CONFIG_FILE}; Qt is left to OpenTOPAS's own detection."
      QT_DESCRIPTION="left to OpenTOPAS (Geant4Config.cmake not readable)"
      # Drop any Qt setting cached in a reused build directory, so OpenTOPAS decides afresh.
      QT_ARGS=(-UTOPAS_USE_QT -UTOPAS_USE_QT6)
    else
      echo "WARNING: no Geant4_qt_FOUND in ${GEANT4_CONFIG_FILE}; Qt is left to OpenTOPAS's own detection."
      QT_DESCRIPTION="left to OpenTOPAS (Qt of Geant4 unknown)"
      # Drop any Qt setting cached in a reused build directory, so OpenTOPAS decides afresh.
      QT_ARGS=(-UTOPAS_USE_QT -UTOPAS_USE_QT6)
    fi
    ;;
  ON)
    if [[ "${GEANT4_QT_MAJOR}" == 6 ]]; then
      QT_ARGS=(-DTOPAS_USE_QT=ON -DTOPAS_USE_QT6=ON)
      QT_DESCRIPTION="ON, Qt6 (forced)"
    else
      QT_ARGS=(-DTOPAS_USE_QT=ON -DTOPAS_USE_QT6=OFF)
      QT_DESCRIPTION="ON (forced)"
    fi
    ;;
  OFF)
    QT_ARGS=(-DTOPAS_USE_QT=OFF -DTOPAS_USE_QT6=OFF)
    QT_DESCRIPTION="OFF (forced; OpenTOPAS turns it back on if it detects Qt in Geant4)"
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
