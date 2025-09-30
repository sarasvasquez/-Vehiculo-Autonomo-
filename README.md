# 🚗 Proyecto: Vehículo Autónomo de Telemetrí

📚 **Asignatura:** Telematica 
📅 **Periodo:** 2025-2  
👥 **Equipo de trabajo:** *[Sara Isabel Vasquez, Maria Clara Medina, Nicol Franchesca Garcia]*  

---

## 📖 Descripción
Este proyecto consiste en el diseño e implementación de un **protocolo de comunicaciones de capa de aplicación** para un vehículo autónomo terrestre.  
El vehículo transmite datos de telemetría en tiempo real (velocidad, nivel de batería, temperatura) y recibe comandos de control de distintos clientes conectados.  

El sistema incluye:  
- **Servidor en C** usando la API de Sockets de Berkeley.  
- **Clientes en al menos dos lenguajes distintos** (ejemplo: Python y Java).  
- **Interfaz gráfica en el cliente** para visualizar la telemetría.  
- **Gestión de usuarios** (administrador y observador).  
- **Logging en servidor** para registrar peticiones y respuestas.  

---

## 🎯 Objetivos
- Implementar un protocolo de aplicación que permita la comunicación entre un vehículo autónomo y múltiples clientes.  
- Diseñar un sistema concurrente donde el servidor maneje múltiples conexiones simultáneas.  
- Garantizar la autenticación de administradores y la distribución confiable de telemetría.  

---

## ⚡ Requerimientos principales
1. Enviar información de telemetría a todos los usuarios cada **10 segundos**.  
2. Recibir comandos de control:  
   - `SPEED UP`  
   - `SLOW DOWN`  
   - `TURN LEFT`  
   - `TURN RIGHT`  
3. Gestionar dos tipos de usuarios:  
   - **Administrador:** puede enviar comandos y consultar usuarios.  
   - **Observador:** solo recibe datos.  
4. Especificar el protocolo en formato **texto**, incluyendo:  
   - Visión general.  
   - Especificación del servicio.  
   - Formato de mensajes.  
   - Reglas de procedimiento.  
   - Ejemplos de uso.  

---

## 🛠️ Tecnologías
- **Servidor:** C (Sockets Berkeley, GCC, Makefile).  
- **Clientes:** Python y Java.  
- **Control de versiones:** Git / GitHub.  
- **Herramientas de apoyo:** Wireshark, Postman, Beej’s Guide.  

---
