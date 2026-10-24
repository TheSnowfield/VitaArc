from pathlib import Path
import argparse
import shlex
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', action='store_true')
parser.add_argument('--out', default='build/song-switch-20261025')
args = parser.parse_args()
out = root / args.out
out.mkdir(parents=True, exist_ok=True)
sdk = Path("/usr/local/vitasdk/bin")
obj = out / "probe.o"
subprocess.run([str(sdk / "arm-vita-eabi-gcc"), "-O2", "-g", "-mthumb", "-Wall",
    "-I", str(root / "src"), "-c", str(root / ".llm/song-switch/probe.c"), "-o", str(obj)], check=True)
configured = shlex.split((root / "build/no-apk/CMakeFiles/VitaArc.dir/link.txt").read_text())
objects = [str(root / "build/no-apk" / arg) for arg in configured if arg.endswith(".obj")]
buffers = out / "buffers.o"
subprocess.run([str(sdk / "arm-vita-eabi-gcc"), "-O3", "-mthumb", "-I", str(root / "src"),
    "-c", str(root / ".llm/profiling/buffers.c"), "-o", str(buffers)], check=True)
objects = [str(buffers) if arg.endswith("/src/bridges/opengl/buffers.c.obj") else arg for arg in objects]
if args.baseline:
    for name in ('mp3', 'feed', 'bgm'):
        source = out / ('baseline-' + name + '.c')
        source.write_bytes(subprocess.check_output(['git', 'show', 'HEAD:src/bridges/audio/' + name + '.c'], cwd=root))
        target = out / ('baseline-' + name + '.o')
        subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O3', '-DNDEBUG', '-DVITAARC_VERBOSE_LOGS=0',
            '-I', str(root / 'src'), '-I', str(root / 'src/bridges/audio'),
            '-c', str(source), '-o', str(target)], check=True)
        objects = [str(target) if arg.endswith('/src/bridges/audio/' + name + '.c.obj') else arg for arg in objects]
libraries = configured[configured.index("-Wl,--whole-archive"):]
names = ["vita_frame_step", "vglSwapBuffers", "vita_audio_render", "sceTouchPeek", "glDrawArrays",
    "glDrawElements", "bridge_gl_bind_buffer", "glBufferData", "glBufferSubData",
    "bridge_gl_vertex_attrib_pointer", "glUniformMatrix4fv", "bridge_gl_get_error", "ov_read",
    "sceAudiodecDecode", "vita_mp3_seek_frames", "vita_mp3_open", "audio_feed_seek",
    "audio_feed_close", "vita_audio_load_bgm", "vita_audio_play_bgm", "mp3_index_find",
    "sceAudiodecCreateDecoder", "sceAudiodecDeleteDecoder", "bridge_gl_tex_image_2d", "pthread_join"]
name = 'baseline-profile' if args.baseline else 'profile'
elf = out / (name + '.elf')
subprocess.run([str(sdk / "arm-vita-eabi-g++"), "-Wl,-q", *["-Wl,--wrap=" + name for name in names],
    str(obj), *objects, *libraries, "-o", str(elf)], check=True)
subprocess.run([str(sdk / "vita-elf-create"), str(elf), str(out / (name + '.velf'))], check=True)
subprocess.run([str(sdk / "vita-make-fself"), "-s", "-c", str(out / (name + '.velf')),
    str(out / (name + '.self'))], check=True)
print(out / (name + '.self'))
