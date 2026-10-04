#!/usr/bin/env bash
# Aigen Workbench v2.2.0 | Updated 2026-10-04 | Build and launch the workbench as pixelgenie.
# agent: gpt-6.1-sol | 2026-10-04 | Launch PixelGenie image workbench | v 2.2.0 | c2ac47
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
"$ROOT/build.sh"
cd "$ROOT"
exec "$ROOT/pixelgenie" "$@"
# agent: gpt-6.1-sol | 2026-10-04 | Launch PixelGenie image workbench | v 2.2.0 | c2ac47
