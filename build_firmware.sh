#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 prepare_stops.py
BOARD=smk37 FELUCCA_SLICE=0 vendor/Felucca-smk37/build.sh "$@"
python3 prepare_install.py
