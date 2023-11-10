#!/bin/bash

## For CodeQL autobuild

set -x
set -e

sudo --non-interactive apt-get update --error-on=any
sudo --non-interactive apt-get install --yes libevdev2 libevdev-dev libsodium23 libsodium-dev pkg-config

make
