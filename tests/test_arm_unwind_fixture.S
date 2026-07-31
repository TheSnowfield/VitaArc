.syntax unified
.thumb
.text
.align 2
.global fixture_unwind_function
.type fixture_unwind_function, %function
.thumb_func
fixture_unwind_function:
.fnstart
  movs r0, #23
  bx lr
.cantunwind
.fnend
.size fixture_unwind_function, .-fixture_unwind_function

.data
.align 2
.global fixture_unwind_data
.type fixture_unwind_data, %object
fixture_unwind_data:
  .word 0x11223344
.size fixture_unwind_data, 4
