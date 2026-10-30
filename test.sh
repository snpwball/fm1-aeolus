#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
python3 prepare_stops.py
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ivendor/Felucca-smk37/firmware/engines/src -Ivendor/Felucca-smk37/firmware/src \
  test_felucca_pipe.c test_baseline.c -o build/test_felucca_pipe
build/test_felucca_pipe
