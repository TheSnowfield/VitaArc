"""Run the real JNI bridge in isolated host processes, with a mock game SO."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil


def main():
    executable, game = map(lambda value: str(Path(value).resolve()), sys.argv[1:3])
    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    with tempfile.TemporaryDirectory(prefix="vitaarc-jni-") as directory:
        root = Path(directory)
        assets = root / "app0:package/assets"
        assets.mkdir(parents=True)
        (assets / "fixture.txt").write_bytes(b"mock asset payload")
        (assets / "empty.txt").write_bytes(b"")
        (assets / 'Fonts').mkdir()
        shutil.copyfile(sys.argv[3], assets / 'Fonts/test.ttf')
        for mode in ("runtime", "dispatch", "references", "write", "read", "startup", "bitmap", "loop", "io_failure", "corrupt"):
            print(f"[JNI mock] {mode}", flush=True)
            subprocess.run([executable, mode, game], cwd=root, env=environment, check=True)
        for version in ('1.6.1c', '1.9.0c', '2.0.0c', '3.0.0c'):
            (root / 'app0:vitaarc-apk-version.txt').write_text(version + '\n', encoding='ascii')
            environment['VITAARC_TEST_APK_VERSION'] = version
            subprocess.run([executable, 'startup', game], cwd=root, env=environment, check=True)
    print("JNI/Cocos closed loop passed (including independent-process persistence).")


if __name__ == "__main__":
    main()
