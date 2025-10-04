import socket
import threading
import tkinter as tk
from tkinter import messagebox
import time

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Cliente Vehículo Autónomo - Python")
        self.geometry("700x600")
        self.resizable(False, False)
        
        # Variables de conexión
        self.tcp_socket = None
        self.udp_socket = None
        self.connected = False
        self.client_id = ""
        self.is_admin = False
        
        # Variables de telemetría
        self.v_speed = tk.StringVar(value="0.0")
        self.v_batt = tk.StringVar(value="0")
        self.v_temp = tk.StringVar(value="0.0")
        self.v_dir = tk.StringVar(value="-")
        
        self.create_widgets()
        
    def create_widgets(self):
        # Frame de conexión
        conn_frame = tk.LabelFrame(self, text="Conexión", padx=10, pady=10)
        conn_frame.pack(fill="x", padx=10, pady=10)
        
        tk.Label(conn_frame, text="Servidor:").grid(row=0, column=0, sticky="e", padx=5)
        self.host_entry = tk.Entry(conn_frame, width=15)
        self.host_entry.insert(0, "127.0.0.1")
        self.host_entry.grid(row=0, column=1, padx=5)
        
        tk.Label(conn_frame, text="Puerto:").grid(row=0, column=2, sticky="e", padx=5)
        self.port_entry = tk.Entry(conn_frame, width=8)
        self.port_entry.insert(0, "5000")
        self.port_entry.grid(row=0, column=3, padx=5)
        
        tk.Label(conn_frame, text="Tipo:").grid(row=1, column=0, sticky="e", padx=5, pady=5)
        self.user_type = tk.StringVar(value="OBSERVER")
        tk.Radiobutton(conn_frame, text="Observador", variable=self.user_type, 
                      value="OBSERVER").grid(row=1, column=1, sticky="w")
        tk.Radiobutton(conn_frame, text="Administrador", variable=self.user_type, 
                      value="ADMIN").grid(row=1, column=2, sticky="w")
        
        tk.Label(conn_frame, text="Password:").grid(row=2, column=0, sticky="e", padx=5)
        self.password_entry = tk.Entry(conn_frame, show="*", width=15)
        self.password_entry.insert(0, "admin123")
        self.password_entry.grid(row=2, column=1, padx=5)
        
        self.connect_btn = tk.Button(conn_frame, text="Conectar", 
                                     command=self.connect, bg="green", fg="white")
        self.connect_btn.grid(row=2, column=2, columnspan=2, padx=5)
        
        # Frame de telemetría
        telem_frame = tk.LabelFrame(self, text="Telemetría", padx=10, pady=10)
        telem_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        tk.Label(telem_frame, text="Velocidad:", font=("Helvetica", 13)).grid(
            row=0, column=0, sticky="e", padx=12, pady=12)
        tk.Label(telem_frame, textvariable=self.v_speed, 
                font=("Helvetica", 28, "bold"), fg="blue").grid(row=0, column=1, sticky="w")
        tk.Label(telem_frame, text="km/h", font=("Helvetica", 13)).grid(
            row=0, column=2, sticky="w")
        
        tk.Label(telem_frame, text="Batería:", font=("Helvetica", 13)).grid(
            row=1, column=0, sticky="e", padx=12, pady=12)
        tk.Label(telem_frame, textvariable=self.v_batt, 
                font=("Helvetica", 28, "bold"), fg="green").grid(row=1, column=1, sticky="w")
        tk.Label(telem_frame, text="%", font=("Helvetica", 13)).grid(
            row=1, column=2, sticky="w")
        
        tk.Label(telem_frame, text="Temperatura:", font=("Helvetica", 13)).grid(
            row=2, column=0, sticky="e", padx=12, pady=12)
        tk.Label(telem_frame, textvariable=self.v_temp, 
                font=("Helvetica", 28, "bold"), fg="red").grid(row=2, column=1, sticky="w")
        tk.Label(telem_frame, text="°C", font=("Helvetica", 13)).grid(
            row=2, column=2, sticky="w")
        
        tk.Label(telem_frame, text="Dirección:", font=("Helvetica", 13)).grid(
            row=3, column=0, sticky="e", padx=12, pady=12)
        tk.Label(telem_frame, textvariable=self.v_dir, 
                font=("Helvetica", 28, "bold"), fg="purple").grid(row=3, column=1, sticky="w")
        
        # Frame de comandos
        cmd_frame = tk.LabelFrame(self, text="Comandos (Solo Admin)", 
                                 padx=10, pady=10)
        cmd_frame.pack(fill="x", padx=10, pady=10)
        
        self.speedup_btn = tk.Button(cmd_frame, text="⬆ SPEED UP", 
                                     command=lambda: self.send_command("SPUP"),
                                     state="disabled", width=15)
        self.speedup_btn.grid(row=0, column=0, padx=5, pady=5)
        
        self.slowdown_btn = tk.Button(cmd_frame, text="⬇ SLOW DOWN", 
                                      command=lambda: self.send_command("SPDN"),
                                      state="disabled", width=15)
        self.slowdown_btn.grid(row=0, column=1, padx=5, pady=5)
        
        self.left_btn = tk.Button(cmd_frame, text="⬅ TURN LEFT", 
                                 command=lambda: self.send_command("TNLF"),
                                 state="disabled", width=15)
        self.left_btn.grid(row=1, column=0, padx=5, pady=5)
        
        self.right_btn = tk.Button(cmd_frame, text="➡ TURN RIGHT", 
                                  command=lambda: self.send_command("TNRT"),
                                  state="disabled", width=15)
        self.right_btn.grid(row=1, column=1, padx=5, pady=5)
        
        self.list_btn = tk.Button(cmd_frame, text="📋 LIST USERS", 
                                 command=self.list_users,
                                 state="disabled", width=15)
        self.list_btn.grid(row=2, column=0, padx=5, pady=5)
        
        self.disconnect_btn = tk.Button(cmd_frame, text="❌ DISCONNECT", 
                                       command=self.disconnect,
                                       state="disabled", width=15, bg="red", fg="white")
        self.disconnect_btn.grid(row=2, column=1, padx=5, pady=5)
        
        # Consola de mensajes
        console_frame = tk.LabelFrame(self, text="Consola", padx=5, pady=5)
        console_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        self.console = tk.Text(console_frame, height=8, state="disabled")
        scrollbar = tk.Scrollbar(console_frame, command=self.console.yview)
        self.console.config(yscrollcommand=scrollbar.set)
        self.console.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
    def log(self, message):
        self.console.config(state="normal")
        timestamp = time.strftime("%H:%M:%S")
        self.console.insert("end", f"[{timestamp}] {message}\n")
        self.console.see("end")
        self.console.config(state="disabled")
        
    def connect(self):
        if self.connected:
            messagebox.showwarning("Advertencia", "Ya estás conectado")
            return
            
        try:
            host = self.host_entry.get()
            port = int(self.port_entry.get())
            
            # Crear socket TCP
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.connect((host, port))
            self.log(f"Conectado a {host}:{port}")
            
            # Enviar mensaje de conexión
            if self.user_type.get() == "ADMIN":
                password = self.password_entry.get()
                conn_msg = f"ADMIN:{password}"
                self.is_admin = True
            else:
                conn_msg = "OBSERVER"
                self.is_admin = False
                
            message = self.build_message("CONN", conn_msg)
            self.tcp_socket.send(message.encode())
            self.log(f"Enviado: {message.strip()}")
            
            # Recibir respuesta
            response = self.tcp_socket.recv(1024).decode()
            self.log(f"Recibido: {response.strip()}")
            
            msg_type, msg_data = self.parse_message(response)
            
            if msg_type == "CACK":
                self.client_id = msg_data
                self.connected = True
                self.log(f"✓ Conectado como {self.client_id}")
                
                # Iniciar recepción UDP
                self.start_udp_receiver()
                
                # Iniciar recepción TCP
                threading.Thread(target=self.receive_tcp, daemon=True).start()
                
                # Actualizar interfaz
                self.connect_btn.config(state="disabled")
                self.disconnect_btn.config(state="normal")
                
                if self.is_admin:
                    self.speedup_btn.config(state="normal")
                    self.slowdown_btn.config(state="normal")
                    self.left_btn.config(state="normal")
                    self.right_btn.config(state="normal")
                    self.list_btn.config(state="normal")
                    
            elif msg_type == "CERR":
                self.log(f"✗ Error: {msg_data}")
                self.tcp_socket.close()
                self.tcp_socket = None
                messagebox.showerror("Error", f"Conexión rechazada: {msg_data}")
                
        except Exception as e:
            self.log(f"✗ Error de conexión: {e}")
            messagebox.showerror("Error", f"No se pudo conectar: {e}")
            if self.tcp_socket:
                self.tcp_socket.close()
                self.tcp_socket = None
                
    def start_udp_receiver(self):
        try:
            self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_socket.bind(("0.0.0.0", 5001))
            threading.Thread(target=self.receive_udp, daemon=True).start()
            self.log("Receptor UDP iniciado en puerto 5001")
        except Exception as e:
            self.log(f"Error iniciando UDP: {e}")
            
    def receive_udp(self):
        while self.connected:
            try:
                data, addr = self.udp_socket.recvfrom(1024)
                message = data.decode()
                msg_type, msg_data = self.parse_message(message)
                
                if msg_type == "TELE":
                    self.process_telemetry(msg_data)
                    
            except Exception as e:
                if self.connected:
                    self.log(f"Error UDP: {e}")
                break
                
    def receive_tcp(self):
        while self.connected:
            try:
                data = self.tcp_socket.recv(1024)
                if not data:
                    self.log("Servidor cerró la conexión")
                    self.disconnect()
                    break
                    
                message = data.decode()
                self.log(f"Recibido: {message.strip()}")
                
            except Exception as e:
                if self.connected:
                    self.log(f"Error TCP: {e}")
                break
                
    def process_telemetry(self, data):
        # Parsear: SPEED:45.5|BATTERY:78|TEMP:35.2|DIR:NORTH
        parts = data.split("|")
        for part in parts:
            if ":" in part:
                key, value = part.split(":", 1)
                if key == "SPEED":
                    self.v_speed.set(value)
                elif key == "BATTERY":
                    self.v_batt.set(value)
                elif key == "TEMP":
                    self.v_temp.set(value)
                elif key == "DIR":
                    self.v_dir.set(value)
                    
    def send_command(self, command):
        if not self.connected or not self.is_admin:
            return
            
        try:
            message = self.build_message(command, "")
            self.tcp_socket.send(message.encode())
            self.log(f"Comando enviado: {command}")
        except Exception as e:
            self.log(f"Error enviando comando: {e}")
            
    def list_users(self):
        if not self.connected or not self.is_admin:
            return
            
        try:
            message = self.build_message("LIST", "")
            self.tcp_socket.send(message.encode())
            self.log("Solicitando lista de usuarios...")
        except Exception as e:
            self.log(f"Error: {e}")
            
    def disconnect(self):
        if not self.connected:
            return
            
        try:
            message = self.build_message("DISC", "")
            self.tcp_socket.send(message.encode())
        except:
            pass
            
        self.connected = False
        
        if self.tcp_socket:
            self.tcp_socket.close()
        if self.udp_socket:
            self.udp_socket.close()
            
        self.log("Desconectado del servidor")
        
        # Resetear interfaz
        self.connect_btn.config(state="normal")
        self.disconnect_btn.config(state="disabled")
        self.speedup_btn.config(state="disabled")
        self.slowdown_btn.config(state="disabled")
        self.left_btn.config(state="disabled")
        self.right_btn.config(state="disabled")
        self.list_btn.config(state="disabled")
        
    def build_message(self, msg_type, data):
        data_len = len(data)
        return f"{msg_type}|{data_len:04d}|{data}\n"
        
    def parse_message(self, message):
        parts = message.strip().split("|", 2)
        if len(parts) >= 3:
            return parts[0], parts[2]
        return "", ""

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", lambda: (app.disconnect(), app.destroy()))
    app.mainloop()
