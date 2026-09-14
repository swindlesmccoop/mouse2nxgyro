#!/usr/bin/env bash
# Fetch the pinned espp submodule and ONLY the nested submodules this project
# actually builds against. espp carries many more (lvgl, esp-dsp, ...) that we
# never compile, so a full recursive init would pull hundreds of MB for nothing.
#
# Safe to re-run. Runs on the host or inside the dev container; needs only git.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo ">> espp submodule"
git submodule update --init external/espp

echo ">> espp nested submodules (fmt, hid-rp, tinyusb, esp-usb)"
git -C external/espp submodule update --init \
  components/format/detail/fmt \
  components/hid-rp/detail/hid-rp \
  external/tinyusb \
  external/esp-usb

echo ">> pinned versions"
echo "   espp:        $(git -C external/espp rev-parse --short HEAD)"
echo "   tinyusb:     $(git -C external/espp/external/tinyusb rev-parse --short HEAD)"
echo "   esp-usb:     $(git -C external/espp/external/esp-usb rev-parse --short HEAD)"
echo "   hid-rp:      $(git -C external/espp/components/hid-rp/detail/hid-rp rev-parse --short HEAD)"
echo "   fmt:         $(git -C external/espp/components/format/detail/fmt rev-parse --short HEAD)"
echo "done."
