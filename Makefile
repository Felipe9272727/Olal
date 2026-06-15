# ============================================================================
#  Olal OS - Makefile
# ============================================================================
ASM   := nasm
QEMU  := qemu-system-i386
BUILD := build

BOOT_BIN   := $(BUILD)/boot.bin
KERNEL_BIN := $(BUILD)/kernel.bin
OS_IMAGE   := $(BUILD)/olal.img

.PHONY: all run run-serial test clean

all: $(OS_IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

# Setor de boot (512 bytes)
$(BOOT_BIN): boot/boot.asm | $(BUILD)
	$(ASM) -f bin $< -o $@

# Kernel (binario flat)
$(KERNEL_BIN): kernel/kernel.asm | $(BUILD)
	$(ASM) -f bin $< -o $@

# Imagem de disquete: boot + kernel, preenchida ate 1.44MB
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $@
	truncate -s 1474560 $@

# Roda com janela grafica
run: $(OS_IMAGE)
	$(QEMU) -fda $(OS_IMAGE)

# Roda sem grafico, espelhando a serial no terminal
run-serial: $(OS_IMAGE)
	$(QEMU) -fda $(OS_IMAGE) -display none -serial stdio

# Verificacao automatica: boota e confere a marca na porta serial
test: $(OS_IMAGE)
	@echo ">> Testando boot do Olal OS..."
	@out=$$(timeout 5 $(QEMU) -fda $(OS_IMAGE) -display none -serial stdio < /dev/null 2>/dev/null); \
	echo "$$out"; \
	echo "$$out" | grep -q "kernel online" \
	  && echo ">> OK: kernel entrou em modo protegido 32-bit." \
	  || { echo ">> FALHA: marca da serial nao encontrada."; exit 1; }

clean:
	rm -rf $(BUILD)
