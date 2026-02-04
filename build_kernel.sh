#!/bin/bash

export ARCH=arm64
export ANDROID_MAJOR_VERSION=q
make crownlte_defconfig
make -j64
