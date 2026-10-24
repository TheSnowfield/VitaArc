from pathlib import Path
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/arcaea-7.0.260c/runtime'
sdk = Path('/usr/local/vitasdk/bin')
obj = out / 'frames.o'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb', '-I', str(root / 'src'),
    '-c', str(root / '.llm/arcaea7/frames.c'), '-o', str(obj)], check=True)
assets = out / 'assets.o'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb', '-I', str(root / 'src/bridges'),
    '-c', str(root / '.llm/arcaea7/assets.c'), '-o', str(assets)], check=True)
memory = out / 'memory.o'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb',
    '-c', str(root / '.llm/compat/memory.c'), '-o', str(memory)], check=True)
build = root / 'build/release-7.0.260c'
configured = shlex.split((build / 'CMakeFiles/VitaArc.dir/link.txt').read_text())
objects = [str(build / arg) for arg in configured if arg.endswith('.obj')]
libraries = configured[configured.index('-Wl,--whole-archive'):]
subprocess.run([str(sdk / 'arm-vita-eabi-g++'), '-Wl,-q', '-Wl,--wrap=vita_frame_step', '-Wl,--wrap=sceTouchPeek',
    '-Wl,--wrap=malloc', '-Wl,--wrap=calloc',
    '-Wl,--wrap=fopen', '-Wl,--wrap=a_asset_manager_open',
    '-Wl,--wrap=sceKernelAllocMemBlock', '-Wl,--wrap=kuKernelAllocMemBlock',
    str(obj), str(assets), str(memory), *objects, *libraries, '-o', str(out / 'check.elf')], check=True)
subprocess.run([str(sdk / 'vita-elf-create'), str(out / 'check.elf'), str(out / 'check.velf')], check=True)
subprocess.run([str(sdk / 'vita-make-fself'), '-s', '-c', str(out / 'check.velf'), str(out / 'probe.self')], check=True)
shutil.copy2(out / 'probe.self', out / 'portable/fs/ux0/app/VARC61616/probe.self')
