/* SPDX-License-Identifier: BSD-2-Clause-Patent */
ENTRY(_Head)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.text.head)) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  .scratch : {
    . = ALIGN(16);
    _ScratchStackBase = .;
    BYTE(0);
    . = _ScratchStackBase + 0x4000;
    _ScratchStackTop = .;
    . = ALIGN(4096);
    _Payload = .;
  }
  _ImageSize = _Payload + 0x00800000;
  ASSERT(_Payload <= 0x10000, "cas shim exceeds 64 KiB bound")
  ASSERT(SIZEOF(.bss) == 0, "cas validator must not use zero-initialized globals")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
