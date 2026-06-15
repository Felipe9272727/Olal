# Olal OS

Um sistema operacional minimalista escrito **do zero em Assembly x86 (32 bits)**.
Ele inicializa em modo real (16 bits), troca para **modo protegido de 32 bits**
e roda um kernel próprio com drivers de vídeo, teclado e serial.

## Recursos

- **Bootloader** (`boot/boot.asm`) — setor de boot de 512 bytes:
  - configura pilha e segmentos;
  - carrega o kernel do disco (BIOS `int 0x13`, com conversão LBA→CHS);
  - habilita a linha A20;
  - monta a **GDT** e entra em modo protegido;
  - salta para o kernel.
- **Kernel** (`kernel/kernel.asm`) — 32 bits, modo protegido:
  - driver de **vídeo VGA** modo texto 80x25 (cores, scroll, backspace,
    cursor de hardware);
  - driver de **teclado PS/2** por polling (mapa de scancodes US);
  - driver **serial COM1** (usado para verificação automática);
  - **mini-shell** interativo que ecoa o que você digita.

## Arquitetura

```
BIOS  ->  boot.asm (16-bit, 0x7C00)  ->  modo protegido  ->  kernel.asm (32-bit, 0x1000)
```

A imagem final (`build/olal.img`) é um disquete de 1.44MB:
setor 0 = bootloader, setores seguintes = kernel.

## Pré-requisitos

- `nasm`
- `qemu-system-i386`
- `make`

```sh
sudo apt-get install nasm qemu-system-x86 make
```

## Compilar e rodar

```sh
make            # gera build/olal.img
make run        # roda no QEMU com janela gráfica
make run-serial # roda sem gráfico, espelhando a serial no terminal
make test       # boota e verifica que o kernel entrou em modo protegido
make clean      # limpa os artefatos
```

## Como funciona o boot

1. A BIOS carrega o setor 0 (512 bytes) em `0x7C00` e o executa em modo real.
2. O bootloader lê os setores do kernel para `0x1000`.
3. Habilita A20, carrega a GDT e liga o bit `PE` de `CR0`.
4. Um *far jump* recarrega `CS` e entra em modo protegido de 32 bits.
5. O kernel assume, inicializa os drivers e abre o shell `Olal>`.
