/* PS-EXE 用リンカスクリプト。RAM 先頭 64KB はカーネル領域なので 0x80010000 から。 */
OUTPUT_FORMAT("elf32-tradlittlemips")
ENTRY(_start)
MEMORY { ram : ORIGIN = 0x80010000, LENGTH = 0x1F0000 }
SECTIONS {
  .text : { *(.text.start) *(.text*) } > ram
  .rodata : { *(.rodata*) } > ram
  .data : { *(.data*) } > ram
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; } > ram
}
