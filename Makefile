ASM    := nasm
CC     := gcc
LD     := ld
QEMU   := qemu-system-i386
BUILD  := build
CFLAGS := -m32 -ffreestanding -nostdlib -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -mno-sse -mno-mmx -mfpmath=387 -Wall -Wextra -O2 -c
CSRC := $(wildcard kernel/*.c)
COBJ := $(patsubst kernel/%.c,$(BUILD)/%.o,$(CSRC))
OBJS := $(BUILD)/entry.o $(BUILD)/isr.o $(COBJ)

.PHONY: all run web serve clean
all: $(BUILD)/olal.img
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	$(ASM) -f bin $< -o $@
$(BUILD)/entry.o: kernel/entry.asm | $(BUILD)
	$(ASM) -f elf32 $< -o $@
$(BUILD)/isr.o: kernel/isr.asm | $(BUILD)
	$(ASM) -f elf32 $< -o $@
$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
$(BUILD)/kernel.elf: $(OBJS) linker.ld
	$(LD) -m elf_i386 -T linker.ld -o $@ $(OBJS)
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@
$(BUILD)/olal.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $@
	truncate -s 1474560 $@
run: $(BUILD)/olal.img
	$(QEMU) -vga std -fda $(BUILD)/olal.img
web: $(BUILD)/olal.img
	cp $(BUILD)/olal.img olal.img
serve: web
	python3 -m http.server 8000
clean:
	rm -rf $(BUILD)
