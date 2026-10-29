# Aeolus stop set: map and size

`aeolus49.csv` lists all 49 ranks from `stops-0.4.0/Aeolus/definition`, in that file's order. 36 belong to manuals III, II, and I. 13 belong to the pedal. A sketch placed 48 of them on six pages in order. Subbass 16 is kept separate because the pedal pads already use it, and fader 4 can mute the pedal. That is not a separate pedal-stop switch. Aeolus divisions are not one keyboard: pedal ranks play from the pedal, manual ranks from the selected manual.

The 49 stock `.ae0` files are 630336 bytes of synthesis parameters, not PCM. `export` runs Aeolus `Rankwave::gen_waves` for 37 notes. Storing every third pipe at 8 bit, 13 source notes per rank, was measured at 2704239 bytes of wave data. The per-rank list is in the CSV. Ranks that end before MIDI 84 repeat their last pipe in that export, so the top of those ranks is only an estimate.

The last checked SMK image was 593024 bytes. `build/app.ld` limits XIP to 0x97400 (619520 bytes), leaving about 26496 bytes. Do not put the tables in VM or USRFLASH, and do not move calibration. Full rendered tables do not fit in that region. A packed parameter set, or building the wave on key-down, still needs its own accuracy and speed check. Load time and polyphony are not measured.

Still open: pack Aeolus harmonic level and attack across the compass, compare the result with `Rankwave` on attack, sustain, and release, and measure fixed-point cost for 8 notes plus RAM and XIP on an SMK build. A rank is not one sine. Pipe differences, attack, mixtures, instability, and release matter. The 8-manual-stop build is the one that exists. Six pages and the full set are not in it.
