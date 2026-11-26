CC      = gcc
ISO_SIZE_BYTES = 1073741824
CFLAGS  = -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-sse -mno-mmx \
          -fno-tree-loop-distribute-patterns -O2 -Wall -Wextra -Wno-misleading-indentation
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib --build-id=none

all: kernel.elf

# PixelBeeOS 2.13 - Core OS Edition, native-resolution boot

boot.o: boot.S
	$(CC) $(CFLAGS) -c $< -o $@
kernel.o: kernel.c font.h acpi.h display.h cmds.h apps.h apps2.h ux.h fat32.h ata.h disk.h net.h netdrv.h netcmd.h netapp.h mines.h crypto.h tls.h nettls.h
	$(CC) $(CFLAGS) -c kernel.c -o $@
kernel.elf: boot.o kernel.o linker.ld
	ld $(LDFLAGS) boot.o kernel.o -o $@

iso: kernel.elf
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/kernel.elf
	cp grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o pixelbeeos.iso iso

run: iso
	qemu-system-i386 -cdrom pixelbeeos.iso -m 256 -vga std -display sdl -nic user,model=e1000

clean:
	rm -rf *.o kernel.elf iso pixelbeeos.iso

# Build a bootable ISO and pad its logical file size to exactly 1 GiB.
# Requires grub-mkrescue (and its xorriso dependencies).
iso-1gb: iso
	@size=$$(stat -c%s pixelbeeos.iso); \
	if [ "$$size" -gt "$(ISO_SIZE_BYTES)" ]; then \
	  echo "ISO is $$size bytes, larger than the 1 GiB target"; exit 1; \
	fi
	truncate -s $(ISO_SIZE_BYTES) pixelbeeos.iso
	@test "$$(stat -c%s pixelbeeos.iso)" -eq "$(ISO_SIZE_BYTES)"
	@echo "Created pixelbeeos.iso: exactly $(ISO_SIZE_BYTES) bytes (1 GiB)"

iso-100mb: iso
	@size=$$(stat -c%s pixelbeeos.iso); target=104857600; \
	if [ "$$size" -gt "$$target" ]; then echo "ISO is $$size bytes, larger than 100 MiB"; exit 1; fi; \
	truncate -s $$target pixelbeeos.iso; \
	echo "Created pixelbeeos.iso: exactly $$target bytes (100 MiB, the extra space is empty padding)"

.PHONY: all iso iso-1gb iso-100mb run clean host-test

# Run lightweight host-side desktop interaction checks (not a hardware boot test).
host-test: kernel_check.c kernel.c acpi.h display.h font.h cmds.h apps.h apps2.h ux.h fat32.h ata.h disk.h net.h netdrv.h netcmd.h netapp.h mines.h crypto.h tls.h nettls.h
	gcc -O2 -DHOSTTEST -I. kernel_check.c -o kernel_check
	./kernel_check

# Render the real C desktop UI to PPM at a chosen pixel resolution.
host-preview: kernel_preview.c kernel.c acpi.h display.h font.h cmds.h apps.h apps2.h ux.h fat32.h ata.h disk.h net.h netdrv.h netcmd.h netapp.h mines.h crypto.h tls.h nettls.h
	gcc -O2 -I. kernel_preview.c -o kernel_preview

# FAT32 + ATA tests: file-backed block device, software IDE controller model, then an independent Python checker.
test-storage: tests/fat32_test.c tests/ata_emu_test.c tests/verify_fat32.py fat32.h ata.h
	gcc -O1 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation tests/fat32_test.c -o fat32_test
	./fat32_test /tmp/pixelbee-fat.img && python3 tests/verify_fat32.py /tmp/pixelbee-fat.img
	gcc -O1 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation tests/ata_emu_test.c -o ata_emu_test
	./ata_emu_test && python3 tests/verify_fat32.py /tmp/ata.img --structure

# Network stack tests: simulated internet, then the same run through register-level models of the e1000 and RTL8139 drivers.
test-network: tests/net_test.c net.h netdrv.h
	gcc -O1 -g -no-pie -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation tests/net_test.c -o net_test && ./net_test
	gcc -O1 -g -no-pie -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -DNIC_MODEL=1 -DNIC_KIND=0 tests/net_test.c -o net_test_e1000 && ./net_test_e1000
	gcc -O1 -g -no-pie -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -DNIC_MODEL=1 -DNIC_KIND=1 tests/net_test.c -o net_test_rtl && ./net_test_rtl

# TLS 1.3: primitives vs RFC vectors + Python `cryptography`, protocol vs real OpenSSL, and the whole stack (TCP+TLS+HTTP) through the e1000 driver model. Needs python3, openssl, python3-cryptography.
test-tls: tests/crypto_cli.c tests/tls_client.c tests/net_test.c crypto.h tls.h net.h nettls.h
	gcc -O2 -Wall -Wno-unused-function -Wno-misleading-indentation tests/crypto_cli.c -o crypto_cli && python3 tests/crypto_test.py ./crypto_cli
	gcc -O2 -Wno-unused-function -Wno-misleading-indentation tests/tls_client.c -o tls_client && python3 tests/tls_interop.py ./tls_client
	gcc -O1 -g -no-pie -fsanitize=address,undefined -Wno-unused-function -Wno-misleading-indentation -DTLS_BRIDGE=1 tests/net_test.c -o net_test_tls && python3 tests/tls_bridge.py ./net_test_tls
	gcc -O1 -g -no-pie -fsanitize=address,undefined -Wno-unused-function -Wno-misleading-indentation -DTLS_BRIDGE=1 -DNIC_MODEL=1 -DNIC_KIND=0 tests/net_test.c -o net_test_tls_e1000 && python3 tests/tls_bridge.py ./net_test_tls_e1000

.PHONY: host-preview test-storage test-network test-tls

# ACPI shutdown/reboot table parser against synthetic RSDP/FADT/DSDT tables.
test-acpi: tests/acpi_test.c acpi.h
	gcc -O1 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function tests/acpi_test.c -o acpi_test && ./acpi_test

# Display-format conversion (16/24/32 bpp, RGB/BGR) round-trip test.
test-display: tests/display_test.c display.h
	gcc -O1 -g -fsanitize=address,undefined -Wall -Wextra tests/display_test.c -o display_test && ./display_test

.PHONY: test-acpi test-display

# Minesweeper rules test.
test-mines: tests/mines_test.c mines.h
	gcc -O1 -g -fsanitize=address,undefined -Wno-unused-function -Wno-misleading-indentation -I. tests/mines_test.c -o mines_test && ./mines_test

.PHONY: test-mines
