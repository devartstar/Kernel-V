all:
	mkdir -p build
	nasm -f bin -o build/stage1.bin bootloader/stage1.asm
	nasm -f bin -o build/stage2.bin bootloader/stage2.asm
	dd if=build/stage1.bin of=boot.img bs=512 count=1 conv=notrunc
	dd if=build/stage2.bin of=boot.img bs=512 count=5 seek=1 conv=notrunc

	# Compile kernel entry point
	nasm -f elf32 kernel/arch/x86/kernel_entry.asm -o build/kernel_entry.o
	
	# Compile kernel source files
	gcc -m32 -ffreestanding -Ikernel/include -c kernel/main/kernel.c -o build/kernel.o -fno-pie
	gcc -m32 -ffreestanding -Ikernel/include -c kernel/lib/printk.c -o build/printk.o -fno-pie  
	gcc -m32 -ffreestanding -Ikernel/include -c kernel/drivers/vga/vga.c -o build/vga.o -fno-pie
	
	# Link kernel
	ld -m elf_i386 -o build/kernel.bin build/kernel_entry.o build/kernel.o build/printk.o build/vga.o -nostdlib --oformat=binary -Tkernel/linker/kernel.ld

	dd if=build/kernel.bin of=boot.img bs=512 count=5 seek=6 conv=notrunc
	dd if=/dev/zero of=boot.img bs=512 count=1 seek=11 conv=notrunc

clean:
	rm -rf build

run:
	qemu-system-i386 -hda boot.img -display curses

.PHONY: all clean run
