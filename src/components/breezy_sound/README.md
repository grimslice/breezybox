# breezy_sound

The BreezyBox sound engine: an 8-voice PICO-8-flavored synth mixer, a
4-channel PICO-8 sfx/music tracker for game ports, and a single raw PCM
stream, mixed by a background task at 44.1 kHz s16 and fed to the board's
audio output. `snd_core.h` and `snd_p8.h` are the APIs that loadable ELF
apps (soundkeys, moddy, game ports, ...) link against by symbol name, so
they stay identical across boards.

Three front ends, one fixed-point oscillator core (`snd_osc.h`, waveform
shapes and amplitudes measured against PICO-8 via zepto8):

- `snd_note_on/off` — live voices with attack/release envelopes (music
  keyboards, synth apps)
- `snd_p8_load/sfx/music/stop` — fire-and-forget PICO-8 cart sound: load
  the sfx/pattern data once, then call `sfx()`/`music()` like the cart does;
  sequencing runs inside the mixer task
- `snd_stream_*` — raw PCM for apps that render their own audio (modplay)

The engine is hardware-agnostic. Each firmware project supplies one
`snd_port.c` implementing the small contract in `snd_port.h`:

- `snd_port_desc` — output format (mono/stereo) and the per-speaker limiter ceiling
- `snd_port_init()` — set up the codec/I2S path, leave it stopped
- `snd_port_start()` / `snd_port_stop()` — power the output up/down
- `snd_port_write()` — blocking write of one rendered chunk (this
  back-pressure paces the mixer)

See the reference ports in the BreezyBox repo:
`examples/p4-tanmatsu/main/snd_port.c` (ES8156 codec via badge-bsp, stereo)
and `examples/s3-waveshare/main/snd_port.c` (MAX98357A mono amp, bare I2S).

Remember to export the `snd_*` symbols to ELF apps in your firmware's
customer symbol table (see the examples' `elf_extras.c`).

## License

MIT
