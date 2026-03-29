#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}/.."
OUT_DIR="${PROJECT_DIR}/third_party/crun/src"

VERSION="${1:-1.26}"
URL="https://github.com/containers/crun/releases/download/${VERSION}/crun-${VERSION}.tar.gz"

mkdir -p "${OUT_DIR}"
echo "Downloading ${URL}"
curl -fL --retry 3 --retry-delay 2 -o "${OUT_DIR}/crun-${VERSION}.tar.gz" "${URL}"
echo "Saved: ${OUT_DIR}/crun-${VERSION}.tar.gz"
