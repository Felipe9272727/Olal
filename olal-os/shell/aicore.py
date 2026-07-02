#!/usr/bin/env python3
"""Olal AI Core - a IA como PARTE do sistema (nao um chat).

Um servico residente do Olal OS que cuida do proprio sistema, sozinho:
  - MEMORIA: monitora a RAM; sob pressao, mata processos orfaos/duplicados
    e, em pressao critica, o app aberto que mais come memoria (nunca a UI,
    nunca o backend). Poda caches que crescem demais.
  - CPU: rebaixa a prioridade (renice) de quem esta roubando CPU da interface.
  - SAUDE: se o backend do Olal morrer, ressuscita. Limpa lixo do /tmp.
  - PRESTA CONTAS: cada acao vira uma linha no diario (/opt/olal/ai-journal.txt),
    que a interface mostra (app Sistema) - o usuario VE a IA trabalhando.

Leve de proposito: 1 passada a cada 20s lendo /proc (custo ~zero).
"""
import os, time, glob, json, subprocess, signal

JOURNAL = "/opt/olal/ai-journal.txt"
STATE   = "/opt/olal/ai-state.json"
LOOP_S  = 20
# nunca tocar nestes (a propria UI/OS). Comparado contra o cmdline inteiro.
PROTECT = ("aicore.py", "backend.py", "olal-shell", "olal-webkit", "firefox",
           "chromium", "openbox", "tint2", "dbus", "pulseaudio", "Xwayland",
           "proot", "bash", "sh -", "python3 -")
CACHE_DIRS = (os.path.expanduser("~/.cache/olal-shader"),
              os.path.expanduser("~/.cache/mesa_shader_cache"))
CACHE_MAX_MB = 300

def log(msg):
    line = time.strftime("%F %T") + "  " + msg
    try:
        lines = []
        if os.path.exists(JOURNAL):
            lines = open(JOURNAL, errors="ignore").read().splitlines()[-199:]
        lines.append(line)
        open(JOURNAL, "w").write("\n".join(lines) + "\n")
    except Exception:
        pass

def meminfo():
    d = {}
    try:
        for ln in open("/proc/meminfo"):
            k, v = ln.split(":", 1)
            d[k] = int(v.strip().split()[0])   # kB
    except Exception:
        return None
    return d

def procs():
    """[(pid, rss_kb, cmdline)] dos processos vivos."""
    out = []
    for p in glob.glob("/proc/[0-9]*"):
        try:
            pid = int(os.path.basename(p))
            cmd = open(p + "/cmdline", "rb").read().replace(b"\0", b" ").decode(errors="ignore").strip()
            if not cmd:
                continue
            rss = 0
            for ln in open(p + "/status"):
                if ln.startswith("VmRSS:"):
                    rss = int(ln.split()[1]); break
            out.append((pid, rss, cmd))
        except Exception:
            pass
    return out

def is_protected(cmd):
    c = cmd.lower()
    return any(k in c for k in PROTECT)

def kill_leftovers(ps):
    """firefox/chromium duplicados de sessoes que morreram mal: mata o mais
    ANTIGO se houver 2+ instancias PRINCIPAIS do mesmo navegador."""
    freed = 0
    for name in ("firefox-esr", "chromium"):
        mains = [(pid, rss) for pid, rss, cmd in ps
                 if name in cmd and "-contentproc" not in cmd and "--type=" not in cmd]
        if len(mains) >= 2:
            mains.sort()                       # PID menor = mais antigo
            for pid, rss in mains[:-1]:        # preserva o mais novo (o atual)
                try:
                    os.kill(pid, signal.SIGKILL); freed += rss
                    log(f"limpei {name} duplicado (pid {pid}, {rss//1024}MB) de sessao anterior")
                except Exception:
                    pass
    return freed

def trim_caches():
    freed = 0
    for d in CACHE_DIRS:
        try:
            files = sorted(glob.glob(d + "/**/*", recursive=True), key=os.path.getmtime)
            files = [f for f in files if os.path.isfile(f)]
            total = sum(os.path.getsize(f) for f in files)
            while total > CACHE_MAX_MB * 1048576 and files:
                f = files.pop(0)
                sz = os.path.getsize(f); os.remove(f)
                total -= sz; freed += sz
        except Exception:
            pass
    if freed:
        log(f"podei caches de shader: liberei {freed//1048576}MB")
    return freed

def clean_tmp():
    n = 0; now = time.time()
    for f in glob.glob("/tmp/olal-probe*.png") + glob.glob("/tmp/olal-hl*.png"):
        try:
            if now - os.path.getmtime(f) > 3600:
                os.remove(f); n += 1
        except Exception:
            pass
    return n

def kill_heaviest_app(ps):
    """pressao CRITICA: derruba o app (nao-essencial) que mais come RAM."""
    apps = [(rss, pid, cmd) for pid, rss, cmd in ps if not is_protected(cmd) and rss > 51200]
    if not apps:
        return False
    apps.sort(reverse=True)
    rss, pid, cmd = apps[0]
    name = cmd.split()[0].rsplit("/", 1)[-1]
    try:
        os.kill(pid, signal.SIGTERM); time.sleep(2)
        try: os.kill(pid, signal.SIGKILL)
        except ProcessLookupError: pass
        log(f"RAM critica: fechei '{name}' (pid {pid}) e liberei ~{rss//1024}MB - reabra quando quiser")
        return True
    except Exception:
        return False

def renice_hogs(ps):
    """quem nao e UI e esta pesado vai pra prioridade baixa (a interface manda)."""
    for pid, rss, cmd in ps:
        if is_protected(cmd) or rss < 102400:
            continue
        try:
            if os.getpriority(os.PRIO_PROCESS, pid) < 5:
                os.setpriority(os.PRIO_PROCESS, pid, 5)
                name = cmd.split()[0].rsplit("/", 1)[-1]
                log(f"dei prioridade de CPU a interface sobre '{name}' (renice +5)")
        except Exception:
            pass

def backend_alive():
    try:
        import urllib.request
        urllib.request.urlopen("http://127.0.0.1:8080/api/sysinfo", timeout=4)
        return True
    except Exception:
        return False

def revive_backend():
    try:
        subprocess.Popen(["bash", "-lc",
            "cd /opt/olal/shell && OLAL_PORT=8080 setsid python3 backend.py >/dev/null 2>&1 &"])
        log("backend do Olal caiu - RESSUSCITEI o backend (interface volta a responder)")
    except Exception:
        pass

def save_state(mem, extra):
    try:
        st = {"ts": time.strftime("%F %T"),
              "mem_total_mb": mem["MemTotal"] // 1024 if mem else 0,
              "mem_avail_mb": mem["MemAvailable"] // 1024 if mem else 0}
        st.update(extra)
        open(STATE, "w").write(json.dumps(st))
    except Exception:
        pass

def main():
    log("Olal AI Core ONLINE - cuidando de memoria, CPU e saude do sistema")
    last_trim = 0
    while True:
        try:
            mem = meminfo()
            ps = procs()
            acted = False
            if mem:
                avail = mem.get("MemAvailable", 0); total = mem.get("MemTotal", 1)
                pct = 100 * avail // total
                if pct < 18:                       # pressao: comeca pelo indolor
                    acted = bool(kill_leftovers(ps)) or acted
                    if time.time() - last_trim > 600:
                        acted = bool(trim_caches()) or acted; last_trim = time.time()
                if pct < 9:                        # critico: sacrifica o mais pesado
                    acted = kill_heaviest_app(ps) or acted
                renice_hogs(ps)
            if not backend_alive():
                revive_backend(); acted = True
            clean_tmp()
            save_state(mem, {"last_action": acted})
        except Exception:
            pass
        time.sleep(LOOP_S)

if __name__ == "__main__":
    main()
