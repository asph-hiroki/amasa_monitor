#!/bin/bash

# ==============================
# AMASA scaler monitor
# Calculation-only script
# Run this on icrhome04
# ==============================

CONFIG_FILE="/home/asuke/amasa_monitor/config/amasa_monitor.conf"

if [ ! -f "${CONFIG_FILE}" ]; then
  echo "ERROR: config file not found: ${CONFIG_FILE}"
  exit 1
fi

. "${CONFIG_FILE}"

mkdir -p "${LOG_DIR}"
mkdir -p "${FIGURE_DIR}"

{
  echo "========================================"
  echo "Start make_root_daily.sh"
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "Host: $(hostname)"
  echo "Config file: ${CONFIG_FILE}"
  echo "Base dir: ${BASE_DIR}"
  echo "Data root: ${DATA_ROOT}"
  echo "Figure dir: ${FIGURE_DIR}"
  echo "Root command: ${ROOT_CMD}"

  cd "${BASE_DIR}" || {
    echo "ERROR: failed to cd ${BASE_DIR}"
    exit 1
  }

  if command -v flock >/dev/null 2>&1; then
    exec 9>"${MAKE_LOCK_FILE}"
    if ! flock -n 9; then
      echo "Another make_root_daily.sh is already running. Exit."
      exit 0
    fi
  else
    echo "WARNING: flock not found. Continue without lock."
  fi

  echo "Check host..."
  HOST_NAME="$(hostname)"
  if [ "${HOST_NAME}" != "icrhome04" ]; then
    echo "WARNING: this script is intended to run on icrhome04."
    echo "Current host: ${HOST_NAME}"
  fi

  echo "Check data root..."
  if [ ! -d "${DATA_ROOT}" ]; then
    echo "ERROR: data root not found: ${DATA_ROOT}"
    echo "This usually means you are running this on the app server."
    exit 1
  fi

  echo "Check ROOT command..."
  if [ ! -x "${ROOT_CMD}" ]; then
    echo "ERROR: ROOT command not found or not executable: ${ROOT_CMD}"
    exit 1
  fi

  echo "ROOT command: ${ROOT_CMD}"

  echo "Run ROOT macro..."
  "${ROOT_CMD}" -l -b -q "last_d_w.C(\"${DATA_ROOT}\", \"${FIGURE_DIR}\")"
  ROOT_STATUS=$?

  echo "ROOT status: ${ROOT_STATUS}"

  if [ ${ROOT_STATUS} -ne 0 ]; then
    echo "ERROR: ROOT macro failed with status ${ROOT_STATUS}"
    exit ${ROOT_STATUS}
  fi

  echo "Check latest.json..."
  if [ ! -f "${LATEST_JSON}" ]; then
    echo "ERROR: latest.json was not created: ${LATEST_JSON}"
    exit 1
  fi

  DATE_DIR=$(grep '"date"' "${LATEST_JSON}" | head -1 | sed 's/.*"date"[ ]*:[ ]*"\([^"]*\)".*/\1/')

  if [ -z "${DATE_DIR}" ]; then
    echo "ERROR: failed to extract date from latest.json"
    exit 1
  fi

  echo "Date dir: ${DATE_DIR}"

  if [ ! -f "${FIGURE_DIR}/${DATE_DIR}/scaler_1min_mean_last1d.root" ]; then
    echo "ERROR: last1d ROOT file was not created"
    exit 1
  fi

  if [ ! -f "${FIGURE_DIR}/${DATE_DIR}/scaler_1min_mean_last1w.root" ]; then
    echo "ERROR: last1w ROOT file was not created"
    exit 1
  fi

  echo "Recent output files:"
  find "${FIGURE_DIR}" -maxdepth 3 -type f \( -name "*.root" -o -name "latest.json" \) -printf "%TY-%Tm-%Td %TH:%TM %p\n" | sort | tail -20

  echo "End make_root_daily.sh"
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "Status: OK"

} >> "${MAKE_LOG_FILE}" 2>&1