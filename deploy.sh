#!/bin/bash
# TizenClaw Build Script
# Automates: gbs build
#
# Usage:
#   ./deploy.sh                    # Full build
#   ./deploy.sh -n                 # Use --noinit for faster rebuild
#   ./deploy.sh --dry-run          # Print commands without executing
#   ./deploy.sh --test             # Build + run E2E smoke tests
#
# See ./scripts/deploy.sh --help for all options.

set -euo pipefail

# ─────────────────────────────────────────────
# Constants
# ─────────────────────────────────────────────
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_NAME="tizenclaw"
GBS_BUILD_LOG="/tmp/gbs_build_output.log"
RPM_OUT_DIR="${PROJECT_DIR}/rpms"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ─────────────────────────────────────────────
# Defaults
# ─────────────────────────────────────────────
ARCH="armv7l"
ARCH_EXPLICIT=false
NOINIT=false
INCREMENTAL=false
DRY_RUN=false
WITH_CRUN=false
RUN_TESTS=false
RUN_FULL_TESTS=false
WITH_ASSETS=false
RAG_PROJECT_DIR=""
BUILD_JOBS=""   # Empty = let GBS decide; set via -j to cap parallel jobs

# ─────────────────────────────────────────────
# Logging helpers
# ─────────────────────────────────────────────
log()    { echo -e "${CYAN}[BUILD]${NC} $*"; }
ok()     { echo -e "${GREEN}[  OK  ]${NC} $*"; }
warn()   { echo -e "${YELLOW}[ WARN ]${NC} $*"; }
fail()   { echo -e "${RED}[ FAIL ]${NC} $*"; exit 1; }
header() { echo -e "\n${BOLD}══════════════════════════════════════════${NC}"; echo -e "${BOLD}  $*${NC}"; echo -e "${BOLD}══════════════════════════════════════════${NC}"; }

# ─────────────────────────────────────────────
# Dry-run wrapper
# ─────────────────────────────────────────────
run() {
  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} $*"
    return 0
  fi
  "$@"
}

# ─────────────────────────────────────────────
# Usage
# ─────────────────────────────────────────────
usage() {
  cat <<EOF
${BOLD}TizenClaw Build${NC}

${CYAN}Usage:${NC}
  $(basename "$0") [options]

${CYAN}Options:${NC}
  -a, --arch <arch>     Build architecture (default: armv7l)
  -j, --jobs <n>        Max parallel compile jobs (default: auto; use 2 on low-RAM systems)
  -n, --noinit          Skip build-env init (faster rebuild)
  -i, --incremental     Use --incremental and --skip-srcrpm for fast iterative build
  -t, --test            Run E2E smoke tests after build
  -T, --full-test       Run all automated test suites after build
      --with-assets     Also build tizenclaw-assets
      --with-crun       Build crun and enable container execution mode
      --dry-run         Print commands without executing
  -h, --help            Show this help

${CYAN}Examples:${NC}
  $(basename "$0")                     # Full build
  $(basename "$0") -n                  # Quick rebuild
  $(basename "$0") -i -n               # Fastest iterative rebuild
  $(basename "$0") -t                  # Build + run E2E tests
  $(basename "$0") -T                  # Build + run full verifications
  $(basename "$0") --with-assets       # Build including tizenclaw-assets
  $(basename "$0") --dry-run           # Preview all steps
  $(basename "$0") -j 2                # Build with 2 parallel jobs (low RAM systems)
  $(basename "$0") -a aarch64          # Build for ARM64 target
EOF
  exit 0
}

# ─────────────────────────────────────────────
# Argument parsing
# ─────────────────────────────────────────────
parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -a|--arch)          ARCH="$2"; ARCH_EXPLICIT=true; shift 2 ;;
      -j|--jobs)          BUILD_JOBS="$2"; shift 2 ;;
      -n|--noinit)        NOINIT=true; shift ;;
      -i|--incremental)   INCREMENTAL=true; shift ;;
      -t|--test)          RUN_TESTS=true; shift ;;
      -T|--full-test)     RUN_FULL_TESTS=true; shift ;;
      --with-assets)      WITH_ASSETS=true; shift ;;
      --with-crun)        WITH_CRUN=true; shift ;;
      --dry-run)          DRY_RUN=true; shift ;;
      -h|--help)          usage ;;
      *)                  fail "Unknown option: $1 (use --help)" ;;
    esac
  done
}

# ─────────────────────────────────────────────
# Step 0: Pre-flight checks
# ─────────────────────────────────────────────
check_prerequisites() {
  header "Pre-flight Checks"

  if ! command -v gbs &>/dev/null; then
    if [ "${DRY_RUN}" = true ]; then
      warn "gbs not found (ignored in dry-run)"
    else
      fail "gbs not found. Install Tizen GBS first."
    fi
  else
    ok "gbs found"
  fi

  log "Architecture : ${ARCH}"
  log "Project dir  : ${PROJECT_DIR}"
  log "Incremental  : ${INCREMENTAL}"
  log "No-init      : ${NOINIT}"
  log "Dry-run      : ${DRY_RUN}"
  if [ -n "${BUILD_JOBS}" ]; then
    log "Parallel jobs: ${BUILD_JOBS} (capped to avoid OOM)"
  else
    log "Parallel jobs: auto"
  fi
}

# ─────────────────────────────────────────────
# Step 1: GBS Build
# ─────────────────────────────────────────────
RPMS_DIR=""

do_build() {
  header "Step 1/2: GBS Build"

  local gbs_args=("-A" "${ARCH}" "--include-all")

  if [ "${INCREMENTAL}" = true ]; then
    gbs_args+=("--incremental" "--skip-srcrpm")
    log "Using --incremental & --skip-srcrpm (fast iterative build)"
  fi

  if [ "${NOINIT}" = true ]; then
    gbs_args+=("--noinit")
    log "Using --noinit (skipping build-env initialization)"
  fi

  if [ "${WITH_CRUN}" = true ]; then
    gbs_args+=("--define" "with_crun 1")
    log "Building WITH crun support (container mode)"
  else
    log "Building WITHOUT crun (default native debug mode)"
  fi

  if [ -n "${BUILD_JOBS}" ]; then
    gbs_args+=("--define" "jobs ${BUILD_JOBS}")
    log "Limiting parallel jobs to: ${BUILD_JOBS}"
  fi

  log "Running: gbs build ${gbs_args[*]}"
  cd "${PROJECT_DIR}"

  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} gbs build ${gbs_args[*]}"
    ok "GBS build succeeded"
    return 0
  fi

  # Run gbs build and capture output for RPMS path extraction
  if gbs build "${gbs_args[@]}" 2>&1 | tee "${GBS_BUILD_LOG}"; then
    ok "GBS build succeeded"
  else
    fail "GBS build failed. Check the build log: ${GBS_BUILD_LOG}"
  fi

  # Extract RPMS directory from gbs build output
  RPMS_DIR=$(grep -A1 'generated RPM packages can be found from local repo:' "${GBS_BUILD_LOG}" \
    | tail -1 | sed 's/^[[:space:]]*//')

  if [ -n "${RPMS_DIR}" ]; then
    ok "RPMS directory: ${RPMS_DIR}"
  else
    warn "Could not parse RPMS path from build output"
  fi
}

# ─────────────────────────────────────────────
# Step 1.5: Build tizenclaw-assets (if present)
# ─────────────────────────────────────────────
RAG_RPM_FILES=()

do_build_rag() {
  if [ "${WITH_ASSETS}" = false ]; then
    return 0
  fi

  # Auto-detect tizenclaw-assets project
  if [ -z "${RAG_PROJECT_DIR}" ]; then
    RAG_PROJECT_DIR="${PROJECT_DIR}/../tizenclaw-assets"
  fi

  if [ ! -f "${RAG_PROJECT_DIR}/CMakeLists.txt" ]; then
    log "tizenclaw-assets project not found at ${RAG_PROJECT_DIR} (skipping)"
    return 0
  fi

  header "Step 1.5: Build tizenclaw-assets"

  local rag_abs_dir
  rag_abs_dir=$(cd "${RAG_PROJECT_DIR}" && pwd)
  log "RAG project: ${rag_abs_dir}"

  local gbs_args=("-A" "${ARCH}" "--include-all")
  if [ "${NOINIT}" = true ]; then
    gbs_args+=("--noinit")
  fi

  log "Running: gbs build ${gbs_args[*]} (tizenclaw-assets)"

  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} cd ${rag_abs_dir} && gbs build ${gbs_args[*]}"
    ok "tizenclaw-assets build (dry-run)"
    return 0
  fi

  local rag_log="/tmp/gbs_assets_build_output.log"
  if (cd "${rag_abs_dir}" && gbs build "${gbs_args[@]}" 2>&1 | tee "${rag_log}"); then
    ok "tizenclaw-assets build succeeded"
  else
    warn "tizenclaw-assets build failed (non-fatal, continuing without RAG)"
    return 0
  fi

  # Find RAG RPMs
  local rag_rpms_dir
  rag_rpms_dir=$(grep -A1 'generated RPM packages can be found from local repo:' "${rag_log}" \
    | tail -1 | sed 's/^[[:space:]]*//')

  if [ -n "${rag_rpms_dir}" ] && [ -d "${rag_rpms_dir}" ]; then
    mapfile -t RAG_RPM_FILES < <(find "${rag_rpms_dir}" -maxdepth 1 \
      -name "tizenclaw-assets*.rpm" \
      ! -name "*-debuginfo-*" \
      ! -name "*-debugsource-*" \
      2>/dev/null | sort)

    for rpm in "${RAG_RPM_FILES[@]}"; do
      ok "RAG RPM: $(basename "${rpm}")"
    done
  fi
}

# ─────────────────────────────────────────────
# Step 2: Locate built RPMs and show summary
# ─────────────────────────────────────────────
RPM_FILES=()

find_rpm() {
  header "Step 2/3: Locating Built RPMs"

  # If RPMS_DIR was not set by do_build (e.g. --dry-run),
  # try to find it from the last build log or fall back to searching GBS-ROOT
  if [ -z "${RPMS_DIR}" ]; then
    if [ -f "${GBS_BUILD_LOG}" ]; then
      RPMS_DIR=$(grep -A1 'generated RPM packages can be found from local repo:' "${GBS_BUILD_LOG}" \
        | tail -1 | sed 's/^[[:space:]]*//')
    fi

    if [ -z "${RPMS_DIR}" ]; then
      local gbs_root="${HOME}/GBS-ROOT"
      RPMS_DIR=$(find "${gbs_root}" -type d -path "*/${ARCH}/RPMS" 2>/dev/null | head -1 || true)
    fi
  fi

  if [ "${DRY_RUN}" = true ]; then
    if [ -z "${RPMS_DIR}" ]; then
      RPMS_DIR="${HOME}/GBS-ROOT/local/repos/tizen/${ARCH}/RPMS"
    fi
    RPM_FILES=("${RPMS_DIR}/${PKG_NAME}-1.0.0-1.${ARCH}.rpm")
    log "[DRY-RUN] Assuming RPMs: ${RPM_FILES[*]}"
    return 0
  fi

  if [ -z "${RPMS_DIR}" ] || [ ! -d "${RPMS_DIR}" ]; then
    fail "RPMS directory not found: ${RPMS_DIR:-unknown}\n       Have you run a GBS build first?"
  fi

  log "Searching in: ${RPMS_DIR}"

  mapfile -t RPM_FILES < <(find "${RPMS_DIR}" -maxdepth 1 \
    -name "${PKG_NAME}*.rpm" \
    ! -name "*-devel-*" \
    ! -name "*-unittests-*" \
    ! -name "*-debuginfo-*" \
    ! -name "*-debugsource-*" \
    2>/dev/null | sort)

  if [ ${#RPM_FILES[@]} -eq 0 ]; then
    fail "No ${PKG_NAME} RPMs found in ${RPMS_DIR}/\n       Run a build first."
  fi

  for rpm in "${RPM_FILES[@]}"; do
    local rpm_size
    rpm_size=$(du -h "${rpm}" | cut -f1)
    ok "Found: $(basename "${rpm}") (${rpm_size})"
  done
}

# ─────────────────────────────────────────────
# Step 3: Collect RPMs into project/rpms/
# ─────────────────────────────────────────────
collect_rpms() {
  header "Step 3/3: Collecting RPMs → ${RPM_OUT_DIR}"

  # Ensure output directory exists
  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} mkdir -p ${RPM_OUT_DIR}"
  else
    mkdir -p "${RPM_OUT_DIR}"
  fi

  local all_rpms=("${RPM_FILES[@]}" "${RAG_RPM_FILES[@]}")

  if [ ${#all_rpms[@]} -eq 0 ]; then
    warn "No RPMs to collect."
    return 0
  fi

  for rpm in "${all_rpms[@]}"; do
    local rpm_basename
    rpm_basename=$(basename "${rpm}")
    if [ "${DRY_RUN}" = true ]; then
      echo -e "  ${YELLOW}[DRY-RUN]${NC} cp ${rpm} → ${RPM_OUT_DIR}/${rpm_basename}"
    else
      cp -f "${rpm}" "${RPM_OUT_DIR}/${rpm_basename}"
      ok "Collected: ${rpm_basename}"
    fi
  done

  if [ "${DRY_RUN}" = false ]; then
    echo ""
    log "All RPMs collected in: ${RPM_OUT_DIR}"
    ls -lh "${RPM_OUT_DIR}"/*.rpm 2>/dev/null || true
  fi
}

# ─────────────────────────────────────────────
# Step 3 (optional): E2E Smoke Test
# ─────────────────────────────────────────────
do_e2e_tests() {
  if [ "${RUN_TESTS}" = false ]; then
    return 0
  fi

  header "Step 3: E2E Smoke Test"

  local test_script="${PROJECT_DIR}/tests/e2e/test_smoke.sh"
  if [ ! -f "${test_script}" ]; then
    warn "E2E test script not found: ${test_script}"
    return 1
  fi

  log "Running: ${test_script}"

  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} bash ${test_script}"
    return 0
  fi

  if bash "${test_script}"; then
    ok "E2E smoke tests passed!"
  else
    fail "E2E smoke tests failed. See output above."
  fi
}

# ─────────────────────────────────────────────
# Step 4 (optional): Full Verification Test
# ─────────────────────────────────────────────
do_full_tests() {
  if [ "${RUN_FULL_TESTS}" = false ]; then
    return 0
  fi

  header "Step 4: Full Verification Test Suite"

  local test_script="${PROJECT_DIR}/tests/verification/run_all.sh"
  if [ ! -f "${test_script}" ]; then
    warn "Full test script not found: ${test_script}"
    return 1
  fi

  log "Running: ${test_script}"

  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} bash ${test_script}"
    return 0
  fi

  if bash "${test_script}"; then
    ok "Full verification tests passed!"
  else
    fail "Full verification tests failed. See output above."
  fi
}

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────
show_summary() {
  echo ""
  header "Build Complete!"
  ok "TizenClaw has been built successfully."
  touch "${PROJECT_DIR}/.build_success"
  echo ""
  log "GBS RPMS dir : ${RPMS_DIR}"
  log "Collected to : ${RPM_OUT_DIR}"
  echo ""
}

# ─────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────
main() {
  parse_args "$@"
  check_prerequisites
  do_build
  do_build_rag
  find_rpm
  collect_rpms
  do_e2e_tests
  do_full_tests
  show_summary
}

main "$@"
