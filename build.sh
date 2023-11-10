#!/bin/bash

## For CodeQL autobuild

set -x
set -e

apt update --error-on=any
apt install --yes libevdev2 libevdev-dev libsodium23 libsodium-dev pkg-config

make
