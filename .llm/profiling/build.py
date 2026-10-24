from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / "build/cpu-profile-20261025"
out.mkdir(exist_ok=True)
sdk = Path("/usr/local/vitasdk/bin")
source = root / ".llm/profiling/probe.c"
obj = out / "probe.o"
subprocess.run([str(sdk / "arm-vita-eabi-gcc"), "-O2", "-g", "-mthumb", "-Wall",
    "-I", str(root / "src"), "-c", str(source), "-o", str(obj)], check=True)
configured = shlex.split((root / "build/no-apk/CMakeFiles/VitaArc.dir/link.txt").read_text())
objects = [str(root / "build/no-apk" / arg) for arg in configured if arg.endswith(".obj")]
buffers = out / "buffers.o"
subprocess.run([str(sdk / "arm-vita-eabi-gcc"), "-O3", "-mthumb", "-I", str(root / "src"),
    "-c", str(root / ".llm/profiling/buffers.c"), "-o", str(buffers)], check=True)
objects = [str(buffers) if arg.endswith("/src/bridges/opengl/buffers.c.obj") else arg for arg in objects]
libraries = configured[configured.index("-Wl,--whole-archive"):]
names = ["vita_frame_step", "vglSwapBuffers", "vita_audio_render", "sceTouchPeek", "glDrawArrays",
    "glDrawElements", "bridge_gl_bind_buffer", "glBufferData", "glBufferSubData",
    "bridge_gl_vertex_attrib_pointer", "glUniformMatrix4fv", "bridge_gl_get_error", "ov_read"]
elf = out / "profile.elf"
subprocess.run([str(sdk / "arm-vita-eabi-g++"), "-Wl,-q", *["-Wl,--wrap=" + name for name in names],
    str(obj), *objects, *libraries, "-o", str(elf)], check=True)
subprocess.run([str(sdk / "vita-elf-create"), str(elf), str(out / "profile.velf")], check=True)
subprocess.run([str(sdk / "vita-make-fself"), "-s", "-c", str(out / "profile.velf"),
    str(out / "profile.self")], check=True)
print(out / "profile.self")
