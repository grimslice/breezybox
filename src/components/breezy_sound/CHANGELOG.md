# Changelog

## 1.1.0 - 2026-07-15

### Added

- PICO-8 sfx/music tracker (`snd_p8.h`): 4 channels rendered inside the mixer
  task at PICO-8's native 22050 Hz (2x upsampled into the 44.1 kHz bus). Apps
  load cart data once (`snd_p8_load`) and make fire-and-forget
  `snd_p8_sfx`/`snd_p8_music` calls — no renderer or per-frame pumping in the
  app. Fixed-point port of zepto8's player (Porklike game feature subset: slides,
  vibrato, drop, fades, arpeggios, loops, patterns, music fades, declick
  crossfade, key-tracked brown noise). `snd_all_off()` also stops the tracker.

### Fixed

- Limiter gain now slews per sample instead of jumping at chunk boundaries;
  the jumps caused loud pops/crackle on note onsets whenever the limiter engaged.

### Changed

- The PCM stream is scaled from int16 full scale to the board's `limit_peak` when
  mixed, instead of being hard-clamped at it.
- One shared oscillator core (`snd_osc.h`) for voices and tracker; waveform
  duty cycles and relative amplitudes now follow zepto8's measured PICO-8
  values (replacing the ear-tuned per-wave gain table — instrument balance in
  the note API shifts slightly).
- The note API's noise wave is now PICO-8's key-tracked brown noise instead
  of sample-and-hold.


## 1.0.0 - 2026-07-06

### Added

- Initial release: 8-voice synth (PICO-8-style waveforms), master limiter, master volume,
  and a single PCM stream mixed in post-limiter. Hardware access goes through the
  `snd_port.h` contract implemented per firmware project.
