#!/bin/bash

# ==============================
# AMASA scaler monitor
# Publish-only script
# Run this on icrapp1
# ==============================

CONFIG_FILE="/home/asuke/amasa_monitor/config/amasa_monitor.conf"

if [ ! -f "${CONFIG_FILE}" ]; then
  echo "ERROR: config file not found: ${CONFIG_FILE}"
  exit 1
fi

. "${CONFIG_FILE}"

mkdir -p "${LOG_DIR}"

{
  echo "========================================"
  echo "Start publish_daily.sh"
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "Host: $(hostname)"
  echo "Config file: ${CONFIG_FILE}"
  echo "Base dir: ${BASE_DIR}"
  echo "Figure dir: ${FIGURE_DIR}"
  echo "Latest json: ${LATEST_JSON}"
  echo "Latest Plotly json: ${LATEST_PLOTLY_JSON}"
  echo "Public base: ${PUBLIC_BASE}"
  echo "Public figure dir: ${PUBLIC_FIGURE_DIR}"
  echo "Public latest json: ${PUBLIC_LATEST_JSON}"

  cd "${BASE_DIR}" || {
    echo "ERROR: failed to cd ${BASE_DIR}"
    exit 1
  }

  if command -v flock >/dev/null 2>&1; then
    exec 9>"${PUBLISH_LOCK_FILE}"
    if ! flock -n 9; then
      echo "Another publish_daily.sh is already running. Exit."
      exit 0
    fi
  else
    echo "WARNING: flock not found. Continue without lock."
  fi

  echo "Check latest.json..."
  if [ ! -f "${LATEST_JSON}" ]; then
    echo "ERROR: latest.json not found: ${LATEST_JSON}"
    echo "Run make_root_daily.sh on icrhome04 first."
    exit 1
  fi

  echo "Check latest_plotly.json..."
  if [ ! -f "${LATEST_PLOTLY_JSON}" ]; then
    echo "ERROR: latest_plotly.json not found: ${LATEST_PLOTLY_JSON}"
    echo "Run make_plotly_json_daily.sh on icrhome04 first."
    exit 1
  fi

  echo "Read date from latest.json..."
  DATE_DIR=$(grep '"date"' "${LATEST_JSON}" | head -1 | sed 's/.*"date"[ ]*:[ ]*"\([^"]*\)".*/\1/')

  if [ -z "${DATE_DIR}" ]; then
    echo "ERROR: failed to extract date from latest.json"
    exit 1
  fi

  echo "Date dir from latest.json: ${DATE_DIR}"

  echo "Read date from latest_plotly.json..."
  PLOTLY_DATE_DIR=$(grep '"date"' "${LATEST_PLOTLY_JSON}" | head -1 | sed 's/.*"date"[ ]*:[ ]*"\([^"]*\)".*/\1/')

  if [ -z "${PLOTLY_DATE_DIR}" ]; then
    echo "ERROR: failed to extract date from latest_plotly.json"
    exit 1
  fi

  echo "Date dir from latest_plotly.json: ${PLOTLY_DATE_DIR}"

  if [ "${DATE_DIR}" != "${PLOTLY_DATE_DIR}" ]; then
    echo "WARNING: latest.json date and latest_plotly.json date differ."
    echo "latest.json date:        ${DATE_DIR}"
    echo "latest_plotly.json date: ${PLOTLY_DATE_DIR}"
    echo "Continue publishing both, but check generation timing."
  fi

  SRC_DATE_DIR="${FIGURE_DIR}/${DATE_DIR}"
  SRC_PLOTLY_DATE_DIR="${FIGURE_DIR}/${PLOTLY_DATE_DIR}"

  PUBLIC_DATE_DIR="${PUBLIC_FIGURE_DIR}/${DATE_DIR}"
  PUBLIC_PLOTLY_DATE_DIR="${PUBLIC_FIGURE_DIR}/${PLOTLY_DATE_DIR}"

  echo "Check source ROOT files..."
  if [ ! -d "${SRC_DATE_DIR}" ]; then
    echo "ERROR: source date dir not found: ${SRC_DATE_DIR}"
    exit 1
  fi

  if [ ! -f "${SRC_DATE_DIR}/scaler_1min_mean_last1d.root" ]; then
    echo "ERROR: last1d ROOT file not found: ${SRC_DATE_DIR}/scaler_1min_mean_last1d.root"
    exit 1
  fi

  if [ ! -f "${SRC_DATE_DIR}/scaler_1min_mean_last1w.root" ]; then
    echo "ERROR: last1w ROOT file not found: ${SRC_DATE_DIR}/scaler_1min_mean_last1w.root"
    exit 1
  fi

  echo "Check source Plotly JSON files..."
  PLOTLY_JSON_DIR="${SRC_PLOTLY_DATE_DIR}/${PLOTLY_JSON_DIR_NAME}"

  if [ ! -d "${PLOTLY_JSON_DIR}" ]; then
    echo "ERROR: Plotly JSON dir not found: ${PLOTLY_JSON_DIR}"
    exit 1
  fi

  if [ ! -f "${PLOTLY_JSON_DIR}/scaler_10sec_last1d.json" ]; then
    echo "ERROR: Plotly last1d JSON not found: ${PLOTLY_JSON_DIR}/scaler_10sec_last1d.json"
    exit 1
  fi

  if [ ! -f "${PLOTLY_JSON_DIR}/scaler_1min_last7d.json" ]; then
    echo "ERROR: Plotly last7d JSON not found: ${PLOTLY_JSON_DIR}/scaler_1min_last7d.json"
    exit 1
  fi

  if [ ! -f "${PLOTLY_JSON_DIR}/scaler_5min_last1m.json" ]; then
    echo "ERROR: Plotly last1m JSON not found: ${PLOTLY_JSON_DIR}/scaler_5min_last1m.json"
    exit 1
  fi

  echo "Create public directories..."
  mkdir -p "${PUBLIC_BASE}"
  mkdir -p "${PUBLIC_FIGURE_DIR}"
  mkdir -p "${PUBLIC_DATE_DIR}"
  mkdir -p "${PUBLIC_PLOTLY_DATE_DIR}"

  echo "Sync ROOT date directory..."
  rsync -av --delay-updates \
    "${SRC_DATE_DIR}/" \
    "${PUBLIC_DATE_DIR}/"

  RSYNC_ROOT_STATUS=$?

  if [ ${RSYNC_ROOT_STATUS} -ne 0 ]; then
    echo "ERROR: rsync ROOT date dir failed with status ${RSYNC_ROOT_STATUS}"
    exit ${RSYNC_ROOT_STATUS}
  fi

  if [ "${PLOTLY_DATE_DIR}" != "${DATE_DIR}" ]; then
    echo "Sync Plotly date directory separately..."
    rsync -av --delay-updates \
      "${SRC_PLOTLY_DATE_DIR}/" \
      "${PUBLIC_PLOTLY_DATE_DIR}/"

    RSYNC_PLOTLY_STATUS=$?

    if [ ${RSYNC_PLOTLY_STATUS} -ne 0 ]; then
      echo "ERROR: rsync Plotly date dir failed with status ${RSYNC_PLOTLY_STATUS}"
      exit ${RSYNC_PLOTLY_STATUS}
    fi
  fi

  echo "Copy latest.json last..."
  cp "${LATEST_JSON}" "${PUBLIC_LATEST_JSON}"

  CP_LATEST_STATUS=$?

  if [ ${CP_LATEST_STATUS} -ne 0 ]; then
    echo "ERROR: failed to copy latest.json"
    exit ${CP_LATEST_STATUS}
  fi

  echo "Copy latest_plotly.json last..."
  cp "${LATEST_PLOTLY_JSON}" "${PUBLIC_BASE}/latest_plotly.json"

  CP_PLOTLY_STATUS=$?

  if [ ${CP_PLOTLY_STATUS} -ne 0 ]; then
    echo "ERROR: failed to copy latest_plotly.json"
    exit ${CP_PLOTLY_STATUS}
  fi

  echo "Public output check:"
  ls -lh "${PUBLIC_LATEST_JSON}"
  ls -lh "${PUBLIC_BASE}/latest_plotly.json"
  ls -lh "${PUBLIC_DATE_DIR}/"

  if [ -d "${PUBLIC_PLOTLY_DATE_DIR}/${PLOTLY_JSON_DIR_NAME}" ]; then
    ls -lh "${PUBLIC_PLOTLY_DATE_DIR}/${PLOTLY_JSON_DIR_NAME}/"
  fi

  echo "End publish_daily.sh"
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "Status: OK"

} >> "${PUBLISH_LOG_FILE}" 2>&1