// vehicle.h
// Declaraciones compartidas para vehicle.c y server.c

#ifndef VEHICLE_H
#define VEHICLE_H

#include <stdio.h>
#include <pthread.h>
#include <netinet/in.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 32
#define CLIENT_ID_LEN 32
#define ADMIN_PASSWORD "admin123"
#define TELEMETRY_INTERVAL 10

// Puerto UDP por defecto si el cliente no registró uno
#define UDP_FALLBACK_PORT 5001

typedef enum { DIR_NORTH = 0, DIR_EAST = 1, DIR_SOUTH = 2, DIR_WEST = 3 } Direction;
typedef enum { USER_OBSERVER = 0, USER_ADMIN = 1 } UserType;

typedef struct {
    double speed;           // km/h
    int battery;            // %
    double temperature;     // C
    Direction direction;
    pthread_mutex_t lock;
} VehicleState;

typedef struct {
    int socket_fd;
    struct sockaddr_in addr; // dirección TCP (IP) del cliente
    int active;
    UserType type;
    char client_id[CLIENT_ID_LEN];
    int udp_port;           // puerto UDP registrado por el cliente (0 = no registrado)
    time_t last_activity;
} ClientInfo;

typedef struct {
    pthread_mutex_t lock;
    ClientInfo clients[MAX_CLIENTS];
    int count;
} ClientList;

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
    VehicleState *vehicle;
    ClientList *client_list;
    FILE *log_file;
    pthread_mutex_t *log_lock;
} ThreadData;

/* vehicle: inicialización / telemetría / comandos */
void init_vehicle(VehicleState *vehicle);
const char* direction_to_string(Direction dir);
void get_telemetry_string(VehicleState *vehicle, char *buffer);
int execute_command(VehicleState *vehicle, const char *command, char *response);

/* client list management */
void init_client_list(ClientList *list);
int add_client(ClientList *list, int socket_fd, struct sockaddr_in addr, UserType type);
void remove_client(ClientList *list, int socket_fd);
ClientInfo* find_client(ClientList *list, int socket_fd);
void get_client_list_string(ClientList *list, char *buffer);

/* protocolo: parsing/build */
void parse_message(const char *buffer, char *type, char *data);
void build_message(char *buffer, const char *type, const char *data);

/* logging util */
void log_message(FILE *log_file, pthread_mutex_t *log_lock,
                 const char *client_id, const char *client_ip, int client_port, const char *message);

#endif // VEHICLE_H
