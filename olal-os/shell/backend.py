#!/usr/bin/env python3
"""Olal OS - backend do sistema.
Serve a interface E expõe o sistema real para ela (terminal, arquivos,
informações, controle). É o que faz o Olal ser um OS, não um launcher:
os apps falam com o Debian de verdade por baixo."""
import http.server, socketserver, subprocess, json, os, pwd, shutil, urllib.parse, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
PORT = int(os.environ.get("OLAL_PORT", "8080"))
HOME = os.path.expanduser("~")

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
    import glob, re
    out, seen = [], set()
    for f in sorted(glob.glob("/usr/share/applications/*.desktop") + glob.glob(os.path.expanduser("~/.local/share/applications/*.desktop"))):
        try:
            txt = open(f, errors="ignore").read()
            if "NoDisplay=true" in txt: continue
            nm = re.search(r"(?m)^Name=(.+)$", txt); ex = re.search(r"(?m)^Exec=(.+)$", txt)
            if not nm or not ex: continue
            name = nm.group(1).strip(); exe = re.sub(r"%[fFuUdDnN]", "", ex.group(1)).strip()
            key = exe.split()[0] if exe else name
            if key in seen or "olal" in f.lower(): continue
            seen.add(key); out.append({"name": name, "exec": exe})
        except: pass
    return {"apps": out}

def launch(exe):
    try:
        env = dict(os.environ, DISPLAY=os.environ.get("DISPLAY", ":0"))
        subprocess.Popen(["bash","-lc", exe + " >/dev/null 2>&1 &"], env=env)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}

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
    def _json(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type","application/json")
        self.send_header("Access-Control-Allow-Origin","*"); self.send_header("Content-Length",str(len(b)))
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        if u.path == "/api/sysinfo": return self._json(sysinfo())
        if u.path == "/api/apps":    return self._json(listapps())
        if u.path == "/api/ls":      return self._json(listdir(q.get("path",[HOME])[0]))
        if u.path == "/api/read":
            try: return self._json({"text": open(os.path.expanduser(q.get("path",[""])[0])).read()[:100000]})
            except Exception as e: return self._json({"text": "", "error": str(e)})
        return super().do_GET()
    def do_POST(self):
        n = int(self.headers.get("Content-Length",0)); body = self.rfile.read(n) if n else b"{}"
        try: data = json.loads(body or b"{}")
        except: data = {}
        u = urllib.parse.urlparse(self.path)
        if u.path == "/api/exec":   return self._json(run(data.get("cmd",""), data.get("cwd")))
        if u.path == "/api/launch": return self._json(launch(data.get("exec","")))
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
