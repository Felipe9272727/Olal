# Olal OS

Um sistema operacional minimalista escrito **do zero em Assembly x86 (32 bits)**,
com **interface gráfica estilo Android**: tela inicial com ícones, barra de
status, suporte a **toque/mouse** e **teclado virtual**.

Ele inicializa em modo real (16 bits), troca para **modo protegido de 32 bits**,
entra em **modo gráfico VGA 320x200** e roda um kernel próprio com drivers de
vídeo, mouse, RTC e serial.

## Recursos

- **Bootloader** (`boot/boot.asm`) — setor de boot de 512 bytes:
  - configura pilha e segmentos;
  - carrega o kernel do disco (BIOS `int 0x13`, com conversão LBA→CHS);
  - coloca a placa de vídeo em **modo gráfico 13h** (320x200, 256 cores);
  - habilita a linha A20, monta a **GDT** e entra em modo protegido;
  - salta para o kernel.
- **Kernel** (`kernel/kernel.asm`) — 32 bits, modo protegido:
  - **double buffering** (desenha em `0x30000` e copia para a tela);
  - **fonte 8x8 própria** (`kernel/font8x8.inc`) para desenhar texto;
  - **paleta personalizada** (cores estilo material design);
  - driver de **mouse PS/2** — no celular o **toque vira clique**;
  - **teclado virtual** QWERTY na tela;
  - leitura do **RTC** (relógio de tempo real, via CMOS);
  - driver **serial COM1** (usado para verificação automática).
- **Tela inicial** com barra de status (relógio) e grade de apps:
  - **Terminal/Notas** — área de texto + teclado virtual;
  - **Calculadora** — teclado numérico com `+ - * = C`;
  - **Relógio** — mostra a hora do RTC;
  - **Sobre** — informações do sistema;
  - **Reboot** — reinicia a máquina (via controlador 8042).

## Arquitetura

```
BIOS -> boot.asm (16-bit, 0x7C00) -> modo 13h -> modo protegido -> kernel.asm (32-bit, 0x1000)
```

A imagem final (`build/olal.img`) é um disquete de 1.44MB:
setor 0 = bootloader, setores seguintes = kernel.

## Pré-requisitos

- `nasm`
- `qemu-system-i386`
- `make`
- `python3` + `pillow` (apenas para regenerar a fonte, opcional)

```sh
sudo apt-get install nasm qemu-system-x86 make
```

## Compilar e rodar

```sh
make            # gera build/olal.img
make run        # roda no QEMU com janela gráfica (use o mouse!)
make run-serial # roda sem gráfico, espelhando a serial no terminal
make test       # boota e verifica que o kernel entrou em modo protegido
make clean      # limpa os artefatos
```

## Rodar no celular (Android)

A forma mais fácil de testar no Android é pelo emulador **v86** no navegador:

1. Pegue o arquivo `build/olal.img`.
2. Abra `https://copy.sh/v86` no Chrome.
3. Em **Custom boot**, escolha **Floppy disk image** e envie o `olal.img`.
4. Toque em **Start** — a tela inicial aparece e o **toque** funciona como clique.

Também dá para rodar no **Termux** (`pkg install nasm qemu-system-x86-64 make`).

## Regenerar a fonte (opcional)

A fonte 8x8 é gerada de uma TTF monoespaçada:

```sh
python3 tools/genfont.py   # regrava kernel/font8x8.inc
```

## Como funciona o boot

1. A BIOS carrega o setor 0 (512 bytes) em `0x7C00` e o executa em modo real.
2. O bootloader lê os setores do kernel para `0x1000` e entra no modo 13h.
3. Habilita A20, carrega a GDT e liga o bit `PE` de `CR0`.
4. Um *far jump* recarrega `CS` e entra em modo protegido de 32 bits.
5. O kernel assume, inicializa os drivers e desenha a tela inicial.
