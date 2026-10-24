# Audio streaming

## Ownership and locks

`audio/feed.c` owns a codec stream, one native pthread and a bounded PCM queue
per BGM channel. The worker alone reads and seeks the codec after creation.
The existing MP3 and Vorbis stream interfaces remain synchronous codec APIs.

Each feed holds sixteen stereo PCM blocks of 1024 frames at 48 kHz, roughly
341 ms for a non-wrapping track. Open and seek prime up to four blocks before
returning. The decoder then fills remaining capacity and sleeps on a condition
variable while full or at EOF. Pause does not consume the queue.

The feed mutex covers only queue metadata and bounded PCM copies. Disk reads,
decoder calls and codec seeks occur outside that mutex. A separate control
mutex serializes seeks. The output mixer never waits on a condition variable;
a temporary empty queue supplies silence without declaring the track finished.
EOF stops playback after all queued samples have been consumed.

The backend mutex protects channels, voices and volumes. BGM control functions
retain a feed under that mutex, release the mutex before waiting for a seek,
then commit channel changes only if it still owns the same feed. Unload detaches
the channel before releasing its reference. Final release stops and joins the
worker before closing the codec and destroying synchronization resources.

## Positions and looping

Position queries return the consumer position. Decoder read-ahead does not move
the game clock. Each PCM block records the codec position before and after
decoding. A block ends at a loop boundary, so partial consumption advances
normally and consuming its final sample applies the recorded loop wrap.

Seek blocks PCM consumption for that feed, clears queued data after moving the
codec, and primes replacement PCM. On a failed seek the worker restores the
previous consumer position rather than the decoder's read-ahead position.
Other BGM channels and SFX remain independent of this wait.

## File and hardware output

The MP3 reader has a 32 KiB stdio buffer. Header detection advances a sliding
four-byte window; normal frame decoding copies that header and reads only the
remaining payload. It no longer uses ftell/fseek for every encoded frame.
Reset and seek retain their existing decoder/resampler behavior.

The output thread mixes into two alternating, 64-byte-aligned PCM buffers.
It drains the final submitted buffer before its stack is released. The native
sceAudioOutOutput call remains blocking and supplies the playback cadence.
The double-buffer and alignment design is consistent with
[SDL's Vita audio backend](https://github.com/libsdl-org/SDL/blob/SDL2/src/audio/vita/SDL_vitaaudio.c).

Generic file reads and explicit load/seek calls keep their synchronous return
contracts. The continuous BGM read/decode path runs in feed workers.
