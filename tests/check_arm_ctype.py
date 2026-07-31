"""Check production ARM Bionic data imports and optional real-game XML parsing."""
import argparse
from pathlib import Path
import struct
from check_arm_loader import LoaderCase


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path)
    args = parser.parse_args()
    case = LoaderCase({"app0:/game.so": args.game.read_bytes()} if args.game else {})
    case.call("bridge_compat_initialize")
    tables = {}
    for imported, symbol, width in (("_ctype_", "bridge_bionic_ctype", 1),
                                    ("_tolower_tab_", "bridge_bionic_tolower", 2),
                                    ("_toupper_tab_", "bridge_bionic_toupper", 2)):
        address = case.call("loader_get_bridge_proc_address", case.string(imported))
        assert address == case.target.symbols[symbol]
        pointer = case.word(address)
        assert pointer != address, "Bionic data import must contain a pointer"
        tables[imported] = bytes(case.calls.machine.mem_read(pointer, 257 * width))
    flags = tables["_ctype_"]
    assert flags[0] == 0
    lower = struct.unpack("<257h", tables["_tolower_tab_"])
    upper = struct.unpack("<257h", tables["_toupper_tab_"])
    assert lower[0] == upper[0] == -1
    for byte in range(256):
        c = bytes([byte])
        assert lower[byte + 1] == c.lower()[0]
        assert upper[byte + 1] == c.upper()[0]
        expected = 0
        for mask, yes in ((1, c.isupper()), (2, c.islower()), (4, c.isdigit()),
                          (8, c.isspace()), (16, byte >= 160 or (33 <= byte <= 126 and not c.isalnum())),
                          (32, byte < 32 or 127 <= byte < 160),
                          (64, byte in b"ABCDEFabcdef"), (128, byte == 32)):
            if yes: expected |= mask
        assert flags[byte + 1] == expected, (byte, flags[byte + 1], expected)
    pointers = {name: case.word(case.target.symbols[name]) for name in
                ("bridge_bionic_ctype", "bridge_bionic_tolower", "bridge_bionic_toupper")}
    case.call("bridge_compat_initialize")
    assert pointers == {name: case.word(case.target.symbols[name]) for name in pointers}
    print("Production ARM ctype passed: data-symbol indirection, all 256 bytes, EOF, short case tables and repeated initialization.")
    if args.game:
        library = case.load("app0:/game.so")
        assert library
        for name in ("_ZN8tinyxml27XMLUtil14SkipWhiteSpaceEPKc", "_ZN8tinyxml27XMLUtil14SkipWhiteSpaceEPc"):
            function = case.export(library, name)
            assert function
            for text, skipped in ((" \t\r\n<plist>", 4), ("<plist>", 0), ("", 0), (" \t", 2)):
                address = case.string(text)
                assert case.calls.call(function, address) == address + skipped
        case.call("loader_free_library", library)
        case.clean()
        print("Actual game TinyXML whitespace parsers pass through production loader/imports.")


if __name__ == "__main__":
    main()
