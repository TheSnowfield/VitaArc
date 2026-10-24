from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
sdk = Path('/usr/local/vitasdk/bin')
out = root / 'build/song-switch-20261025'
objects = root / 'build/no-apk/CMakeFiles/VitaArc.dir/src'
sources = ['bridges/audio/mp3.c', 'bridges/audio/mp3_frame.c', 'bridges/audio/mp3_index.c',
           'bridges/audio/feed.c', 'bridges/audio/stream.c',
           'bridges/audio/mp3_marks.c',
           *['bridges/audio/' + name + '.c' for name in ('backend', 'common', 'mixer', 'bgm', 'sfx', 'wav')],
           'logcat/logcat.c', 'platform/stdio.c']
elf = out / 'seek.elf'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb', '-Wall', '-Wextra', '-Werror',
    '-I', str(root / 'src'), str(root / '.llm/song-switch/seek.c'),
    *[str(objects / (source + '.obj')) for source in sources],
    '-Wl,-q,--wrap=sceAudiodecDecode,--wrap=__sfp,--wrap=__sinit,--wrap=__sfp_lock_acquire,--wrap=__sfp_lock_release',
    '-Wl,--wrap=sceAudiodecCreateDecoder,--wrap=sceAudiodecDeleteDecoder,--wrap=pthread_create,--wrap=pthread_join',
    '-Wl,--whole-archive', '-lpthread', '-Wl,--no-whole-archive',
    '-lvorbisfile', '-lvorbis', '-logg', '-lm',
    '-lSceLibKernel_stub', '-lSceAudio_stub', '-lSceAudiodec_stub', '-lSceSysmodule_stub', '-o', str(elf)], check=True)
subprocess.run([str(sdk / 'vita-elf-create'), str(elf), str(out / 'seek.velf')], check=True)
subprocess.run([str(sdk / 'vita-make-fself'), '-s', '-c', str(out / 'seek.velf'), str(out / 'seek.self')], check=True)
print(out / 'seek.self')
