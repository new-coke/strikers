#!/bin/sh
# Build dependencies for a Linux build, from SDL3's README-linux list because Aurora builds SDL3
# from source: tools/install-linux-deps.sh

set -e

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    clang lld cmake ninja-build python3 pkg-config \
    libasound2-dev libpulse-dev libaudio-dev libjack-dev libsndio-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
    libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
    libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
    libwayland-dev wayland-protocols libdecor-0-dev \
    libdbus-1-dev libibus-1.0-dev libudev-dev
