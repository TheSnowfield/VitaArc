"""Owned-member/listener fixtures using actual game C++ routines and SDK objects."""

import re
import struct
import subprocess

from arm_fixture_build import sdk


def game_call_targets(case, library, game, name, offsets):
    assembly = subprocess.run([str(sdk / "bin/arm-vita-eabi-objdump"), "-d", "--no-show-raw-insn", "--disassemble=" + name, game],
                              capture_output=True, text=True, check=True).stdout
    start = re.search(r"^([0-9a-f]+) <" + re.escape(name) + r"(?:@@Base)?>:", assembly, re.M)
    assert start, assembly
    relative = int(start.group(1), 16)
    bias = (case.export(library, name) & ~1) - relative
    branches = {int(address, 16): int(target, 16) for address, target in re.findall(r"^\s*([0-9a-f]+):\s+bl\s+([0-9a-f]+)\b", assembly, re.M)}
    return {key: (branches[relative + offset] + bias) | 1 for key, offset in offsets}


class PurchaseFixture:
    def __init__(self, case, library, image, game, purchase, mode):
        self.case, self.library, self.image = case, library, image
        self.game, self.purchase, self.mode = game, purchase, mode
        self.resources = set()
        self.wrapper = 0
        self.listener = 0
        self.slot = 0
        self.extra_registrations = 0
        self.wrapper_slot_cleared = False
        self.mismatched_symbol = None
        self.members = mode in ("purchase-members", "purchase-members-shared")

    def prepare(self, strings):
        case, purchase = self.case, self.purchase
        if self.members:
            before = set(case.platform.allocations)
            allocate = case.export(self.library, "_Znwj")
            deallocate = case.export(self.library, "_ZdlPv")
            text = case.calls.SCRATCH + 0x2100
            case.calls.machine.mem_write(text, b"purchase member COW lifetime\0")
            for index, offset in enumerate((4, 8, 20, 80, 84, 88)):
                case.calls.call(strings["destroy"], purchase + offset)
                if index and self.mode == "purchase-members-shared":
                    case.calls.call(strings["copy"], purchase + offset, purchase + 4)
                else:
                    case.calls.call(strings["construct"], purchase + offset, text, text + 128)
            move = game_call_targets(case, self.library, self.game, "_ZN15PurchaseManager28loadPurchasesFromSecureStoreEv", [("move", 0x2a)])["move"]
            source = case.calls.SCRATCH + 0x2200
            for offset in (44, 56, 68):
                storage = case.calls.call(allocate, 12)
                assert storage
                for index in range(2):
                    case.calls.call(strings["copy"], storage + index * 4, purchase + (4 if index == 0 else 8))
                case.calls.word(storage + 8, 0xdeadbeef)  # Unconstructed capacity must not be destroyed.
                case.calls.machine.mem_write(source, struct.pack("<III", storage, storage + 8, storage + 12))
                case.calls.call(move, purchase + offset, source)
                assert bytes(case.calls.machine.mem_read(source, 12)) == bytes(12)
            source_function = case.calls.SCRATCH + 0x2300
            api = source_function + 32
            case.calls.machine.mem_write(api, struct.pack("<6I", allocate, deallocate, strings["copy"], strings["destroy"], purchase,
                                                        case.export(self.library, "_ZTV15PurchaseManager") + 8))
            case.calls.call(self.image.symbols["test_purchase_function_source"], source_function, purchase + 4, api)
            copy_function = case.export(self.library, "_ZNSt8functionIFvb14PurchaseResultEEC1ERKS2_")
            destroy_function = case.export(self.library, "_ZNSt14_Function_baseD1Ev")
            assert copy_function and destroy_function
            case.calls.call(copy_function, purchase + 24, source_function)
            case.calls.call(destroy_function, source_function)
            self.resources = set(case.platform.allocations) - before
            assert len(self.resources) == (5 if self.mode == "purchase-members-shared" else 10)
            assert self.stats()[:3] == (1, 1, 0)
        if self.mode in ("purchase-listener", "purchase-foreign-listener", "purchase-listener-oom", "purchase-unknown-wrapper"):
            getter = case.export(self.library, "_ZN6sdkbox10IAPWrapper11getInstanceEv")
            base = getter & ~1
            self.slot = base + 0x10 + struct.unpack("<i", case.calls.machine.mem_read(base + 0x9c, 4))[0]
            assert case.word(self.slot) == 0
            self.wrapper = case.calls.call(case.export(self.library, "_Znwj"), 44)
            case.calls.call(case.export(self.library, "_ZN6sdkbox17IAPWrapperEnabledC1Ev"), self.wrapper)
            case.calls.word(self.slot, self.wrapper)
            before = set(case.platform.allocations)
            assert case.calls.call(getter) == self.wrapper  # Real factory's cached path, not a matching formula alone.
            assert set(case.platform.allocations) == before
            self.listener = case.calls.SCRATCH + 0x2700 if self.mode == "purchase-foreign-listener" else purchase + 92
            case.calls.call(case.export(self.library, "_ZN6sdkbox17IAPWrapperEnabled11setListenerEPNS_11IAPListenerE"), self.wrapper, self.listener)
            if self.mode == "purchase-unknown-wrapper":
                case.calls.word(self.wrapper, case.export(self.library, "_ZTV11AppDelegate") + 8)
            owner = case.calls.SCRATCH + 0x2800
            expected = self.listener if self.mode in ("purchase-foreign-listener", "purchase-unknown-wrapper") else 0
            case.calls.machine.mem_write(owner, struct.pack("<4I", self.wrapper, self.slot,
                                                          case.export(self.library, "_ZN6sdkbox17IAPWrapperEnabledD0Ev"), expected))
            on_load = case.export(self.library, "JNI_OnLoad")
            assert case.call("bridge_cxa_atexit", self.image.symbols["test_destroy_purchase_wrapper"], owner, on_load) == 0
            self.extra_registrations = 1
        mismatches = {
            "purchase-schema-mismatch": "_ZN15PurchaseManager28loadPurchasesFromSecureStoreEv",
            "purchase-vector-mismatch": "_ZNSt6vectorISsSaISsEED1Ev",
            "purchase-function-mismatch": "_ZNSt14_Function_baseD1Ev",
            "purchase-iap-mismatch": "_ZN6sdkbox10IAPWrapper11getInstanceEv"}
        if self.mode in mismatches:
            name = case.string(mismatches[self.mode])
            previous = case.calls.call(self.image.symbols["test_purchase_symbol_size"], self.library, name, 0, instruction_limit=20000000)
            assert previous > 0
            self.mismatched_symbol = (mismatches[self.mode], previous)

    def stats(self):
        return struct.unpack("<5I", self.case.calls.machine.mem_read(self.image.symbols["purchase_fixture_stats"], 20))

    def assert_prepare_intact(self):
        if self.wrapper:
            assert self.case.word(self.slot) == self.wrapper
            assert self.case.word(self.wrapper + 12) == self.listener

    def before_service_cleanup(self):
        if self.wrapper:
            # The cached slot belongs to the DSO: check while mappings are live,
            # after the fixture owner finalized, rather than after unmapping.
            assert self.stats()[3] == 1 and self.case.word(self.slot) == 0
            self.wrapper_slot_cleared = True

    def repair_prepare(self):
        if self.mode == "purchase-unknown-wrapper":
            self.case.calls.word(self.wrapper, self.case.export(self.library, "_ZTVN6sdkbox17IAPWrapperEnabledE") + 8)
            self.case.calls.word(self.case.calls.SCRATCH + 0x2800 + 12, 0)
        elif self.mode == "purchase-iap-mismatch":
            name, size = self.mismatched_symbol
            assert self.case.calls.call(self.image.symbols["test_purchase_symbol_size"], self.library, self.case.string(name), size, instruction_limit=20000000) == 0
        else:
            raise AssertionError(self.mode)

    def check(self):
        stats = self.stats()
        assert stats[:3] == ((1, 2, 0) if self.members else (0, 0, 0)), stats
        assert stats[3] == (1 if self.wrapper else 0) and stats[4] == 0, stats
        assert not (self.resources & self.case.platform.allocations.keys()), "owned purchase resources retained"
        if self.wrapper:
            assert self.wrapper_slot_cleared and self.wrapper not in self.case.platform.allocations
        return {"purchase_member_allocations_released": len(self.resources),
                "purchase_function_clones": stats[0], "purchase_function_destroys": stats[1],
                "purchase_function_invokes": stats[2], "fixture_iap_wrappers_destroyed": stats[3]}
