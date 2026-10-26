#!/usr/bin/env python3
"""Prepare every local APK as an independently selectable version directory."""

import sys

from package_apk import main


if __name__ == "__main__":
    sys.argv.append("--all")
    raise SystemExit(main())
