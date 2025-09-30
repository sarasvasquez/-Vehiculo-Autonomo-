Proyecto: Vehículo Autónomo de Telemetría
Descripción
Sistema cliente-servidor que simula un vehículo autónomo que envía telemetría (velocidad, batería, temperatura, dirección) a múltiples usuarios y recibe comandos de control.
Integrantes

[Nombre 1]
[Nombre 2]
[Nombre 3]

Estructura del Proyecto
├── docs/           # Documentación del protocolo
├── server/         # Servidor en C
├── clients/        # Clientes en Python y Java
└── README.md
Requisitos Previos
Servidor (C)

GCC compiler
Make
Linux/Unix/macOS (o WSL en Windows)

Cliente Python

Python 3.8+
tkinter (para GUI)

Cliente Java

JDK 11+

Compilación y Ejecución
Servidor
bashcd server
make
./server 5000 logs/server.log
Parámetros:

5000: Puerto TCP para comandos
logs/server.log: Archivo de logs

El servidor automáticamente usa el puerto 5001 para telemetría UDP.
Cliente Python
bashcd clients/python
pip install -r requirements.txt
python client.py
Cliente Java
bashcd clients/java
javac Client.java
java Client
Uso
Como Observador

Ejecutar el cliente
Conectar como "Observer"
Ver telemetría en tiempo real

Como Administrador

Ejecutar el cliente
Conectar como "Admin" con password: admin123
Enviar comandos: SPEED UP, SLOW DOWN, TURN LEFT, TURN RIGHT
Ver lista de usuarios conectados

Protocolo
Ver especificación completa en: docs/protocolo.md
Puertos

TCP 5000: Comandos y autenticación
UDP 5001: Telemetría (cada 10 segundos)

Comandos Disponibles

SPEED UP: Aumentar velocidad
SLOW DOWN: Disminuir velocidad
TURN LEFT: Girar izquierda
TURN RIGHT: Girar derecha
LIST USERS: Ver usuarios conectados (solo admin)

Video Demostración
[Enlace al video]
Notas

El servidor soporta hasta 50 clientes simultáneos
La telemetría se envía automáticamente cada 10 segundos
Los comandos requieren permisos de administrador
