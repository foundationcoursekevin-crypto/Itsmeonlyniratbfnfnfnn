/* PixelBeeOS boot stub - Multiboot 1; GRUB selects the panel framebuffer mode. */
.set MAGIC,  0x1BADB002
.set FLAGS,  (1<<0)|(1<<1)|(1<<2)
.set CHECK,  -(MAGIC+FLAGS)

.section .multiboot
.align 4
.long MAGIC, FLAGS, CHECK
.long 0, 0, 0, 0, 0        /* address fields unused (ELF) */
.long 0, 0, 0, 32          /* linear framebuffer, 32 bpp preferred, size left to GRUB (grub.cfg gfxmode list / boot-menu entries); kmain also accepts 24 and 16 bpp */

.section .bss
.align 16
stack_bottom: .skip 65536
stack_top:

.section .text
.global _start
_start:
    cli
    cld
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kmain
1:  hlt
    jmp 1b

.section .note.GNU-stack,"",@progbits
