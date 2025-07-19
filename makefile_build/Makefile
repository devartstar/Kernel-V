all:
	mkdir -p build
	nasm -f bin -o build/stage1.bin bootloader/stage1.asm
	nasm -f bin -o build/stage2.bin bootloader/stage2.asm
	dd if=build/stage1.bin of=boot.img bs=512 count=1 conv=notrunc
	dd if=build/stage2.bin of=boot.img bs=512 count=5 seek=1 conv=notrunc

	gcc -m32 -ffreestanding -c kernel/main/kernel.c -o build/kernel.o -fno-pie
	ld -m elf_i386 -o build/kernel.bin build/kernel.o -nostdlib --oformat=binary -Ttext=0x10000

	dd if=build/kernel.bin of=boot.img bs=512 count=5 seek=6 conv=notrunc
	dd if=/dev/zero of=boot.img bs=512 count=1 seek=11 conv=notrunc

clean:
	rm -rf build

run:
	qemu-system-i386 -hda boot.img -display curses

.PHONY: all clean run
