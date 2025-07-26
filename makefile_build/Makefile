all: boot.img

boot.img: build/stage1.bin build/stage2.bin build/kernel.bin
	dd if=build/stage1.bin of=boot.img bs=512 count=1 conv=notrunc
	dd if=build/stage2.bin of=boot.img bs=512 count=5 seek=1 conv=notrunc
	dd if=build/kernel.bin of=boot.img bs=512 count=5 seek=6 conv=notrunc
	dd if=/dev/zero of=boot.img bs=512 count=1 seek=11 conv=notrunc

build/stage1.bin: bootloader/stage1.asm
	mkdir -p build
	nasm -f bin -o build/stage1.bin bootloader/stage1.asm

build/stage2.bin: bootloader/stage2.asm
	mkdir -p build
	nasm -f bin -o build/stage2.bin bootloader/stage2.asm

build/kernel.o: kernel/main/kernel.c
	mkdir -p build
	gcc -m32 -ffreestanding -Ikernel/include -c kernel/main/kernel.c -o build/kernel.o -fno-pie

build/printk.o: kernel/lib/printk.c
	mkdir -p build
	gcc -m32 -ffreestanding -Ikernel/include -c kernel/lib/printk.c -o build/printk.o -fno-pie

build/vga.o: kernel/drivers/vga/vga.c
	mkdir -p build
	gcc -m32 -ffreestanding -Ikernel/include -c kernel/drivers/vga/vga.c -o build/vga.o -fno-pie

build/kernel_entry.o: kernel/arch/x86/kernel_entry.asm
	mkdir -p build
	nasm -f elf32 -o build/kernel_entry.o kernel/arch/x86/kernel_entry.asm

build/kernel.bin: build/kernel_entry.o build/kernel.o build/printk.o build/vga.o kernel/linker/kernel.ld
	ld -m elf_i386 -T kernel/linker/kernel.ld -o build/kernel.elf build/kernel_entry.o build/kernel.o build/printk.o build/vga.o -nostdlib
	objcopy -O binary build/kernel.elf build/kernel.bin

clean:
	rm -rf build boot.img

run: boot.img
	qemu-system-i386 -hda boot.img -display curses

.PHONY: all clean run

