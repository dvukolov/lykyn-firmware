#!/usr/bin/env bash
# Build the on-device dashboard and embed it into the firmware component.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/webapp"
if [[ ! -d node_modules ]]; then
  npm ci
fi
npm test
npm run build
cd "$ROOT"
python3 scripts/embed_static_files.py   webapp/out   external_components/lykyn_dashboard/static_files.h   external_components/lykyn_dashboard/static_files.cpp
