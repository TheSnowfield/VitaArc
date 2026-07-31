"""Run actual game constructors/JNI_OnLoad and require complete heap teardown.

Uses the production loader/runtime. Native CRT per-thread state and pthread
identity/key deletion are modeled; no game routine, JNI service or destructor
is replaced. The current 1.6.1c run reaches JNI 1.4 and requires zero allocations
after teardown. Rejection faults retain unsupported objects and fail the same
whole-heap gate. This diagnostic is not a game boot proof.
"""

import argparse
from collections import Counter
import json
import struct
import subprocess
from bisect import bisect_right

from arm_fixture_build import compile_source, link_fixture, root, sdk
from check_arm_bridges import ArmImage
from check_arm_loader import LoaderCase
from arm_purchase_fixture import PurchaseFixture, game_call_targets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True, help="Local 1.6.1c libcocos2dcpp.so")
    parser.add_argument("--cleanup-case", default="normal", choices=("normal", "foreign-analytics", "foreign-achievement", "foreign-platform", "foreign-purchase", "bootstrap-mismatch", "constructor-mismatch", "string-caller-mismatch", "string-destructor-mismatch", "prepare-oom", "platform-nonempty", "platform-shared", "purchase-members", "purchase-members-shared", "purchase-listener", "purchase-foreign-listener", "purchase-listener-oom", "purchase-unknown-wrapper", "purchase-schema-mismatch", "purchase-vector-mismatch", "purchase-function-mismatch", "purchase-iap-mismatch"),
                        help="Teardown fault injection; the final whole-heap gate remains strict in every case")
    args = parser.parse_args()
    from pathlib import Path
    data = Path(args.game).read_bytes()
    case = LoaderCase({"app0:/libcocos2dcpp.so": data})
    origins = {}
    stacks = {}
    allocate = case.platform.allocate

    def allocate_tracked(size):
        address = allocate(size)
        origins[address] = case.calls.reg("LR")
        stack = case.calls.reg("SP")
        available = min(128, case.calls.STACK + 0x10000 - stack) & ~3
        stacks[address] = struct.unpack("<" + "I" * (available // 4), case.calls.machine.mem_read(stack, available))
        return address

    case.platform.allocate = allocate_tracked
    library = case.load("app0:/libcocos2dcpp.so")
    assert library
    output = root / "build/arm-game-bootstrap"
    if args.cleanup_case != "normal":
        output = output / args.cleanup_case
    output.mkdir(parents=True, exist_ok=True)
    sources = (root / "tests/test_arm_game_bootstrap.c", root / "tests/test_arm_memory.c", root / "tests/test_arm_purchase.c")
    objects = [compile_source(source, output / (source.stem + ".o"), includes=[root / "src"]) for source in sources]
    executable = output / "bootstrap.elf"
    link_fixture(objects, executable, "test_initialize_reent",
                 exports=["test_reent_size", "test_pending_exception", "test_destroy_vm", "test_purchase_function_source", "test_destroy_purchase_wrapper", "test_purchase_symbol_size", "purchase_fixture_stats"])
    fixture = ArmImage(case.calls.machine, executable)
    assert case.calls.call(fixture.symbols["test_reent_size"]) < 0x4000
    case.calls.call(fixture.symbols["test_initialize_reent"], case.calls.SCRATCH + 0x5000)
    case.calls.register(case.target.symbols["pthread_self"], lambda: case.platform.return_value(7))

    def delete_key():
        case.platform.keys.pop(case.calls.reg("R0"), None)
        case.platform.return_value(0)

    case.calls.register(case.target.symbols["pthread_key_delete"], delete_key)

    def console():
        assert case.platform.string(case.calls.reg("R0")) == "%s"
        message = case.platform.string(case.calls.reg("R1"))
        case.platform.messages.append(message)
        case.platform.return_value(len(message))

    case.calls.register(case.target.symbols["sceClibPrintf"], console)
    registrations = []

    def register():
        registrations.append(tuple(case.calls.reg(name) for name in ("R0", "R1", "R2")))
        return False  # Observe entry; execute the production registration body.

    case.calls.register(case.target.symbols["bridge_cxa_atexit"], register)
    try:
        case.call("bridge_compat_initialize")
        case.call("bridge_patch_gl", library)
        case.call("bridge_patch_cocos2dx", library)
        assert case.call("loader_init_library", library) == 1
        constructor_allocations = set(case.platform.allocations)
        assert len(registrations) == 896, "fixture expects local 1.6.1c constructors"
        vm = case.call("jni_runtime_get_java_vm")
        env = case.call("jni_runtime_get_env")
        on_load = case.export(library, "JNI_OnLoad")
        version = case.calls.call(on_load, vm, 0, instruction_limit=20000000)
        assert version == 0x10004
        assert case.calls.call(fixture.symbols["test_pending_exception"], env) == 0
        print(f"Actual game constructors returned; {len(registrations)} C++ callbacks registered; JNI_OnLoad returned JNI 1.4 without exception.", flush=True)
        game_registrations = len(registrations)
        fixture_registrations = 0
        application_slot = case.export(library, "_ZN7cocos2d11Application21sm_pSharedApplicationE")
        application = case.word(application_slot)
        services = {"analytics": case.word(application + 12), "achievement": case.word(application + 20), "platform": case.word(application + 16), "purchase": case.word(application + 8)}
        assert all(case.platform.allocations[address] == {"platform": 20, "purchase": 96}.get(name, 4) for name, address in services.items())
        platform_strings = set()
        released_strings = []
        string_callees = {}
        if args.cleanup_case in ("platform-nonempty", "platform-shared", "string-destructor-mismatch", "purchase-members", "purchase-members-shared"):
            # GNU objdump independently resolves the real game's call targets;
            # this oracle does not use the bridge's Thumb BL decoder.
            string_callees = game_call_targets(case, library, args.game, "Java_org_cocos2dx_cpp_AppActivity_setDeviceId",
                                              (("construct", 0x28), ("assign", 0x3c), ("destroy", 0x42), ("copy", 0x30)))
        if args.cleanup_case in ("platform-nonempty", "platform-shared"):
            platform = services["platform"]
            text = b"VitaArc owned COW string lifetime\0"
            case.calls.machine.mem_write(case.calls.SCRATCH + 0x1800, text)
            for offset in (4, 8):
                case.calls.call(string_callees["destroy"], platform + offset)
            case.calls.call(string_callees["construct"], platform + 4, case.calls.SCRATCH + 0x1800, case.calls.SCRATCH + 0x1900)
            if args.cleanup_case == "platform-shared":
                case.calls.call(string_callees["copy"], platform + 8, platform + 4)
                external = case.calls.SCRATCH + 0x1910
                case.calls.call(string_callees["copy"], external, platform + 4)
                # A third owner is destroyed by C++ module finalization before
                # the two platform-owned references. Nothing fakes refcounts.
                assert case.call("bridge_cxa_atexit", string_callees["destroy"], external, on_load) == 0
                fixture_registrations = 1
                assert case.word(platform + 4) == case.word(platform + 8) == case.word(external)
            else:
                case.calls.call(string_callees["construct"], platform + 8, case.calls.SCRATCH + 0x1800, case.calls.SCRATCH + 0x1900)
                assert case.word(platform + 4) != case.word(platform + 8)
            platform_strings = {case.word(platform + offset) - 12 for offset in (4, 8)}
            assert all(address in case.platform.allocations for address in platform_strings)
        purchase_fixture = PurchaseFixture(case, library, fixture, args.game, services["purchase"], args.cleanup_case)
        purchase_fixture.prepare(string_callees)
        fixture_registrations += purchase_fixture.extra_registrations
        if args.cleanup_case.startswith("foreign-"):
            name = args.cleanup_case.removeprefix("foreign-")
            symbol = {"analytics": "_ZTV16AnalyticsManager", "achievement": "_ZTV18AchievementManager", "platform": "_ZTV13PlatformUtils", "purchase": "_ZTV15PurchaseManager"}[name]
            vtable = case.export(library, symbol)
            assert vtable
            case.calls.word(services[name], vtable + 8)
        elif args.cleanup_case.endswith("-mismatch") and not args.cleanup_case.startswith("purchase-"):
            names = {"bootstrap-mismatch": "_Z22cocos_android_app_initP7_JNIEnv",
                     "constructor-mismatch": "_ZN11AppDelegateC1EP13AudioProviderP15PurchaseManagerP16AnalyticsManagerP13PlatformUtilsP18AchievementManager",
                     "string-caller-mismatch": "Java_org_cocos2dx_cpp_AppActivity_setDeviceId"}
            address = (string_callees["destroy"] if args.cleanup_case == "string-destructor-mismatch" else case.export(library, names[args.cleanup_case])) & ~1
            if args.cleanup_case == "string-destructor-mismatch":
                # Inert padding before this function's literal pool is not
                # executed by other static string destructors during teardown.
                address += 0x4e
                assert bytes(case.calls.machine.mem_read(address, 2)) == b"\x00\xbf"
            byte = bytes(case.calls.machine.mem_read(address, 1))[0]
            # Mutate unused code/padding only, testing profile rejection without
            # corrupting the destructors used elsewhere by module finalization.
            case.calls.machine.mem_write(address, bytes([byte ^ 1]))
        cleanup_phases = []
        released_services = []

        def service_cleanup():
            assert case.word(case.target.symbols["__exit_entries"]) == 0
            purchase_fixture.before_service_cleanup()
            cleanup_phases.append(True)
            return False

        case.calls.register(case.target.symbols["__cleanup_cocos_services"], service_cleanup)
        release = case.platform.release

        def release_observed(address):
            if address in services.values():
                assert cleanup_phases, "service released before module finalizers drained"
                released_services.append(address)
            if address in platform_strings:
                assert cleanup_phases, "platform string released before finalization/other COW owners"
                released_strings.append(address)
            if address in purchase_fixture.resources:
                assert cleanup_phases, "purchase members released before module finalizers drained"
            release(address)

        case.platform.release = release_observed
        if args.cleanup_case in ("prepare-oom", "purchase-listener-oom"):
            pending = case.word(case.target.symbols["__exit_entries"])
            allocation_count = len(case.platform.allocations)
            failed = []
            calloc = case.platform.calloc

            def fail_prepare():
                failed.append(True)
                case.platform.return_value(0)

            case.calls.register(case.target.symbols["calloc"], fail_prepare)
            case.call("loader_free_library", library)
            assert len(failed) == 1 and not cleanup_phases and not released_services
            assert case.word(application_slot) == application
            assert case.word(case.target.symbols["__exit_entries"]) == pending
            assert len(case.platform.allocations) == allocation_count
            assert case.export(library, "JNI_OnLoad") == on_load
            purchase_fixture.assert_prepare_intact()
            case.calls.register(case.target.symbols["calloc"], calloc)
        if args.cleanup_case in ("purchase-unknown-wrapper", "purchase-iap-mismatch"):
            pending = case.word(case.target.symbols["__exit_entries"])
            allocation_count = len(case.platform.allocations)
            case.call("loader_free_library", library)
            assert case.word(application_slot) == application and not cleanup_phases and not released_services
            assert case.word(case.target.symbols["__exit_entries"]) == pending
            assert len(case.platform.allocations) == allocation_count and case.export(library, "JNI_OnLoad") == on_load
            purchase_fixture.assert_prepare_intact()
            purchase_fixture.repair_prepare()
        case.call("loader_free_library", library)
        expected = set(services.values())
        if args.cleanup_case.startswith("foreign-"):
            expected.remove(services[args.cleanup_case.removeprefix("foreign-")])
        elif args.cleanup_case in ("bootstrap-mismatch", "constructor-mismatch"):
            expected.clear()
        elif args.cleanup_case in ("string-caller-mismatch", "string-destructor-mismatch"):
            expected.remove(services["platform"])
            expected.remove(services["purchase"])
        elif args.cleanup_case in ("purchase-schema-mismatch", "purchase-vector-mismatch", "purchase-function-mismatch"):
            expected.remove(services["purchase"])
        assert set(released_services) == expected and len(released_services) == len(expected)
        assert len(cleanup_phases) == (1 if expected else 0)
        assert set(released_strings) == platform_strings and len(released_strings) == len(platform_strings)
        purchase_report = purchase_fixture.check()
        assert case.word(case.target.symbols["__exit_entries"]) == 0
        assert case.calls.call(fixture.symbols["test_destroy_vm"], vm) == 0
        case.call("jni_runtime_get_stats", case.calls.SCRATCH + 0x1000)
        objects, references = struct.unpack("<II", case.calls.machine.mem_read(case.calls.SCRATCH + 0x1000, 8))
        assert objects == references == 0
        assert not case.platform.blocks and not case.platform.handles and not case.platform.locks
        report = {
            **purchase_report,
            "cleanup_case": args.cleanup_case,
            "bootstrap_services_released": len(released_services),
            "platform_string_allocations_released": len(released_strings),
            "fixture_cxx_registrations": fixture_registrations,
            "jni_version": hex(version), "cxx_registrations": game_registrations,
            "jni_objects_after_destroy": objects, "jni_references_after_destroy": references,
            "retained_native_allocations": len(case.platform.allocations),
            "retained_native_bytes": sum(case.platform.allocations.values()),
            "retained_from_constructors": len(set(case.platform.allocations) & constructor_allocations),
            "allocation_origins": dict(Counter(hex(origins[address]) for address in case.platform.allocations)),
        }
        # Bounded retained-only evidence. Saved stack words can be function
        # operands as well as return PCs; these are candidates, not an unwind.
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + index * header[11])
                    for index in range(header[12])]
        functions = {}
        vtables = {}
        entry_relative = None
        for section in sections:
            if section[1] not in (2, 11):
                continue
            names_section = sections[section[6]]
            names = data[names_section[4]:names_section[4] + names_section[5]]
            for offset in range(section[4], section[4] + section[5], section[9]):
                name, value, size, info, _, owner = struct.unpack_from("<IIIBBH", data, offset)
                symbol_name = names[name:].split(b"\0", 1)[0].decode()
                if owner and symbol_name.startswith("_ZTV"):
                    vtables[(value & ~1) + 8] = symbol_name
                if owner and info & 15 == 2 and size:
                    function_name = symbol_name
                    functions[value & ~1] = (size, function_name)
                    if function_name == "JNI_OnLoad":
                        entry_relative = value & ~1
        positions = sorted(functions)
        assert entry_relative is not None
        bias = (on_load & ~1) - entry_relative
        context_names = set()
        contexts = []
        for address, size in list(case.platform.allocations.items())[:128]:
            candidates = []
            for candidate in (origins[address], *stacks[address]):
                relative = (candidate & ~1) - bias
                index = bisect_right(positions, relative) - 1
                if index >= 0:
                    start = positions[index]
                    length, name = functions[start]
                    if relative < start + length and name not in candidates:
                        candidates.append(name)
                        context_names.add(name)
            contexts.append({"address": hex(address), "size": size, "stack_symbol_candidates": candidates,
                             "object_type": vtables.get(case.word(address) - bias) if size >= 4 else None,
                             "phase": "constructors" if address in constructor_allocations else "JNI_OnLoad",
                             "words": list(struct.unpack("<" + "I" * (min(size, 32) // 4),
                                                        case.calls.machine.mem_read(address, min(size, 32) // 4 * 4)))})
            if contexts[-1]["object_type"]:
                context_names.add(contexts[-1]["object_type"])
        ordered_names = sorted(context_names)
        if ordered_names:
            demangled = subprocess.run([str(sdk / "bin/arm-vita-eabi-c++filt")], input="\n".join(ordered_names) + "\n",
                                       text=True, capture_output=True, check=True).stdout.splitlines()
            descriptions = dict(zip(ordered_names, demangled))
            for context in contexts:
                context["stack_symbol_candidates"] = [descriptions[name] for name in context["stack_symbol_candidates"]]
                if context["object_type"]:
                    context["object_type"] = descriptions[context["object_type"]]
        (output / "retained.json").write_text(json.dumps(contexts, indent=2) + "\n")
        report["retained_context_samples"] = len(contexts)
        report["sampled_object_types"] = dict(Counter(context["object_type"] for context in contexts if context["object_type"]))
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report), flush=True)
        case.clean()  # Whole-heap strict, including unsupported-profile/type fault cases.
    except Exception:
        print("ARM bootstrap stop:", {name: hex(case.calls.reg(name)) for name in ("PC", "LR", "SP", "R0", "R1")}, flush=True)
        raise


if __name__ == "__main__":
    main()
