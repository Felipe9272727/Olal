# Olal OS — estado do sistema (v12.1)

> Documento de continuidade. Se você é uma IA (ou humano) mexendo neste repo,
> **leia isto antes de alterar o boot/render** — os itens abaixo foram
> encontrados por reprodução real e NÃO podem regredir.

## Arquitetura do boot (o que roda no celular)

```
Termux: ./olal
  ├─ git reset --hard origin/main  (auto-update)
  ├─ deploy por 3 vias (copia direta multi-caminho, /tmp compartilhado, self-heal por curl)
  ├─ pulseaudio (on-demand) + virgl (zink/Turnip se disponível, senão software)
  ├─ termux-x11 :0
  └─ proot Debian: openbox + tint2 → exec olal-shell
       olal-shell (dentro do Debian):
         ├─ self-update (interface + a si mesmo + olal-webkit, via curl)
         ├─ backend.py (espera responder ANTES de abrir navegador)
         ├─ LIMPA sobras de sessão + locks órfãos (fix da tela preta)
         └─ tiers, na ordem:
              0a) Olal nativo WebKit GPU   [só com GPU_ON=1 e detector]
              0b) Olal nativo WebKit sw    [HERMÉTICO: llvmpipe, sem virpipe]
              1)  Chromium GPU Turnip/zink [só com GPU_ON=1 e detector]
              2)  Firefox GPU WebRender    [só com GPU_ON=1 e detector]
              3)  Firefox software         [HERMÉTICO — caminho garantido]
              4)  Chromium swiftshader     [só com detector: é PRETO em arm64]
              5)  Firefox com perfil limpo [último recurso]
```

Cada tentativa fica registrada em `/opt/olal/boot-report.txt`
(o `olal-doctor`, seção 6, mostra). O detector de tela preta captura a raiz
do X e mede o brilho: >8 = renderizou; preto persistente >35s = falhou →
derruba e tenta o próximo tier.

## Bugs reais encontrados (reproduzidos, não teorizados) — NÃO REGREDIR

1. **Locks órfãos do perfil = a tela preta do usuário.** O menu Desligar usa
   `kill -9` no Firefox → `~/.olal-ff/lock` e `.parentlock` ficam apontando
   pra processo morto → no boot seguinte o Firefox (`--no-remote`) sai NA HORA
   → cascata desce até o swiftshader → preto em arm64.
   **Fix:** o olal-shell mata sobras e remove locks (Firefox + Chromium
   Singleton*) antes de abrir qualquer navegador. *Testado: cenário plantado,
   script antigo falha, script novo renderiza.*

2. **Tiers "software" herdavam `GALLIUM_DRIVER=virpipe` do boot.** Com
   `LIBGL_ALWAYS_SOFTWARE=1` isso contradiz e o WebKit **aborta** (SIGABRT)
   quando o virgl está fora; o Firefox roteava GL pelo virgl instável.
   **Fix:** tiers software são herméticos (`env -u MESA_LOADER_DRIVER_OVERRIDE`,
   `GALLIUM_DRIVER=llvmpipe`). *Testado: antes = Aborted; depois = renderiza.*

3. **Detector matava o Firefox cedo demais.** Firefox leva ~15–20s pra
   renderizar a frio; a janela era 12s → o detector o matava no meio do boot
   ("tela preta" falsa) → cascata. **Fix:** janela de 35s; qualquer brilho >8
   (até a tela branca de loading) conta como vivo.

4. **`gfx.webrender.all=true` (introduzido por outra IA) = tela preta.**
   Força WebRender por hardware num GL instável: pinta 1 frame e morre.
   **Fix:** `gfx.webrender.software=true` (CPU, confiável). GPU só nos tiers
   de GPU, protegidos pelo detector.

## Coisas que parecem otimização mas NÃO funcionam no proot (não readicionar)

- `systemctl disable/mask` (não há systemd) — no-op.
- `/etc/sysctl.d/*` (sem root) — no-op.
- `/etc/security/limits.conf` (sem PAM) — no-op. Use `ulimit` por shell.
- Polling permanente na UI (setInterval infinito) — mata o idle-0%; o polling
  de janelas é LAZY (só roda com app externo aberto).

## GPU (estilo Winlator/GameHub)

- Padrão: zink/Turnip via virgl acelerado (pacotes Termux `mesa-zink
  virglrenderer-mesa-zink vulkan-loader-android`).
- Opt-in: `olal-gpu-boost` instala o Mesa kgsl (lfdevs/mesa-for-android-container)
  em **/opt/olal/mesa-kgsl (isolado — nunca tocar o Mesa do sistema)**;
  Freedreno faz GL direto na Adreno. `olal-gpu-boost off` desfaz.
- Env aplicado nos tiers de GPU: `mesa_glthread=true`,
  `MESA_VK_WSI_PRESENT_MODE=mailbox`, `ZINK_DESCRIPTORS=lazy`, shader cache.

## Como diagnosticar problema de boot

```
bash ~/Olal/olal-os/olal-doctor
```
Seção 6 = relatório dos últimos boots (tier, sucesso/falha, brilho, tempo).

## Validação (2026-07-01, script real sob Xvfb, env idêntico ao device)

- 1º boot (sem WebKit): Firefox software renderiza (brilho 54). ✅
- Boot nativo: WebKit renderiza direto, sem fallback. ✅
- Cenário do bug (locks órfãos): limpa e renderiza. ✅
- Cascata: WebKit abortado → Firefox assume → renderiza (observado ao vivo). ✅
- Entrega: raw.githubusercontent serve byte-idêntico ao testado. ✅

Pendente (só executável no aparelho físico): confirmação do boot no
Adreno 710 do usuário — `cd ~/Olal && git fetch origin main && git reset
--hard origin/main && cd olal-os && ./olal`.
