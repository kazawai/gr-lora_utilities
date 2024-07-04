#!/bin/sh

LATEST_VERSION=`git ls-remote https://github.com/kazawai/gr-lora_utilities.git | grep HEAD | cut -f 1`
docker build -t kazawai/gr-lora_utilities --build-arg CACHEBUST=$LATEST_VERSION .
