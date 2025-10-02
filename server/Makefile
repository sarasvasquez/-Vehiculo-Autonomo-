# Makefile para el Servidor del Vehículo Autónomo
# Uso: make        (compilar)
#      make clean  (limpiar archivos compilados)
#      make run    (compilar y ejecutar)

# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = server
OBJS = server.o vehicle.o

# Regla por defecto: compilar todo
all: $(TARGET)
	@echo ""
	@echo "✓ Compilación exitosa"
	@echo ""
	@echo "Para ejecutar:"
	@echo "  ./server <puerto> <archivo_log>"
	@echo ""
	@echo "Ejemplo:"
	@echo "  ./server 5000 logs/server.log"
	@echo ""

# Compilar el ejecutable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compilar server.o
server.o: server.c vehicle.h
	$(CC) $(CFLAGS) -c server.c

# Compilar vehicle.o
vehicle.o: vehicle.c vehicle.h
	$(CC) $(CFLAGS) -c vehicle.c

# Limpiar archivos compilados
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "✓ Archivos limpiados"

# Crear directorio de logs si no existe
setup:
	mkdir -p logs
	@echo "✓ Directorio logs/ creado"

# Compilar y crear directorios
build: setup all

# Ejecutar (para pruebas rápidas)
run: all setup
	./$(TARGET) 5000 logs/server.log

.PHONY: all clean setup build run