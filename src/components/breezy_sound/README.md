# breezy_sound

The BreezyBox sound engine: an 8-voice PICO-8-flavored synth mixer plus a
single raw PCM stream, mixed by a background task at 44.1 kHz s16 and fed to
the board's audio output. `snd_core.h` is the API that loadable ELF apps
(soundkeys, moddy, ...) link against by symbol name, so it stays identical
across boards.

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
