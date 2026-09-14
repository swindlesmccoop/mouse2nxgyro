# Build/flash toolchain for mouse2nxgyro.
#
# Everything (ESP-IDF v6.0.1, the xtensa-esp32s3 toolchain, python env, esptool,
# ccache) lives in Espressif's official image. Nothing is installed on the host.
#
# Pinned by tag AND manifest-list digest so a fresh clone builds against the same
# toolchain a year from now. The digest is the multi-arch manifest list for
# espressif/idf:v6.0.1 (amd64 + arm64), taken from Docker Hub on 2026-09-14.
# To bump: change the tag, then update the digest from
#   https://hub.docker.com/v2/repositories/espressif/idf/tags/<tag>  (field "digest")
FROM espressif/idf:v6.0.1@sha256:efc19fae2f52fc6873630c668da26aa834139a063b5fb73a46ebc0dbd217b587

# ccache is already in the image; point it at a volume so rebuilds are fast and
# the cache disappears with `docker volume rm` rather than living on the host.
ENV IDF_CCACHE_ENABLE=1 \
    CCACHE_DIR=/ccache

# The project is bind-mounted here by scripts/dev.sh / docker-compose / devcontainer.
WORKDIR /project

# The container is run as the host user (see scripts/dev.sh) so build/ stays
# user-owned. ESP-IDF's git version probing then complains about the root-owned
# /opt/esp/idf checkout ("dubious ownership"); a system-wide safe.directory fixes
# that without needing a writable $HOME.
RUN git config --system --add safe.directory '*'

# Let any uid write the ccache dir if the volume is created fresh by the daemon.
RUN mkdir -p /ccache && chmod 1777 /ccache

# A named non-root user for the devcontainer flow (VS Code / Cursor remap its uid
# to the host user via updateRemoteUserUID). scripts/dev.sh does not use it; it
# passes --user $(id -u):$(id -g) directly.
ARG DEV_UID=1000
ARG DEV_GID=1000
# The Ubuntu 24.04 base already has an `ubuntu` user at uid/gid 1000; evict
# whatever holds our uid so `dev` can take it.
RUN set -eu; \
    existing="$(getent passwd "${DEV_UID}" | cut -d: -f1 || true)"; \
    if [ -n "$existing" ]; then userdel -r "$existing" 2>/dev/null || userdel "$existing"; fi; \
    getent group "${DEV_GID}" >/dev/null || groupadd --gid "${DEV_GID}" dev; \
    useradd --uid "${DEV_UID}" --gid "${DEV_GID}" --create-home --shell /bin/bash dev

# The base image's entrypoint sources ESP-IDF's export.sh, so `idf.py` is on PATH
# for whatever command follows.
