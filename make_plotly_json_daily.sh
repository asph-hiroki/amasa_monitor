#!/bin/bash

# ==============================
# AMASA scaler monitor
# Plotly JSON generation script
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
  echo "Start make_plotly_json_daily.sh"
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "Host: $(hostname)"
  echo "Config file: ${CONFIG_FILE}"
  echo "Base dir: ${BASE_DIR}"
  echo "Data root: ${DATA_ROOT}"
  echo "Figure dir: ${FIGURE_DIR}"
  echo "Latest Plotly JSON: ${LATEST_PLOTLY_JSON}"
  echo "ROOT command: ${ROOT_CMD}"

  cd "${BASE_DIR}" || {
    echo "ERROR: failed to cd ${BASE_DIR}"
    exit 1
  }

  if command -v flock >/dev/null 2>&1; then
    exec 9>"${MAKE_PLOTLY_LOCK_FILE}"
    if ! flock -n 9; then
      echo "Another make_plotly_json_daily.sh is already running. Exit."
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
  "${ROOT_CMD}" -l -b -q "make_plotly_json.C(\"${DATA_ROOT}\", \"${FIGURE_DIR}\", ${PLOTLY_TARGET_TOTAL_POINTS}, ${PLOTLY_MIN_POINTS_PER_TRACE}, ${PLOTLY_MAX_POINTS_PER_TRACE}, ${PLOTLY_HARD_LIMIT_POINTS}, ${PLOTLY_DATASET_LAST1D_DURATION_SEC}, ${PLOTLY_DATASET_LAST1D_BASE_BIN_SEC}, ${PLOTLY_DATASET_LAST7D_DURATION_SEC}, ${PLOTLY_DATASET_LAST7D_BASE_BIN_SEC}, ${PLOTLY_DATASET_LAST1M_DURATION_SEC}, ${PLOTLY_DATASET_LAST1M_BASE_BIN_SEC})"

  ROOT_STATUS=$?

  echo "ROOT status: ${ROOT_STATUS}"

  if [ ${ROOT_STATUS} -ne 0 ]; then
    echo "ERROR: ROOT macro failed with status ${ROOT_STATUS}"
    exit ${ROOT_STATUS}
  fi

  echo "Check latest_plotly.json..."
  if [ ! -f "${LATEST_PLOTLY_JSON}" ]; then
    echo "ERROR: latest_plotly.json was not created: ${LATEST_PLOTLY_JSON}"
    exit 1
  fi

  DATE_DIR=$(grep '"date"' "${LATEST_PLOTLY_JSON}" | head -1 | sed 's/.*"date"[ ]*:[ ]*"\([^"]*\)".*/\1/')

  if [ -z "${DATE_DIR}" ]; then
    echo "ERROR: failed to extract date from latest_plotly.json"
    exit 1
  fi

  echo "Date dir: ${DATE_DIR}"

  JSON_DIR="${FIGURE_DIR}/${DATE_DIR}/${PLOTLY_JSON_DIR_NAME}"

  echo "Check Plotly JSON files..."
  if [ ! -d "${JSON_DIR}" ]; then
    echo "ERROR: JSON dir not found: ${JSON_DIR}"
    exit 1
  fi

  if [ ! -f "${JSON_DIR}/scaler_10sec_last1d.json" ]; then
    echo "ERROR: Plotly last1d JSON not found: ${JSON_DIR}/scaler_10sec_last1d.json"
    exit 1
  fi

  if [ ! -f "${JSON_DIR}/scaler_1min_last7d.json" ]; then
    echo "ERROR: Plotly last7d JSON not found: ${JSON_DIR}/scaler_1min_last7d.json"
    exit 1
  fi

  if [ ! -f "${JSON_DIR}/scaler_5min_last1m.json" ]; then
    echo "ERROR: Plotly last1m JSON not found: ${JSON_DIR}/scaler_5min_last1m.json"
    exit 1
  fi

  echo "Recent Plotly output files:"
  find "${FIGURE_DIR}" -maxdepth 4 -type f \( -name "latest_plotly.json" -o -name "*.json" \) -printf "%TY-%Tm-%Td %TH:%TM %p\n" | sort | tail -30

  echo "End make_plotly_json_daily.sh"
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "Status: OK"

} >> "${MAKE_PLOTLY_LOG_FILE}" 2>&1