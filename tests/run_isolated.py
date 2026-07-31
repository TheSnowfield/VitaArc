"""Run a host executable in a disposable working directory."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile


executable = str(Path(sys.argv[1]).resolve())
environment = os.environ.copy()
environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
with tempfile.TemporaryDirectory(prefix="vitaarc-host-") as directory:
    subprocess.run([executable, *sys.argv[2:]], cwd=directory, env=environment, check=True)
