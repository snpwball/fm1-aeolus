#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p prototype/build
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ivendor/Felucca-smk37/firmware/engines/src -Ivendor/Felucca-smk37/firmware/src \
  prototype/test_felucca_pipe.c -o prototype/build/test_felucca_pipe
prototype/build/test_felucca_pipe
