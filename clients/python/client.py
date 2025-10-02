# clients/python/client.py
import tkinter as tk
import random, time

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Vehículo Autónomo - Demo sin hilos")
        self.geometry("700x480"); self.minsize(700,480)

        # Encabezado
        tk.Label(self, text="DEMOSTRACIÓN (sin servidor) — actualiza cada 1s",
                 font=("Helvetica", 14, "bold")).pack(fill="x", padx=10, pady=(12,6))

        # Panel telemetría
        frm = tk.LabelFrame(self, text="Telemetría")
        frm.pack(fill="both", expand=True, padx=10, pady=8)

        self.v_speed = tk.StringVar(value="0")
        self.v_batt  = tk.StringVar(value="0")
        self.v_temp  = tk.StringVar(value="0")
        self.v_dir   = tk.StringVar(value="-")

        # Fila 1: Velocidad
        tk.Label(frm, text="Velocidad:", font=("Helvetica", 13)).grid(row=0, column=0, sticky="e", padx=12, pady=12)
        tk.Label(frm, textvariable=self.v_speed, font=("Helvetica", 28, "bold")).grid(row=0, column=1, sticky="w")
        tk.Label(frm, text="km/h", font=("Helvetica", 13)).grid(row=0, column=2, sticky="w")

        # Fila 2: Batería
        tk.Label(frm, text="Batería:", font=("Helvetica", 13)).grid(row=1, column=0, sticky="e", padx=12, pady=12)
        tk.Label(frm, textvariable=self.v_batt, font=("Helvetica", 28, "bold")).grid(row=1, column=1, sticky="w")
        tk.Label(frm, text="%", font=("Helvetica", 13)).grid(row=1, column=2, sticky="w")

        # Fila 3: Temperatura
        tk.Label(frm, text="Temperatura:", font=("Helvetica", 13)).grid(row=2, column=0, sticky="e", padx=12, pady=12)
        tk.Label(frm, textvariable=self.v_temp, font=("Helvetica", 28, "bold")).grid(row=2, column=1, sticky="w")
        tk.Label(frm, text="°C", font=("Helvetica", 13)).grid(row=2, column=2, sticky="w")

        # Fila 4: Dirección
        tk.Label(frm, text="Dirección:", font=("Helvetica", 13)).grid(row=3, column=0, sticky="e", padx=12, pady=12)
        tk.Label(frm, textvariable=self.v_dir, font=("Helvetica", 28, "bold")).grid(row=3, column=1, sticky="w")

        # Consola simple
        self.console = tk.Text(self, height=8)
        self.console.pack(fill="both", expand=False, padx=10, pady=(0,10))
        self.log(">> Demo sin hilos iniciado.")

        # Estado interno
        self._speed = random.randint(10,30)
        self._batt  = random.randint(70,100)
        self._temp  = random.randint(25,38)
        self._dirs  = ["LEFT","RIGHT","FORWARD"]

        # Arranca loop de actualización sin hilos
        self.after(1000, self.tick)

    def log(self, txt: str):
        self.console.insert("end", txt.rstrip()+"\n"); self.console.see("end")

    def tick(self):
        # Actualiza números “fake”
        self._speed = max(0, min(60, self._speed + random.choice([-1,0,1])))
        self._batt  = max(0, self._batt  - random.choice([0,0,1]))
        self._temp  = max(15, min(60, self._temp + random.choice([-1,0,1])))

        self.v_speed.set(str(self._speed))
        self.v_batt.set(str(self._batt))
        self.v_temp.set(str(self._temp))
        self.v_dir.set(random.choice(self._dirs))

        self.log(f"TELEMETRY speed={self._speed} batt={self._batt} temp={self._temp} dir={self.v_dir.get()} ts={time.strftime('%H:%M:%S')}")

        # Repite cada 1s
        self.after(1000, self.tick)

if __name__ == "__main__":
    App().mainloop()
