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
#   ./scripts/dev.sh relay            python3 scripts/relay.py (runs on the HOST, needs pyserial)
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
  relay)          exec python3 "$repo_root/scripts/relay.py" "$@" ;;
  purge)
    docker image rm -f "$IMAGE" 2>/dev/null || true
    docker volume rm -f "$CCACHE_VOLUME" 2>/dev/null || true
    echo "removed image '$IMAGE' and volume '$CCACHE_VOLUME'"
    ;;
  help|-h|--help)
    sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'
    ;;
  *)
    echo "unknown command: $cmd (try: $0 help)" >&2
    exit 2
    ;;
esac
