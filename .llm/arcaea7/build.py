from pathlib import Path
import shlex
import subprocess
import shutil

root = Path(__file__).resolve().parents[2]
out = root / 'build/arcaea-7.0.260c/runtime'
out.mkdir(parents=True, exist_ok=True)
sdk = Path('/usr/local/vitasdk/bin')
symbols = {line.split('|')[0] for line in (root / 'src/bridges/symbol.def').read_text().splitlines() if '|' in line}
missing = [name for name in (out.parent / 'missing-imports.txt').read_text().splitlines() if name not in symbols]
source = ['#include <string.h>', '#include <psp2/kernel/clib.h>', '#include <psp2/kernel/processmgr.h>',
          '#include <loader.h>', '#include <bridges/audio/provider.h>',
          'static void stop(const char* name) { sceClibPrintf("ARCAE7 unsupported call: %s\\n", name); sceKernelExitProcess(71); }']
rows = []
for index, name in enumerate(missing):
    if name.startswith('AMEDIAFORMAT_KEY_'):
        source.append(f'static const char* missing_{index} = "{name.removeprefix("AMEDIAFORMAT_KEY_").lower()}";')
    else:
        source.append(f'static void missing_{index}(void) {{ stop("{name}"); }}')
    rows.append(f'{{"{name}", &missing_{index}}},')
source += ['static const struct { const char* name; void* value; } imports[] = {', *rows, '};',
           'extern void* __real_loader_get_bridge_proc_address(const char*);',
           'void* __wrap_loader_get_bridge_proc_address(const char* name) {',
           'void* value = __real_loader_get_bridge_proc_address(name); if(value) return value;',
           'for(unsigned i=0;i<sizeof(imports)/sizeof(imports[0]);++i) if(!strcmp(name,imports[i].name)) return imports[i].value;',
           'return 0; }',
           'static void unknown(void) { stop("new AudioProvider slot"); }',
           'uintptr_t probe_provider[] = {0,0, (uintptr_t)audio_provider_init, (uintptr_t)audio_provider_destruct,',
           '(uintptr_t)audio_provider_delete, (uintptr_t)audio_provider_load_bgm, (uintptr_t)audio_provider_unload_bgm,',
           '(uintptr_t)audio_provider_play_bgm, (uintptr_t)audio_provider_set_bgm_paused, (uintptr_t)audio_provider_get_bgm_position,',
           '(uintptr_t)audio_provider_set_bgm_position, (uintptr_t)audio_provider_set_channel_bgm_volume,',
           '(uintptr_t)unknown, (uintptr_t)unknown, (uintptr_t)audio_provider_preload_sfx, (uintptr_t)audio_provider_unload_sfx,',
           '(uintptr_t)unknown, (uintptr_t)audio_provider_stop_sfx_v3, (uintptr_t)audio_provider_get_sfx_volume_v3,',
           '(uintptr_t)audio_provider_set_sfx_volume_v3, (uintptr_t)audio_provider_set_sfx_volume, (uintptr_t)unknown,',
           '(uintptr_t)audio_provider_play_sfx_v3, (uintptr_t)audio_provider_update, (uintptr_t)unknown,',
           '(uintptr_t)audio_provider_suspend, (uintptr_t)audio_provider_resume, (uintptr_t)unknown};',
           'extern void* __real_loader_get_import_proc_address(dynalib_t*, const char*);',
           'void* __wrap_loader_get_import_proc_address(dynalib_t* lib,const char* name) {',
           'if(!strcmp(name,"_ZTV24AudioProviderFMODAndroid")) return probe_provider;',
           'return __real_loader_get_import_proc_address(lib,name); }']
generated = out / 'imports.c'
generated.write_text('\n'.join(source) + '\n')
obj = out / 'imports.o'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb', '-I', str(root / 'src'),
                '-I', str(root / 'src/bridges'), '-c', str(generated), '-o', str(obj)], check=True)
configured = shlex.split((root / 'build/no-apk/CMakeFiles/VitaArc.dir/link.txt').read_text())
objects = [str(root / 'build/no-apk' / arg) for arg in configured if arg.endswith('.obj')]
frames = out / 'frames.o'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb', '-I', str(root / 'src'),
    '-c', str(root / '.llm/arcaea7/frames.c'), '-o', str(frames)], check=True)
objects.append(str(frames))
relocation = (root / 'src/loader/relocation.c').read_text()
relocation_source = out / 'relocation.c'
relocation_source.write_text(relocation)
relocation_object = out / 'relocation.o'
subprocess.run([str(sdk / 'arm-vita-eabi-gcc'), '-O2', '-g', '-mthumb', '-I', str(root / 'src'),
    '-I', str(root / 'src/bridges'), '-I', str(root / 'src/loader'), '-c', str(relocation_source),
    '-o', str(relocation_object)], check=True)
objects = [str(relocation_object) if arg.endswith('/src/loader/relocation.c.obj') else arg for arg in objects]
libraries = configured[configured.index('-Wl,--whole-archive'):]
elf = out / 'probe.elf'
subprocess.run([str(sdk / 'arm-vita-eabi-g++'), '-Wl,-q', '-Wl,--wrap=loader_get_bridge_proc_address',
                '-Wl,--wrap=loader_get_import_proc_address', '-Wl,--wrap=vita_frame_step',
                '-Wl,--wrap=sceTouchPeek', str(obj), *objects, *libraries, '-o', str(elf)], check=True)
subprocess.run([str(sdk / 'vita-elf-create'), str(elf), str(out / 'probe.velf')], check=True)
subprocess.run([str(sdk / 'vita-make-fself'), '-s', '-c', str(out / 'probe.velf'), str(out / 'probe.self')], check=True)
app = out / 'portable/fs/ux0/app/VARC61616'
app.mkdir(parents=True, exist_ok=True)
shutil.copy2(out / 'probe.self', app / 'probe.self')
