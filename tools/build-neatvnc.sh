#!/usr/bin/env bash

# Update and install dependencies
sudo apt update
sudo apt install -y meson ninja-build libpixman-1-dev zlib1g-dev libdrm-dev pkg-config

TOP_DIR=$(pwd)
# Clone and build aml
if [ ! -d "aml" ]; then
    git clone https://github.com/any1/aml --depth=1 -b v0.3.0
else
    pushd aml
    git fetch --tags
    git checkout v0.3.0
    popd
fi
pushd aml
meson -Dprefix=$TOP_DIR/_static --default-library=static build
ninja -C build install
popd

export PKG_CONFIG_PATH=$TOP_DIR/_static/lib/$(uname -m)-linux-gnu/pkgconfig

# Clone and build NeatVNC
if [ ! -d "neatvnc" ]; then
    git clone https://github.com/any1/neatvnc --depth=1 -b v0.9.5
else
    pushd neatvnc
    git fetch --tags
    git checkout v0.9.5
    popd
fi
pushd neatvnc
meson -Dprefix=$TOP_DIR/_static --default-library=static build
ninja -C build install
popd

# Prompt for PKG_CONFIG_PATH
echo "Now, statically-linked libraries of both aml and neatvnc were installed, and you can set PKG_CONFIG_PATH properly for the Mado build system to detect and facilitate."
echo "For example, you can run:"
echo "  export PKG_CONFIG_PATH=\$(pwd)/_static/lib/\$(uname -m)-linux-gnu/pkgconfig/"
