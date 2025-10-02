#ifndef VEHICLE_H
#define VEHICLE_H

// ============ INCLUDES ============
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

// ============ CONSTANTES ============
#define MAX_CLIENTS 50              // Máximo de clientes simultáneos
#define BUFFER_SIZE 1024            // Tamaño del buffer de mensajes
#define ADMIN_PASSWORD "admin123"   // Password del administrador
#define TELEMETRY_INTERVAL 10       // Segundos entre envíos de telemetría
#define CLIENT_TIMEOUT 30           // Timeout de inactividad (segundos)

// ============ ENUMERACIONES ============

// Tipo de usuario
typedef enum {
    USER_OBSERVER,  // Solo puede ver telemetría
    USER_ADMIN      // Puede ver telemetría y enviar comandos
} UserType;

// Dirección del vehículo
typedef enum {
    DIR_NORTH,
    DIR_SOUTH,
    DIR_EAST,
    DIR_WEST
} Direction;

// ============ ESTRUCTURAS ============

// Estado del vehículo (datos de telemetría)
typedef struct {
    float speed;            // Velocidad en km/h (0-100)
    int battery;            // Nivel de batería en % (0-100)
    float temperature;      // Temperatura en °C (0-100)
    Direction direction;    // Dirección actual
    pthread_mutex_t lock;   // Mutex para acceso thread-safe
} VehicleState;

// Información de un cliente conectado
typedef struct {
    int socket_fd;               // File descriptor del socket TCP
    struct sockaddr_in addr;     // Dirección IP y puerto del cliente
    UserType type;               // Tipo de usuario (ADMIN u OBSERVER)
    char client_id[16];          // ID único del cliente (ej: "CLI001")
    int active;                  // 1 si está activo, 0 si desconectado
    time_t last_activity;        // Timestamp de última actividad (para timeout)
} ClientInfo;

// Lista de clientes conectados
typedef struct {
    ClientInfo clients[MAX_CLIENTS];  // Array de clientes
    int count;                        // Número de clientes activos
    pthread_mutex_t lock;             // Mutex para acceso thread-safe
} ClientList;

// Datos que se pasan a cada thread de cliente
typedef struct {
    int client_socket;                // Socket del cliente
    struct sockaddr_in client_addr;   // Dirección del cliente
    VehicleState *vehicle;            // Puntero al estado del vehículo (compartido)
    ClientList *client_list;          // Puntero a la lista de clientes (compartido)
    FILE *log_file;                   // Puntero al archivo de log (compartido)
    pthread_mutex_t *log_lock;        // Mutex para escribir en el log
} ThreadData;

// ============ FUNCIONES DEL VEHÍCULO ============

// Inicializa el estado del vehículo
void init_vehicle(VehicleState *vehicle);

// Obtiene la telemetría en formato de mensaje del protocolo
void get_telemetry_string(VehicleState *vehicle, char *buffer);

// Ejecuta un comando y devuelve la respuesta
int execute_command(VehicleState *vehicle, const char *command, char *response);

// Convierte Direction a string
const char* direction_to_string(Direction dir);

// ============ FUNCIONES DE GESTIÓN DE CLIENTES ============

// Inicializa la lista de clientes
void init_client_list(ClientList *list);

// Agrega un cliente a la lista
int add_client(ClientList *list, int socket_fd, struct sockaddr_in addr, UserType type);

// Remueve un cliente de la lista
void remove_client(ClientList *list, int socket_fd);

// Busca un cliente por su socket
ClientInfo* find_client(ClientList *list, int socket_fd);

// Obtiene una lista de todos los clientes conectados (formato string)
void get_client_list_string(ClientList *list, char *buffer);

// ============ FUNCIONES DE LOGGING ============

// Escribe un mensaje en consola y archivo de log
void log_message(FILE *log_file, pthread_mutex_t *log_lock, 
                 const char *client_id, const char *client_ip, 
                 int client_port, const char *message);

// ============ FUNCIONES DEL PROTOCOLO ============

// Parsea un mensaje recibido: "TIPO|LONG|DATOS\n"
void parse_message(const char *buffer, char *type, char *data);

// Construye un mensaje para enviar: "TIPO|LONG|DATOS\n"
void build_message(char *buffer, const char *type, const char *data);

#endif // VEHICLE_H