# PSV hardware startup reports, 2026-10-24

These reports are actual PSV hardware failures, separate from the completed
Vita3K-only offline play tests. Do not label hardware compatibility verified.

## Before enabling kubridge

User screenshot: prefetch abort, EPC/PC/BadVAddr zero, LR 0x81088cbd.
The user then explicitly noticed kubridge's kernel module was not enabled.
They enabled it and reported a different subsequent failure. Do not treat
enabling kubridge as proof that all startup failures are fixed. The first LR
cannot be safely symbolized without the executable's actual load base/build.

## After enabling kubridge: 2.0 immediate startup

User confirms version 2.0 and crash immediately on launch. Screenshot attachment:
`C:/Users/CirnoBaka/.codex/attachments/17c422b8-5cf1-42b6-aeb1-274067ed995f/codex-clipboard-d935161a-ad80-46c0-8d17-9f70414ce41f.png`.

- Data abort, EPC/PC 0x9870c278, BadVAddr 0x140, LR 0x9853e721.
- r0 0x819a3510, r1 zero. Game library maps at 0x98000000.
- Exact 2.0 ELF offset 0x70c278 is cocos2d::Node::addChild(Node*), instruction
  `ldr.w r2, [r1, #0x140]`. The child pointer is null.
- Caller at 0x53e71e is StartScene::init()+0x41e, `blx r2`; returns to 0x53e720.
- The preceding Sprite::create call at 0x53e6f6 receives the 24-byte asset name
  `startup/start_wreath.png`, and stores the result in StartScene+0x23c before
  invoking addChild. Thus this sprite creation failed. The next code would
  dereference it again; suppressing addChild alone would not fix the failure.
- Asset resolved by literal at 0x53ed08 plus Thumb PC at 0x53e6ea -> 0x965ff6.

The local 2.0 APK and final r3 VPK contain identical valid PNG bytes:
137677 bytes, 745x576 RGBA, SHA256
45754e36c93751dd968dbe1b21a430558c91519e32d7200b1737266b39610263.
This verifies the packaged input, not the user's installed file or the game
decoder/texture allocation on real hardware.

Root failure remains unresolved: resource read/decode/texture creation and
memory/compiler failures can all lead to a null Sprite. Request the actual
`ux0:vitaarc/boot.log` (and optionally .psp2dmp) before claiming a specific cause.
An unanswered optional question asks whether libshacccg.suprx is installed.
Current main attempts runtime compilation first, enabling the packaged offline
shader mode only if both compiler-init paths fail. Vita3K tests lacked the runtime
compiler and used the 39 packaged offline binaries. Hardware may take the other
path; this is a hypothesis to distinguish with logs, not an established cause.
Do not bypass setup, replace missing graphics with success stubs, or silently
patch game null checks to claim a fix. The first screenshot alone did not
establish a specific reason to change the game code.

## After adding shacccg: heap corruption, diagnostic r4

The user subsequently added shacccg and supplied a different hardware data
abort: EPC `VitaArc + 0x0015615e`, PC 0x811b915e, BadVAddr 0x1a881370.
For the original released ELF, offset 0x15615e maps to `_free_r+0x5e`, at
`str r6, [r4, #12]`. Screenshot r4=0x1a881364 matches that invalid write.
This is corrupted allocator free-list state during free, not proof that the
runtime compiler itself caused the earlier corruption. LR is zero in this
allocator instruction sequence and does not supply its caller.

The exact original ELF, VELF and SELF (878234348f410c8ebfdb62036b775a99f1f9cfa22662d12938a23dbda760c5ff)
are preserved at `build/psv-2.0-r4/baseline/`. Use that ELF for the reported
offset, not a subsequently rebuilt ELF with shifted function addresses.

Diagnostic r4 now prefers a valid offline shader pack before trying shark_init,
even when a runtime compiler is installed. Without a valid pack it retains the
two runtime compiler paths. Main, touch and lifecycle fixtures use this same
selection order. This removes a hardware-specific compiler path from the
packaged configuration; it is a controlled test, not a proven heap fix.
Startup logs also identify the selected path and record used/free/arena heap
sizes before library loading, after constructors, and before/after JNI.

SELF SHA256: 7a2ae2841a4ad5c5be345eea419bd82a5d89984e7a75f570709e5d5144ac3674.
`build/psv-2.0-r4/VitaArc-2.0.0c-PSV-r4.vpk` differs from the final r3 2.0
package only in eboot.bin; all other entries are byte-identical and CRC-checked.
The companion eboot ZIP is for replacing a backed-up eboot in an existing 2.0
installation that already has the offline pack. No game assets, null checks or
save data are changed. Instructions and package-verification.json are alongside.

40 linked ARM startup/controller/cleanup cases pass, including no runtime init
when both pack and compiler are available. ARM shader cache coverage also passes
online/offline dispatch, malformed/missing/link failure and handle cleanup.
The actual r4 eboot runs in Vita3K for 45 seconds and restores original data
after bounded observation: qa/run-2.0.0c-20261024-163600. Actual PSV validation
of r4 and the hardware boot.log remain pending.
