#!/usr/bin/env bash
# Run the ESP-IDF toolchain in a container. Nothing is installed on the host.
#
#   ./scripts/dev.sh bootstrap        fetch pinned submodules (host git only)
#   ./scripts/dev.sh image            build the toolchain image from ./Dockerfile
#   ./scripts/dev.sh build            idf.py build
#   ./scripts/dev.sh flash            idf.py -p $PORT flash
#   ./scripts/dev.sh monitor          idf.py -p $PORT monitor      (Ctrl-] to exit)
#   ./scripts/dev.sh flash-monitor    idf.py -p $PORT flash monitor
#   ./scripts/dev.sh menuconfig       idf.py menuconfig
#   ./scripts/dev.sh fullclean        idf.py fullclean
#   ./scripts/dev.sh idf <args...>    any idf.py invocation
#   ./scripts/dev.sh shell            interactive bash with idf.py on PATH
#   ./scripts/dev.sh relay            Feather CDC -> S3 UART (Docker; pass-through ttyUSB/ttyACM)
#   ./scripts/dev.sh feather-image    build the Pico SDK image (Dockerfile.feather)
#   ./scripts/dev.sh feather-build    UF2 -> build-feather/mouse_relay.uf2  (BOOTSEL copy)
#   ./scripts/dev.sh purge            remove the image and the ccache volume
#
# Environment overrides:
#   PORT=/dev/ttyUSB0   serial port of the board's Micro-USB UART bridge
#   IMAGE=mouse2nxgyro-dev
#
# Removal: `./scripts/dev.sh purge` + deleting this directory leaves the host as
# it was. Container state is limited to the image, one named ccache volume, and
# the bind-mounted repo (build/ is created with your uid/gid, not root's).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${IMAGE:-mouse2nxgyro-dev}"
FEATHER_IMAGE="${FEATHER_IMAGE:-mouse2nxgyro-feather}"
RELAY_IMAGE="${RELAY_IMAGE:-mouse2nxgyro-relay}"
PORT="${PORT:-/dev/ttyUSB0}"
CCACHE_VOLUME="${IMAGE}-ccache"

cmd="${1:-help}"
shift || true

ensure_image() {
  if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo ">> toolchain image '$IMAGE' not found; building it (one-time, ~5 GB pull)"
    build_image
  fi
}

build_image() {
  docker build \
    --build-arg DEV_UID="$(id -u)" \
    --build-arg DEV_GID="$(id -g)" \
    -t "$IMAGE" "$repo_root"
}

# Assemble `docker run` arguments. The serial port is only passed through when it
# exists, so `build` works with no board attached.
run_args() {
  local -a args=(
    --rm
    --user "$(id -u):$(id -g)"
    -e HOME=/tmp
    -e TERM="${TERM:-xterm-256color}"
    -v "$repo_root:/project"
    -v "$CCACHE_VOLUME:/ccache"
    -w /project
  )
  if [[ -t 0 && -t 1 ]]; then
    args+=(-it)
  fi
  if [[ -e "$PORT" ]]; then
    # Pass the device and join its owning group (dialout on Debian/Ubuntu, uucp
    # on Arch/CachyOS) so the unprivileged container user can open it.
    args+=(--device "$PORT:$PORT" --group-add "$(stat -c %g "$PORT")")
  fi
  printf '%s\n' "${args[@]}"
}

build_feather_image() {
  docker build -f "$repo_root/Dockerfile.feather" -t "$FEATHER_IMAGE" "$repo_root"
}

ensure_feather_image() {
  if ! docker image inspect "$FEATHER_IMAGE" >/dev/null 2>&1; then
    echo ">> Feather toolchain image '$FEATHER_IMAGE' not found; building it"
    build_feather_image
  fi
}

build_relay_image() {
  docker build -f "$repo_root/Dockerfile.relay" -t "$RELAY_IMAGE" "$repo_root"
}

ensure_relay_image() {
  if ! docker image inspect "$RELAY_IMAGE" >/dev/null 2>&1; then
    echo ">> relay image '$RELAY_IMAGE' not found; building it"
    build_relay_image
  fi
}

# Pass every USB-serial node into the container and join its owning group
# (dialout / uucp) so the unprivileged user can open it. Host /sys is already
# visible, which is how the script reads VID/PID for autodetection.
serial_device_args() {
  local d g
  local -A seen_g=()
  local -a extra=()
  shopt -s nullglob
  for d in /dev/ttyUSB* /dev/ttyACM*; do
    extra+=(--device "$d:$d")
    g="$(stat -c %g "$d")"
    if [[ -z ${seen_g[$g]+x} ]]; then
      extra+=(--group-add "$g")
      seen_g[$g]=1
    fi
  done
  if ((${#extra[@]} == 0)); then
    echo "!! no /dev/ttyUSB* or /dev/ttyACM* — plug the S3 Micro-USB and Feather USB-C first." >&2
    exit 1
  fi
  printf '%s\n' "${extra[@]}"
}

run_relay() {
  ensure_relay_image
  local -a args=(
    --rm
    --network none
    --user "$(id -u):$(id -g)"
    -e HOME=/tmp
    -e TERM="${TERM:-xterm-256color}"
    -v "$repo_root:/project:ro"
    -w /project
  )
  if [[ -t 0 && -t 1 ]]; then
    args+=(-it)
  fi
  local -a devices
  mapfile -t devices < <(serial_device_args)
  docker run "${args[@]}" "${devices[@]}" "$RELAY_IMAGE" python3 /project/scripts/relay.py "$@"
}

feather_build() {
  ensure_feather_image
  mkdir -p "$repo_root/build-feather"
  docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -v "$repo_root:/project" \
    -w /project/feather/pico \
    "$FEATHER_IMAGE" \
    bash -lc 'cmake -S /project/feather/pico -B /project/build-feather -G Ninja -DPICO_BOARD=adafruit_feather_rp2040_usb_host && cmake --build /project/build-feather'
  echo ">> UF2: $repo_root/build-feather/mouse_relay.uf2"
  echo "   Hold BOOT on the Feather, plug USB-C, copy that UF2 onto the mass-storage drive."
}

run_in_container() {
  ensure_image
  local -a args
  mapfile -t args < <(run_args)
  docker run "${args[@]}" "$IMAGE" "$@"
}

need_port() {
  if [[ ! -e "$PORT" ]]; then
    echo "!! serial port $PORT not found. Plug the board's Micro-USB (UART) port in," >&2
    echo "   or set PORT=/dev/ttyUSBn (check: ls /dev/ttyUSB* /dev/ttyACM*)." >&2
    exit 1
  fi
}

case "$cmd" in
  bootstrap)      exec "$repo_root/scripts/bootstrap.sh" ;;
  image)          build_image ;;
  build)          run_in_container idf.py build "$@" ;;
  flash)          need_port; run_in_container idf.py -p "$PORT" flash "$@" ;;
  monitor)        need_port; run_in_container idf.py -p "$PORT" monitor "$@" ;;
  flash-monitor)  need_port; run_in_container idf.py -p "$PORT" flash monitor "$@" ;;
  menuconfig)     run_in_container idf.py menuconfig ;;
  fullclean)      run_in_container idf.py fullclean ;;
  idf)            run_in_container idf.py "$@" ;;
  shell)          run_in_container /bin/bash ;;
  relay)          run_relay "$@" ;;
  feather-image)  build_feather_image ;;
  feather-build)  feather_build ;;
  purge)
    docker image rm -f "$IMAGE" 2>/dev/null || true
    docker image rm -f "$FEATHER_IMAGE" 2>/dev/null || true
    docker image rm -f "$RELAY_IMAGE" 2>/dev/null || true
    docker volume rm -f "$CCACHE_VOLUME" 2>/dev/null || true
    echo "removed images and volume '$CCACHE_VOLUME'"
    ;;
  help|-h|--help)
    sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'
    ;;
  *)
    echo "unknown command: $cmd (try: $0 help)" >&2
    exit 2
    ;;
esac
