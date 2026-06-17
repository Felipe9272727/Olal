# Olal OS — Linux real no seu celular (com a nossa cara)

O Olal OS agora roda como um **Linux de verdade (Debian)** por cima do seu
Android — **sem root e sem apagar nada**. Em cima do Linux, ele abre **direto
na interface do Olal** (a mesma cara, com todos os apps e touch), e os apps de
**Navegador** e **YouTube** usam o **Chromium real** do celular, então o
**YouTube toca de verdade**.

## ⚡ Quer ver a interface AGORA (sem instalar nada)?
Abra no Chrome do seu celular:

**https://felipe9272727.github.io/Olal/olal-os/shell/**

É a mesma interface do Olal (home, apps, touch, YouTube real, Olal AI com a
rede neural). Bom para testar a cara antes de fazer a instalação completa do
Linux abaixo. *(A instalação no Termux é o "OS de verdade", rodando isso em
cima do Debian em tela cheia.)*

## O que você precisa instalar antes (2 apps, pelo F-Droid)
1. **Termux** — https://f-droid.org/packages/com.termux/
2. **Termux:X11** — https://f-droid.org/packages/com.termux.x11/

> Use os do **F-Droid**, não os da Play Store (a versão da Play Store é antiga).

## Passo a passo (no Termux)
Abra o **Termux** e cole isto:

```bash
pkg install -y git
git clone https://github.com/Felipe9272727/Olal
cd Olal/olal-os
bash install.sh
```

A instalação baixa o Debian + Chromium (pode demorar uns minutos e ~1 GB).

## Para ligar o Olal OS
Ainda no Termux:

```bash
./olal
```

Ele tenta **abrir o app Termux:X11 sozinho**. Se não abrir, abra o app
**Termux:X11** na mão — o Olal aparece em tela cheia, com a home, os apps e o
touch. 🎉

> **Importante:** o **app Termux:X11** (APK) precisa ser da **mesma versão** do
> pacote `termux-x11-nightly`. Se a tela ficar preta, é quase sempre versão
> diferente — reinstale os dois do F-Droid.

## O que funciona
- **Olal AI (o coração da OS)** — um assistente acessível de qualquer tela
  (barra na home + botão 🤖 flutuante). Ele **conversa** (LLM de verdade) e
  **controla o sistema**: "toca lofi no youtube", "abre a calculadora",
  "quanto é 17×23", "que horas são", "abre wikipedia.org". Os comandos
  funcionam **mesmo sem internet**.
- **YouTube** — busca real (Piped) e toca vídeo de verdade (Chromium real).
- **Navegador** — abre sites de verdade; busca abre em modo leitura.
- **Terminal, Calc, Notas, Relógio, Config, Arquivos, Paint, OLA-32, Sistema,
  Rede, Olal JS** — todos os apps da nossa OS, com o mesmo design e touch.
- **OLA-32** — a nossa CPU própria roda no app **e** no terminal (comando `ola32`).

## Para desligar
No Termux: `Ctrl+C` e depois `exit`. Para reabrir, é só `./olal` de novo.

## Observação honesta sobre desempenho
O vídeo do YouTube roda na CPU real do celular (sem aceleração de GPU dentro
do Linux), então em aparelhos mais fracos pode engasgar um pouco. Mas é o
**YouTube de verdade**, não emulado — diferente da versão antiga do Olal que
rodava dentro de um emulador no navegador.
