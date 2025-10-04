# clients/python/client.py
import socket
import threading
import queue
import tkinter as tk
from tkinter import messagebox
import time
import random

# -------------------------
# Parser robusto para mensajes
# -------------------------
def parse_kv_line(line: str) -> dict:
    """
    Acepta varios formatos:
    1) "TELEMETRY SPEED=25 BATTERY=81 TEMP=36 DIR=LEFT TS=..."
    2) "TELE|0041|SPEED:0.0|BATTERY:100|TEMP:25.0|DIR:NORTH"
    3) Mensajes tipo "CACK|..." / "CERR|..."
    Devuelve dict con clave "_verb".
    """
    line = line.strip()
    if not line:
        return {}

    # Respuestas tipo CMOK/CERR/CACK: conservar todo tras el primer separador
    if line.upper().startswith(("CMOK|", "CMER|", "CERR|", "CACK|", "DACK|")):
        parts = line.split("|", 2)
        verb = parts[0]
        data = parts[2] if len(parts) > 2 else (parts[1] if len(parts) > 1 else "")
        return {"_verb": verb, "DATA": data}

    # Formato tipo KEY=VAL (espacios)
    parts = line.split()
    if len(parts) > 1 and "=" in parts[1]:
        d = {"_verb": parts[0]}
        for p in parts[1:]:
            if "=" in p:
                k, v = p.split("=", 1)
                d[k.strip()] = v.strip()
        return d

    # Formato con pipes y dos puntos
    if "|" in line:
        fields = line.split("|")
        verb = fields[0].upper()
        if verb == "TELE":
            verb = "TELEMETRY"
        d = {"_verb": verb}
        for fld in fields[1:]:
            fld = fld.strip()
            if not fld:
                continue
            if ":" in fld:
                k, v = fld.split(":", 1)
                d[k.strip()] = v.strip()
            else:
                idx = len([k for k in d.keys() if not k.startswith("_")])
                d[f"META_{idx}"] = fld
        return d

    # Fallback: devolver la línea cruda
    return {"_verb": "SRV", "RAW": line}


# -------------------------
# Hilo receptor de socket TCP
# -------------------------
class SocketReceiver(threading.Thread):
    def __init__(self, sock: socket.socket, out_queue: queue.Queue, on_disconnect):
        super().__init__(daemon=True)
        self.sock = sock
        self.out_queue = out_queue
        self.on_disconnect = on_disconnect
        self._stop = threading.Event()
        self._buf = b""

    def stop(self):
        self._stop.set()
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except:
            pass
        try:
            self.sock.close()
        except:
            pass

    def run(self):
        try:
            while not self._stop.is_set():
                data = self.sock.recv(4096)
                if not data:
                    break
                self._buf += data
                # Procesar por líneas terminadas en \n
                while b"\n" in self._buf:
                    line, self._buf = self._buf.split(b"\n", 1)
                    line = line.decode("utf-8", "ignore")
                    msg = parse_kv_line(line)
                    if msg:
                        self.out_queue.put(msg)
        except Exception as e:
            # enviar info de excepción a la UI para depuración
            try:
                self.out_queue.put({"_verb": "SRV", "RAW": f"RECV_EXCEPTION: {repr(e)}"})
            except:
                pass
        finally:
            try:
                self.on_disconnect()
            except:
                pass


# -------------------------
# Hilo receptor UDP (telemetría)
# -------------------------
class UDPReceiver(threading.Thread):
    def __init__(self, udp_sock: socket.socket, out_queue: queue.Queue, on_stop):
        super().__init__(daemon=True)
        self.udp_sock = udp_sock
        self.out_queue = out_queue
        self.on_stop = on_stop
        self._stop = threading.Event()

    def stop(self):
        self._stop.set()
        try:
            self.udp_sock.close()
        except:
            pass

    def run(self):
        try:
            while not self._stop.is_set():
                try:
                    data, addr = self.udp_sock.recvfrom(4096)
                except OSError:
                    break
                if not data:
                    continue
                try:
                    line = data.decode("utf-8", "ignore").strip()
                except:
                    line = ""
                if line:
                    msg = parse_kv_line(line)
                    if msg:
                        self.out_queue.put(msg)
        finally:
            try:
                self.on_stop()
            except:
                pass


# -------------------------
# Demo telemetry (thread)
# -------------------------
class DemoTelemetry(threading.Thread):
    def __init__(self, out_queue: queue.Queue, on_stop=lambda: None):
        super().__init__(daemon=True)
        self.out_queue = out_queue
        self._stop = threading.Event()
        self.on_stop = on_stop
        self.speed = random.randint(0, 20)
        self.battery = random.randint(60, 100)
        self.temp = random.randint(25, 40)
        self.dir = random.choice(["LEFT", "RIGHT", "FORWARD"])

    def stop(self):
        self._stop.set()

    def run(self):
        try:
            while not self._stop.is_set():
                # Variar un poco
                self.speed = max(0, min(60, self.speed + random.choice([-1, 0, 1])))
                self.battery = max(0, self.battery - random.choice([0, 0, 1]))
                self.temp = max(15, min(60, self.temp + random.choice([-1, 0, 1])))
                self.dir = random.choice(["LEFT", "RIGHT", "FORWARD"])

                msg = {
                    "_verb": "TELEMETRY",
                    "SPEED": str(self.speed),
                    "BATTERY": str(self.battery),
                    "TEMP": str(self.temp),
                    "DIR": self.dir,
                    "TS": time.strftime("%Y-%m-%dT%H:%M:%S")
                }
                self.out_queue.put(msg)
                time.sleep(1.0)
        finally:
            try:
                self.on_stop()
            except:
                pass


# -------------------------
# App Tkinter
# -------------------------
class TelemetryApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Vehículo Autónomo - Cliente (Observer)")
        self.geometry("720x520")
        self.minsize(720, 520)

        self.sock = None
        self.receiver = None
        self.udp_receiver = None
        self.udp_sock = None
        self.demo = None

        self.msg_queue = queue.Queue()

        self._build_ui()
        self._poll_queue()

    def _build_ui(self):
        frm_conn = tk.LabelFrame(self, text="Conexión")
        frm_conn.pack(fill="x", padx=10, pady=8)

        tk.Label(frm_conn, text="Host:").grid(row=0, column=0, padx=6, pady=6, sticky="w")
        self.ent_host = tk.Entry(frm_conn, width=20)
        self.ent_host.grid(row=0, column=1, padx=6, pady=6, sticky="w")
        self.ent_host.insert(0, "127.0.0.1")

        tk.Label(frm_conn, text="Puerto:").grid(row=0, column=2, padx=6, pady=6, sticky="w")
        self.ent_port = tk.Entry(frm_conn, width=8)
        self.ent_port.grid(row=0, column=3, padx=6, pady=6, sticky="w")
        self.ent_port.insert(0, "5555")

        self.btn_connect = tk.Button(frm_conn, text="Conectar", command=self.on_connect)
        self.btn_connect.grid(row=0, column=4, padx=6, pady=6)

        self.btn_disconnect = tk.Button(frm_conn, text="Desconectar", command=self.on_disconnect, state="disabled")
        self.btn_disconnect.grid(row=0, column=5, padx=6, pady=6)

        self.demo_var = tk.BooleanVar(value=False)
        self.chk_demo = tk.Checkbutton(frm_conn, text="Modo Demo (sin servidor)", variable=self.demo_var, command=self.on_toggle_demo)
        self.chk_demo.grid(row=1, column=0, columnspan=6, padx=6, pady=4, sticky="w")

        self.lbl_status = tk.Label(self, text="Estado: Desconectado", fg="#555")
        self.lbl_status.pack(fill="x", padx=12)

        frm_tel = tk.LabelFrame(self, text="Telemetría")
        frm_tel.pack(fill="both", expand=True, padx=10, pady=8)

        self.var_speed = tk.StringVar(value="0")
        self.var_batt  = tk.StringVar(value="0")
        self.var_temp  = tk.StringVar(value="0")
        self.var_dir   = tk.StringVar(value="-")

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

        for i in range(3):
            frm_tel.grid_rowconfigure(i, weight=1)
        frm_tel.grid_columnconfigure(1, weight=1)

        self.txt_console = tk.Text(self, height=8)
        self.txt_console.pack(fill="both", expand=False, padx=10, pady=(0,10))
        self.txt_console.insert("end", ">> Cliente listo.\n")
        self.txt_console.configure(state="disabled")

    # -------- Conexión --------
    def on_connect(self):
        if self.demo:
            messagebox.showinfo("Demo activado", "Desactiva el modo demo para conectarte al servidor real.")
            return

        host = self.ent_host.get().strip()
        port_str = self.ent_port.get().strip()
        try:
            port = int(port_str)
        except:
            messagebox.showerror("Error", "Puerto inválido.")
            return

        # Crear conexión TCP
        try:
            self.sock = socket.create_connection((host, port), timeout=5)
        except Exception as e:
            self._set_status(f"Error de conexión: {e}", bad=True)
            return

        self._set_status(f"Conectado a {host}:{port}")
        self._append_console(f"Conectado a {host}:{port}")

        # Obtener puerto local usado por la conexión TCP (para bind UDP)
        try:
            local_ip, local_port = self.sock.getsockname()
        except Exception:
            local_port = None

        # NOTA: no bindeamos UDP hasta que el handshake TCP sea exitoso
        self.udp_receiver = None
        self.udp_sock = None

        # Leer bienvenida corta (timeout corto) si llega algo
        try:
            self.sock.settimeout(1.0)
            data = self.sock.recv(4096)
            if data:
                line = data.decode("utf-8", "ignore").strip()
                self._append_console(f"SRV> {line}")
        except Exception:
            pass
        finally:
            try:
                if self.sock:
                    self.sock.settimeout(None)
            except:
                pass

        # Enviar CONN en formato TIPO|LLLL|DATA\n (sin CR, solo LF)
        try:
            conn_data = "OBSERVER"
            conn_msg = f"CONN|{len(conn_data):04d}|{conn_data}\n"
            self._append_console(f"Enviando CONN -> {conn_msg.strip()}")
            self.sock.sendall(conn_msg.encode("utf-8"))
        except Exception as e:
            self._append_console(f"Error enviando CONN: {e}")
            try:
                if self.sock:
                    self.sock.close()
            except:
                pass
            self.sock = None
            self.on_disconnect()
            return

        # Leer respuesta (CACK or CERR)
        try:
            self.sock.settimeout(2.0)
            data = self.sock.recv(4096)
            if data:
                resp = data.decode("utf-8", "ignore").strip()
                self._append_console(f"SRV> {resp}")
                if resp.upper().startswith("CERR") or "INVALID" in resp.upper():
                    self._set_status(f"Error servidor: {resp}", bad=True)
                    self.on_disconnect()
                    return
        except Exception:
            # timeout -> no respuesta corta, pero seguimos
            pass
        finally:
            try:
                if self.sock:
                    self.sock.settimeout(None)
            except:
                pass

        # Handshake OK -> bind UDP en puerto local si lo conocemos
        if local_port:
            try:
                udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                udp_sock.bind(('', local_port))
                self.udp_sock = udp_sock
                self.udp_receiver = UDPReceiver(udp_sock, self.msg_queue, on_stop=lambda: self._append_console("UDP receiver stopped."))
                self.udp_receiver.start()
                self._append_console(f"UDP listener bind en puerto local {local_port}")
            except Exception as e:
                self._append_console(f"Warning: no se pudo bind UDP en {local_port}: {e}")

        # Arrancar receptor TCP para mensajes posteriores
        self.receiver = SocketReceiver(self.sock, self.msg_queue, self._on_socket_closed)
        self.receiver.start()

        self.btn_connect.config(state="disabled")
        self.btn_disconnect.config(state="normal")
        self._set_status("Conectado y listo")

    def on_disconnect(self):
        if self.receiver:
            self.receiver.stop()
            self.receiver = None
        if self.udp_receiver:
            try:
                self.udp_receiver.stop()
            except:
                pass
            self.udp_receiver = None
        if self.udp_sock:
            try:
                self.udp_sock.close()
            except:
                pass
            self.udp_sock = None
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.sock = None
        self.btn_connect.config(state="normal")
        self.btn_disconnect.config(state="disabled")
        self._set_status("Desconectado")
        self._append_console("Desconectado.")

    def _on_socket_closed(self):
        self.after(0, self.on_disconnect)

    # -------- Demo --------
    def on_toggle_demo(self):
        if self.demo_var.get():
            if self.sock or self.receiver:
                self.on_disconnect()
            self.demo = DemoTelemetry(self.msg_queue, on_stop=lambda: self._append_console("Demo detenido."))
            self.demo.start()
            self._set_status("Demo: Generando telemetría falsa…")
            self._append_console("Demo iniciado.")
            self.btn_connect.config(state="disabled")
            self.btn_disconnect.config(state="normal")
        else:
            if self.demo:
                self.demo.stop()
                self.demo = None
            self.btn_connect.config(state="normal")
            self.btn_disconnect.config(state="disabled")
            self._set_status("Desconectado")
            self._append_console("Demo desactivado.")

    # -------- UI helpers --------
    def _append_console(self, text: str):
        self.txt_console.configure(state="normal")
        self.txt_console.insert("end", text.rstrip() + "\n")
        self.txt_console.see("end")
        self.txt_console.configure(state="disabled")

    def _set_status(self, text: str, bad: bool=False):
        self.lbl_status.config(text=f"Estado: {text}", foreground=("#a00" if bad else "#555"))

    # -------- Poll de la cola --------
    def _poll_queue(self):
        try:
            while True:
                msg = self.msg_queue.get_nowait()
                self._handle_msg(msg)
        except queue.Empty:
            pass
        self.after(100, self._poll_queue)

    def _handle_msg(self, msg: dict):
        verb = msg.get("_verb", "")
        if verb == "TELEMETRY":
            spd = msg.get("SPEED")
            bat = msg.get("BATTERY")
            tmp = msg.get("TEMP")
            drr = msg.get("DIR")
            ts  = msg.get("TS")

            if spd is not None:
                try:
                    spd_val = float(spd)
                    spd = str(int(spd_val)) if spd_val.is_integer() else str(spd_val)
                except:
                    pass
                self.var_speed.set(spd)
            if bat is not None:
                self.var_batt.set(str(bat))
            if tmp is not None:
                try:
                    tmp_val = float(tmp)
                    tmp = str(int(tmp_val)) if tmp_val.is_integer() else str(tmp_val)
                except:
                    pass
                self.var_temp.set(tmp)
            if drr is not None:
                self.var_dir.set(drr)

            self._append_console(f"TELEMETRY {msg}")
        else:
            # Mostrar cualquier mensaje servidor / raw
            self._append_console(f"SRV> {msg}")

if __name__ == "__main__":
    app = TelemetryApp()
    app.mainloop()
