import http.server
import urllib.request
import subprocess
import sys

WSL_IP = subprocess.check_output(['wsl', 'hostname', '-I']).decode().strip()
TARGET = f"http://{WSL_IP}:8080"
PORT = 8080

class ProxyHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            url = TARGET + self.path
            req = urllib.request.Request(url)
            for key, val in self.headers.items():
                if key.lower() not in ('host',):
                    req.add_header(key, val)
            resp = urllib.request.urlopen(req, timeout=30)
            self.send_response(resp.status)
            for key, val in resp.headers.items():
                if key.lower() not in ('transfer-encoding',):
                    self.send_header(key, val)
            self.end_headers()
            self.wfile.write(resp.read())
        except Exception as e:
            self.send_error(502, str(e))
    
    def do_POST(self):
        length = int(self.headers.get('content-length', 0))
        body = self.rfile.read(length) if length else None
        try:
            url = TARGET + self.path
            req = urllib.request.Request(url, data=body, method='POST')
            for key, val in self.headers.items():
                if key.lower() not in ('host',):
                    req.add_header(key, val)
            resp = urllib.request.urlopen(req, timeout=30)
            self.send_response(resp.status)
            for key, val in resp.headers.items():
                if key.lower() not in ('transfer-encoding',):
                    self.send_header(key, val)
            self.end_headers()
            self.wfile.write(resp.read())
        except Exception as e:
            self.send_error(502, str(e))
    
    def log_message(self, format, *args):
        pass  # Suppress logs

print(f"Proxying localhost:{PORT} -> {TARGET}")
httpd = http.server.HTTPServer(('0.0.0.0', PORT), ProxyHandler)
httpd.serve_forever()
