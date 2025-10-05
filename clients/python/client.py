
# client.py
# Cliente GUI para el servidor del vehículo autónomo.
# Reemplaza tu antiguo client.py por este archivo.

import socket
import threading
import tkinter as tk
from tkinter import messagebox
import time

BUFFER = 4096

def build_message(msg_type: str, data: str) -> str:
    data_len = len(data)
    return f"{msg_type}|{data_len:04d}|{data}\n"

def parse_message(message: str):
    parts = message.strip().split("|", 2)
    if len(parts) >= 3:
        return parts[0], parts[2]
    return "", ""

class TelemetryApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Cliente Vehículo Autónomo")
        self.geometry("720x520")
        self.minsize(720, 520)

        # Sockets y estado
        self.tcp_sock = None
        self.udp_sock = None
        self.connected = False
        self.is_admin = False
        self.client_id = None

        # Telemetry vars
        self.var_speed = tk.StringVar(value="0")
        self.var_batt  = tk.StringVar(value="0")
        self.var_temp  = tk.StringVar(value="0")
        self.var_dir   = tk.StringVar(value="-")

        self._build_ui()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self):
        frm_conn = tk.LabelFrame(self, text="Conexión")
        frm_conn.pack(fill="x", padx=10, pady=8)

        tk.Label(frm_conn, text="Host:").grid(row=0, column=0, padx=6, pady=6, sticky="w")
        self.ent_host = tk.Entry(frm_conn, width=20); self.ent_host.grid(row=0, column=1)
        self.ent_host.insert(0, "127.0.0.1")

        tk.Label(frm_conn, text="Puerto TCP:").grid(row=0, column=2, padx=6, sticky="w")
        self.ent_port = tk.Entry(frm_conn, width=8); self.ent_port.grid(row=0, column=3)
        self.ent_port.insert(0, "5555")

        self.user_type = tk.StringVar(value="OBSERVER")
        tk.Radiobutton(frm_conn, text="Observador", variable=self.user_type, value="OBSERVER").grid(row=1, column=0, sticky="w")
        tk.Radiobutton(frm_conn, text="Administrador", variable=self.user_type, value="ADMIN").grid(row=1, column=1, sticky="w")

        tk.Label(frm_conn, text="Password (admin):").grid(row=1, column=2, sticky="e")
        self.ent_pass = tk.Entry(frm_conn, show="*", width=12); self.ent_pass.grid(row=1, column=3)
        self.ent_pass.insert(0, "admin123")

        self.btn_connect = tk.Button(frm_conn, text="Conectar", command=self.on_connect)
        self.btn_connect.grid(row=0, column=4, padx=6)
        self.btn_disconnect = tk.Button(frm_conn, text="Desconectar", command=self.on_disconnect, state="disabled")
        self.btn_disconnect.grid(row=0, column=5, padx=6)

        self.lbl_status = tk.Label(self, text="Estado: Desconectado"); self.lbl_status.pack(fill="x", padx=12)

        # Telemetry frame
        frm_tel = tk.LabelFrame(self, text="Telemetría")
        frm_tel.pack(fill="both", expand=True, padx=10, pady=8)

        tk.Label(frm_tel, text="Velocidad:").grid(row=0, column=0, padx=10, pady=10, sticky="e")
        tk.Label(frm_tel, textvariable=self.var_speed, font=("Segoe UI", 16, "bold")).grid(row=0, column=1, padx=10, pady=10, sticky="w")
        tk.Label(frm_tel, text="km/h").grid(row=0, column=2, sticky="w")

        tk.Label(frm_tel, text="Batería:").grid(row=1, column=0, padx=10, pady=10, sticky="e")
        tk.Label(frm_tel, textvariable=self.var_batt, font=("Segoe UI", 16, "bold")).grid(row=1, column=1, padx=10, pady=10, sticky="w")
        tk.Label(frm_tel, text="%").grid(row=1, column=2, sticky="w")

        tk.Label(frm_tel, text="Temperatura:").grid(row=2, column=0, padx=10, pady=10, sticky="e")
        tk.Label(frm_tel, textvariable=self.var_temp, font=("Segoe UI", 16, "bold")).grid(row=2, column=1, padx=10, pady=10, sticky="w")
        tk.Label(frm_tel, text="°C").grid(row=2, column=2, sticky="w")

        tk.Label(frm_tel, text="Dirección:").grid(row=3, column=0, padx=10, pady=10, sticky="e")
        tk.Label(frm_tel, textvariable=self.var_dir, font=("Segoe UI", 16, "bold")).grid(row=3, column=1, padx=10, pady=10, sticky="w")

        # Commands frame (admin only)
        frm_cmd = tk.LabelFrame(self, text="Comandos (solo admin)")
        frm_cmd.pack(fill="x", padx=10, pady=6)

        self.btn_spup = tk.Button(frm_cmd, text="⬆ SPEED UP", command=lambda: self.send_command("SPUP"), state="disabled"); self.btn_spup.grid(row=0,column=0,padx=6)
        self.btn_spdn = tk.Button(frm_cmd, text="⬇ SLOW DOWN", command=lambda: self.send_command("SPDN"), state="disabled"); self.btn_spdn.grid(row=0,column=1,padx=6)
        self.btn_left = tk.Button(frm_cmd, text="⬅ TURN LEFT", command=lambda: self.send_command("TNLF"), state="disabled"); self.btn_left.grid(row=0,column=2,padx=6)
        self.btn_right= tk.Button(frm_cmd, text="➡ TURN RIGHT", command=lambda: self.send_command("TNRT"), state="disabled"); self.btn_right.grid(row=0,column=3,padx=6)
        self.btn_list  = tk.Button(frm_cmd, text="📋 LIST USERS", command=self.send_list, state="disabled"); self.btn_list.grid(row=0,column=4,padx=6)

        # Console
        self.txt_console = tk.Text(self, height=8, state="disabled")
        self.txt_console.pack(fill="both", expand=False, padx=10, pady=(0,10))
        self._append_console(">> Cliente listo.")

    # UI helpers
    def _append_console(self, text: str):
        self.txt_console.configure(state="normal")
        self.txt_console.insert("end", text.rstrip() + "\n")
        self.txt_console.see("end")
        self.txt_console.configure(state="disabled")

    def _set_status(self, text: str, bad: bool=False):
        self.lbl_status.config(text=f"Estado: {text}", foreground=("#a00" if bad else "#000"))
        self._append_console(text)

    # Connect / handshake
    def on_connect(self):
        if self.connected:
            messagebox.showwarning("Advertencia", "Ya estás conectado")
            return

        host = self.ent_host.get().strip()
        try:
            port = int(self.ent_port.get().strip())
        except ValueError:
            messagebox.showerror("Error", "Puerto inválido")
            return

        # Crear UDP ephemeral socket para recibir telemetría
        try:
            self.udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.udp_sock.bind(("0.0.0.0", 0))
            udp_port = self.udp_sock.getsockname()[1]
            self._append_console(f"UDP creado en puerto local {udp_port}")
        except Exception as e:
            self._append_console(f"Error creando UDP: {e}")
            self.udp_sock = None
            udp_port = 0

        # Crear TCP y conectar
        try:
            self.tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_sock.settimeout(6.0)
            self._append_console(f"Conectando a {host}:{port} ...")
            self.tcp_sock.connect((host, port))
            # volver a modo bloqueante
            self.tcp_sock.settimeout(None)
        except Exception as e:
            self._append_console(f"Error TCP connect: {e}")
            messagebox.showerror("Error", f"No se pudo conectar TCP: {e}")
            try:
                if self.tcp_sock: self.tcp_sock.close()
            except:
                pass
            self.tcp_sock = None
            if self.udp_sock:
                try: self.udp_sock.close()
                except: pass
                self.udp_sock = None
            return

        # Preparar CONN incluyendo puerto UDP
        if self.user_type.get() == "ADMIN":
            passwd = self.ent_pass.get().strip()
            conn_data = f"ADMIN:{passwd}:{udp_port}"
            self.is_admin = True
        else:
            conn_data = f"OBSERVER:{udp_port}"
            self.is_admin = False

        try:
            msg = build_message("CONN", conn_data)
            self.tcp_sock.sendall(msg.encode())
            self._append_console(f"Enviado: {msg.strip()}")

            # leer respuesta de handshake (línea)
            buf = b""
            while b"\n" not in buf:
                chunk = self.tcp_sock.recv(BUFFER)
                if not chunk:
                    raise ConnectionError("Servidor cerró la conexión")
                buf += chunk
            line, _ = buf.split(b"\n", 1)
            line = line.decode(errors="ignore").strip()
            self._append_console(f"Recibido: {line}")
            typ, data = parse_message(line)
            if typ == "CACK":
                self.client_id = data
                self.connected = True
                self._append_console(f"✓ Conectado como {self.client_id}")
                # iniciar listeners
                threading.Thread(target=self._tcp_recv_loop, daemon=True).start()
                if self.udp_sock:
                    threading.Thread(target=self._udp_recv_loop, daemon=True).start()
                # habilitar UI
                if self.is_admin:
                    for b in (self.btn_spup, self.btn_spdn, self.btn_left, self.btn_right, self.btn_list):
                        b.config(state="normal")
                self.btn_connect.config(state="disabled")
                self.btn_disconnect.config(state="normal")
                self._set_status("Conectado")
            else:
                self._append_console(f"✗ Conexión rechazada: {typ} {data}")
                messagebox.showerror("Error", f"Conexión rechazada: {typ} {data}")
                try: self.tcp_sock.close()
                except: pass
                self.tcp_sock = None
        except Exception as e:
            self._append_console(f"Error handshake: {e}")
            try: self.tcp_sock.close()
            except: pass
            self.tcp_sock = None

    def _tcp_recv_loop(self):
        try:
            while self.connected and self.tcp_sock:
                data = self.tcp_sock.recv(BUFFER)
                if not data:
                    break
                for line in data.decode(errors="ignore").splitlines():
                    self._append_console(f"TCP: {line}")
        except Exception as e:
            if self.connected: self._append_console(f"TCP recv error: {e}")
        finally:
            self._append_console("TCP cerrado por servidor")
            self.on_disconnect()

    def _udp_recv_loop(self):
        try:
            while self.connected and self.udp_sock:
                try:
                    data, addr = self.udp_sock.recvfrom(BUFFER)
                except OSError:
                    break
                text = data.decode(errors="ignore").strip()
                self._append_console(f"UDP desde {addr}: {text}")
                typ, payload = parse_message(text)
                if typ == "TELE":
                    parts = payload.split("|")
                    for p in parts:
                        if ":" in p:
                            k, v = p.split(":", 1)
                            k = k.strip().upper()
                            if k == "SPEED": self.var_speed.set(v)
                            elif k == "BATTERY": self.var_batt.set(v)
                            elif k == "TEMP": self.var_temp.set(v)
                            elif k == "DIR": self.var_dir.set(v)
                else:
                    # también aceptar payload simple "SPEED:..|BATTERY:.."
                    if "SPEED:" in text:
                        parts = text.split("|")
                        for p in parts:
                            if ":" in p:
                                k, v = p.split(":",1)
                                k = k.strip().upper()
                                if k == "SPEED": self.var_speed.set(v)
                                elif k == "BATTERY": self.var_batt.set(v)
                                elif k == "TEMP": self.var_temp.set(v)
                                elif k == "DIR": self.var_dir.set(v)
        finally:
            self._append_console("UDP receptor terminado")

    # commands
    def send_command(self, cmd: str):
        if not self.connected or not self.is_admin:
            return
        try:
            self.tcp_sock.sendall(build_message(cmd, "").encode())
            self._append_console(f"Comando enviado: {cmd}")
        except Exception as e:
            self._append_console(f"Error enviando comando: {e}")

    def send_list(self):
        if not self.connected or not self.is_admin:
            return
        try:
            self.tcp_sock.sendall(build_message("LIST", "").encode())
            self._append_console("Solicitado LIST")
        except Exception as e:
            self._append_console(f"Error LIST: {e}")

    def on_disconnect(self):
        # invoked by GUI button or when server closes
        self.connected = False
        try:
            if self.tcp_sock:
                try:
                    self.tcp_sock.sendall(build_message("DISC","").encode())
                except: pass
                self.tcp_sock.close()
        except:
            pass
        try:
            if self.udp_sock:
                self.udp_sock.close()
        except:
            pass
        self.tcp_sock = None
        self.udp_sock = None

        for b in (self.btn_spup, self.btn_spdn, self.btn_left, self.btn_right, self.btn_list):
            b.config(state="disabled")
        self.btn_connect.config(state="normal")
        self.btn_disconnect.config(state="disabled")
        self._set_status("Desconectado")
        self._append_console("Desconectado")

    def _on_close(self):
        try:
            self.on_disconnect()
        finally:
            self.destroy()

if __name__ == "__main__":
    app = TelemetryApp()
    app.mainloop()

