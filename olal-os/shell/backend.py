#!/usr/bin/env python3
"""Olal OS - backend do sistema.
Serve a interface E expõe o sistema real para ela (terminal, arquivos,
informações, controle). É o que faz o Olal ser um OS, não um launcher:
os apps falam com o Debian de verdade por baixo.

INTEGRAÇÃO DE APPS (novo): apps baixados abrem como "telas" dentro do
Olal — maximizados, sem decoração — e a barra do topo (tint2) tem um
botão "Olal" pra voltar à interface. As rotas /api/windows, /api/focus
e /api/launch_integrated fazem essa ponte WM ↔ interface.
"""
import http.server, socketserver, subprocess, json, os, pwd, shutil, urllib.parse, sys
import urllib.request, time, re, threading

ROOT = os.path.dirname(os.path.abspath(__file__))
PORT = int(os.environ.get("OLAL_PORT", "8080"))
HOME = os.path.expanduser("~")
DISPLAY = os.environ.get("DISPLAY", ":0")

def run(cmd, cwd=None, timeout=30):
    try:
        p = subprocess.run(cmd, shell=True, cwd=cwd or HOME, capture_output=True,
                           text=True, timeout=timeout, executable="/bin/bash")
        return {"out": p.stdout + p.stderr, "code": p.returncode}
    except subprocess.TimeoutExpired:
        return {"out": "(tempo esgotado)\n", "code": 124}
    except Exception as e:
        return {"out": f"erro: {e}\n", "code": 1}

def sysinfo():
    def g(c):
        try: return subprocess.check_output(c, shell=True, text=True, timeout=5).strip()
        except: return "?"
    return {
        "os":      g("cat /etc/olal-release 2>/dev/null || echo 'Olal OS'"),
        "kernel":  g("uname -r"),
        "arch":    g("uname -m"),
        "host":    g("hostname"),
        "user":    pwd.getpwuid(os.getuid()).pw_name,
        "uptime":  g("uptime -p 2>/dev/null || uptime"),
        "cpu":     g("nproc") + " nucleos  (" + g("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 2>/dev/null || echo ARM").strip() + ")",
        "mem":     g("free -h | awk '/Mem:/{print $3\" / \"$2}'"),
        "disk":    g("df -h \"$HOME\" | awk 'NR==2{print $3\" / \"$2\" (\"$5\")\"}'"),
        "gpu":     g("(MESA_LOADER_DRIVER_OVERRIDE=zink glxinfo 2>/dev/null || glxinfo 2>/dev/null) | grep -i 'OpenGL renderer' | cut -d: -f2 | head -1 || echo software"),
        "distro":  g("cat /etc/debian_version 2>/dev/null | sed 's/^/Debian /' || echo base"),
    }

def listapps():
    """lista os programas GUI instalados no Olal OS (.desktop)."""
    import glob
    out, seen = [], set()
    paths = sorted(glob.glob("/usr/share/applications/*.desktop") + glob.glob(os.path.expanduser("~/.local/share/applications/*.desktop")))
    for f in paths:
        try:
            txt = open(f, errors="ignore").read()
            if "NoDisplay=true" in txt: continue
            nm = re.search(r"(?m)^Name=(.+)$", txt); ex = re.search(r"(?m)^Exec=(.+)$", txt)
            ic = re.search(r"(?m)^Icon=(.+)$", txt)
            if not nm or not ex: continue
            name = nm.group(1).strip(); exe = re.sub(r"%[fFuUdDnN]", "", ex.group(1)).strip()
            key = exe.split()[0] if exe else name
            if key in seen or "olal" in f.lower(): continue
            seen.add(key)
            out.append({"name": name, "exec": exe, "icon": (ic.group(1).strip() if ic else key[0].upper()),
                        "bin": key})
        except: pass
    return {"apps": out}

# --------------------------------------------------------------------
#  INTEGRAÇÃO DE APPS (janelas X11 maximizadas, sem decoração, gerenciadas
#  pela barra do Olal). Tudo via wmctrl/xdotool - depende do setup-desktop.sh
#  ter instalado esses pacotes.
# --------------------------------------------------------------------
def windows():
    """Lista janelas X11 abertas (id, título, classe)."""
    try:
        out = subprocess.check_output(
            ["wmctrl", "-lx"], stderr=subprocess.DEVNULL, text=True, timeout=3
        ).strip().splitlines()
        wins = []
        for line in out:
            parts = line.split(None, 4)
            if len(parts) < 5: continue
            wid, host, cls = parts[0], parts[1], parts[2]
            title = parts[4].strip()
            # ignora a propria barra do Olal (tint2) e dockapps
            if cls.lower().startswith("tint2"): continue
            wins.append({"id": wid, "title": title, "class": cls,
                         "is_olal": "firefox" in cls.lower() or "chromium" in cls.lower()})
        return {"windows": wins}
    except Exception as e:
        return {"windows": [], "error": str(e)}

def focus_window(win_id_or_title):
    """Traz uma janela para frente. Aceita ID hex (0x...) ou substring do título."""
    if not shutil.which("wmctrl"):
        return {"ok": False, "error": "wmctrl nao instalado"}
    try:
        if win_id_or_title.startswith("0x"):
            subprocess.run(["wmctrl", "-ia", win_id_or_title], check=False, timeout=3)
        else:
            subprocess.run(["wmctrl", "-a", win_id_or_title], check=False, timeout=3)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def close_window(win_id_or_title):
    """Fecha uma janela (X)."""
    if not shutil.which("wmctrl"):
        return {"ok": False, "error": "wmctrl nao instalado"}
    try:
        if win_id_or_title.startswith("0x"):
            subprocess.run(["wmctrl", "-ic", win_id_or_title], check=False, timeout=3)
        else:
            subprocess.run(["wmctrl", "-c", win_id_or_title], check=False, timeout=3)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def launch_integrated(exe):
    """Lança um app e o integra como "tela" dentro do Olal:
       1) dispara o app em background;
       2) espera a janela X11 aparecer (até 8s);
       3) maximiza + remove decoração + traz pra frente;
       4) retorna o ID da janela p/ a interface gerenciar.
       Assim apps baixados viram "telas" do Olal, não janelas aleatórias."""
    if not shutil.which("wmctrl"):
        # sem wmctrl, fallback pro launch simples (UX antiga)
        return launch(exe)
    try:
        # janelas ANTES do lançamento (pra detectar a nova)
        before = {w["id"] for w in windows().get("windows", [])}
        env = dict(os.environ, DISPLAY=DISPLAY)
        # dispara o app (bash -lc pra herdar PATH/profile)
        subprocess.Popen(["bash", "-lc", exe + " >/dev/null 2>&1 &"], env=env)
        # espera a janela nova aparecer (até 8s)
        new_id = None
        for _ in range(16):
            time.sleep(0.5)
            after = windows().get("windows", [])
            for w in after:
                if w["id"] not in before:
                    new_id = w["id"]; break
            if new_id: break
        if not new_id:
            return {"ok": True, "integrated": False, "msg": "app lançado (sem janela X11 detectada)"}
        # integra: maximiza + fullscreen + remove decoração + foca
        for args in (["wmctrl","-ir",new_id,"-b","add,maximized_vert,maximized_horz"],
                     ["wmctrl","-ir",new_id,"-b","add,fullscreen"],
                     ["wmctrl","-ir",new_id,"-b","remove,decorated"],
                     ["wmctrl","-ia",new_id]):
            try: subprocess.run(args, check=False, timeout=2)
            except: pass
        return {"ok": True, "integrated": True, "win_id": new_id}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def launch(exe):
    """Launch simples (sem integração de janela) — usado como fallback."""
    try:
        env = dict(os.environ, DISPLAY=DISPLAY)
        subprocess.Popen(["bash","-lc", exe + " >/dev/null 2>&1 &"], env=env)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def ai_proxy(prompt):
    """Fala com a LLM (sem chave) pelo SERVIDOR. Caminho de rede diferente do
    navegador - as vezes passa onde o navegador e barrado por CORS/UA."""
    try:
        url = "https://text.pollinations.ai/" + urllib.parse.quote(prompt, safe="") + "?model=openai"
        req = urllib.request.Request(url, headers={"User-Agent": "OlalOS/1.0"})
        with urllib.request.urlopen(req, timeout=22) as r:
            return {"text": r.read().decode("utf-8", "ignore").strip()}
    except Exception as e:
        return {"text": "", "error": str(e)}

def aicore_status():
    """estado do Olal AI Core (a IA-orgao do sistema) + diario de acoes."""
    st, journal, alive = {}, [], False
    try:
        st = json.loads(open("/opt/olal/ai-state.json").read())
    except Exception:
        pass
    try:
        journal = open("/opt/olal/ai-journal.txt", errors="ignore").read().splitlines()[-12:]
    except Exception:
        pass
    try:
        out = subprocess.check_output(["pgrep", "-f", "aicore.py"], text=True, timeout=3)
        alive = bool(out.strip())
    except Exception:
        alive = False
    return {"alive": alive, "state": st, "journal": journal}

def listdir(path):
    path = os.path.abspath(os.path.expanduser(path or HOME))
    try:
        items = []
        for name in sorted(os.listdir(path)):
            full = os.path.join(path, name)
            isdir = os.path.isdir(full)
            try: size = os.path.getsize(full) if not isdir else 0
            except: size = 0
            items.append({"name": name, "dir": isdir, "size": size})
        return {"path": path, "parent": os.path.dirname(path), "items": items}
    except Exception as e:
        return {"path": path, "parent": HOME, "items": [], "error": str(e)}

class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=ROOT, **k)
    def log_message(self, *a): pass
    def end_headers(self):
        # nunca deixa o navegador servir a interface do cache
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache"); self.send_header("Expires", "0")
        super().end_headers()
    def _json(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type","application/json")
        self.send_header("Access-Control-Allow-Origin","*"); self.send_header("Content-Length",str(len(b)))
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        if u.path == "/api/sysinfo":  return self._json(sysinfo())
        if u.path == "/api/aicore":   return self._json(aicore_status())
        if u.path == "/api/apps":     return self._json(listapps())
        if u.path == "/api/windows":  return self._json(windows())
        if u.path == "/api/ls":       return self._json(listdir(q.get("path",[HOME])[0]))
        if u.path == "/api/read":
            try: return self._json({"text": open(os.path.expanduser(q.get("path",[""])[0])).read()[:100000]})
            except Exception as e: return self._json({"text": "", "error": str(e)})
        if u.path == "/api/focus":
            return self._json(focus_window(q.get("win",[""])[0]))
        if u.path == "/api/close":
            return self._json(close_window(q.get("win",[""])[0]))
        return super().do_GET()
    def do_POST(self):
        n = int(self.headers.get("Content-Length",0)); body = self.rfile.read(n) if n else b"{}"
        try: data = json.loads(body or b"{}")
        except: data = {}
        u = urllib.parse.urlparse(self.path)
        if u.path == "/api/ai":     return self._json(ai_proxy(data.get("prompt","")))
        if u.path == "/api/exec":   return self._json(run(data.get("cmd",""), data.get("cwd")))
        if u.path == "/api/launch": return self._json(launch_integrated(data.get("exec","")))
        if u.path == "/api/power":
            a = data.get("action")
            if a == "restart": subprocess.Popen(["bash","-lc","sleep 1; pkill -f olal-shell; pkill chromium; pkill firefox-esr"]); return self._json({"ok":True})
            if a == "shutdown": subprocess.Popen(["bash","-lc","sleep 1; pkill -9 -f 'startxfce\\|openbox\\|olal-shell\\|chromium\\|firefox'"]); return self._json({"ok":True})
            return self._json({"ok":False})
        if u.path == "/api/write":
            try:
                open(os.path.expanduser(data.get("path","")), "w").write(data.get("text",""))
                return self._json({"ok": True})
            except Exception as e: return self._json({"ok": False, "error": str(e)})
        self._json({"error":"rota desconhecida"}, 404)

if __name__ == "__main__":
    os.chdir(ROOT)
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("127.0.0.1", PORT), H) as srv:
        print(f"Olal OS backend em http://127.0.0.1:{PORT}", flush=True)
        srv.serve_forever()
