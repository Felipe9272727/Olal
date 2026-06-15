ASM    := nasm
CC     := gcc
LD     := ld
QEMU   := qemu-system-i386
BUILD  := build
CFLAGS := -m32 -ffreestanding -nostdlib -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -mno-sse -mno-mmx -mfpmath=387 -Wall -Wextra -O2 -c
CSRC := $(wildcard kernel/*.c)
COBJ := $(patsubst kernel/%.c,$(BUILD)/%.o,$(CSRC))
OBJS := $(BUILD)/entry.o $(COBJ)

.PHONY: all run clean
all: $(BUILD)/olal.img
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	$(ASM) -f bin $< -o $@
$(BUILD)/entry.o: kernel/entry.asm | $(BUILD)
	$(ASM) -f elf32 $< -o $@
$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
$(BUILD)/kernel.elf: $(OBJS) linker.ld
	$(LD) -m elf_i386 -T linker.ld -o $@ $(OBJS)
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@
$(BUILD)/olal.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $@
	truncate -s 4194304 $@
run: $(BUILD)/olal.img
	$(QEMU) -vga std -drive format=raw,file=$(BUILD)/olal.img,if=ide
clean:
	rm -rf $(BUILD)
