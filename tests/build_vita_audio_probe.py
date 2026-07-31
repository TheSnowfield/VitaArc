"""Build a standalone SELF using production MP3 code and real SDK endpoints.

Run inside an existing isolated VitaArc app with its local game assets.
This does not build a VPK or alter the normal VitaArc target.
"""

import argparse
import subprocess

from arm_fixture_build import root, sdk, compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vorbis", action="store_true")
    args = parser.parse_args()
    name = "vorbis" if args.vorbis else "audio"
    output = root / f"build/vita-{name}-probe"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / f"{name}-probe.elf"
    subprocess.run([
        compiler, "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-mthumb",
        "-I", str(root / "src"),
        *(["-DVITAARC_VORBIS_PROBE"] if args.vorbis else []),
        str(root / "tests/vita_audio_probe.c"), str(root / "src/bridges/audio/mp3.c"),
        str(root / "src/bridges/audio/stream.c"),
        *[str(root / "src/bridges/audio" / (name + ".c"))
          for name in ("backend", "common", "mixer", "bgm", "sfx", "wav")],
        str(root / "src/logcat/logcat.c"),
        "-Wl,--whole-archive", "-lpthread", "-Wl,--no-whole-archive",
        "-lvorbisfile", "-lvorbis", "-logg", "-lm",
        "-lSceAudiodec_stub", "-lSceAudio_stub", "-lSceSysmodule_stub",
        "-lSceNet_stub", "-lSceNetCtl_stub", "-lSceLibKernel_stub",
        "-o", str(executable)], check=True)
    velf = output / f"{name}-probe.velf"
    subprocess.run([str(sdk / "bin/vita-elf-create"), str(executable), str(velf)], check=True)
    subprocess.run([str(sdk / "bin/vita-make-fself"), "-c", str(velf),
                    str(output / f"{name}-probe.self")], check=True)
    print(output / f"{name}-probe.self")


if __name__ == "__main__":
    main()
