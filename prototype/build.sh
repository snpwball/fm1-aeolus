#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
source_dir=vendor/aeolus-main/source
mkdir -p prototype/build
c++ -std=c++11 -O2 -I"$source_dir" prototype/reference.cc "$source_dir/addsynth.cc" "$source_dir/rankwave.cc" "$source_dir/rngen.cc" "$source_dir/exp2ap.cc" -o prototype/build/reference
for stop in I_principal_8 flute8 I_trumpet; do
  prototype/build/reference vendor/stops-0.4.0 "$stop.ae0" "prototype/build/$stop.wav"
done
