# third_party

`minimp3.h`, `minimp3_ex.h` — MP3 decoder by lieff, public domain (CC0).
Upstream: https://github.com/lieff/minimp3 (files copied unmodified, licence in `minimp3.LICENSE`).

Used by `src/external_music.h` to stream the player's own tracks from `audio/`.
Compiled through `#define MINIMP3_IMPLEMENTATION` in that header, so no extra
build step or linker flag is needed.
