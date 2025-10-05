
// vehicle.c
// Implementación de las funciones declaradas en vehicle.h
// Incluye manejo de lista de clientes, parsing/build, telemetría y ejecución de comandos.

#include "vehicle.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>

void init_vehicle(VehicleState *vehicle) {
    vehicle->speed = 0.0;
    vehicle->battery = 100;
    vehicle->temperature = 25.0;
    vehicle->direction = DIR_NORTH;
    pthread_mutex_init(&vehicle->lock, NULL);
}

const char* direction_to_string(Direction dir) {
    switch (dir) {
        case DIR_NORTH: return "NORTH";
        case DIR_EAST:  return "EAST";
        case DIR_SOUTH: return "SOUTH";
        case DIR_WEST:  return "WEST";
        default: return "UNKNOWN";
    }
}

void get_telemetry_string(VehicleState *vehicle, char *buffer) {
    if (!buffer) return;
    pthread_mutex_lock(&vehicle->lock);
    char data[512];
    snprintf(data, sizeof(data), "SPEED:%.1f|BATTERY:%d|TEMP:%.1f|DIR:%s",
             vehicle->speed, vehicle->battery, vehicle->temperature,
             direction_to_string(vehicle->direction));
    int data_len = (int)strlen(data);
    snprintf(buffer, BUFFER_SIZE, "TELE|%04d|%s\n", data_len, data);
    pthread_mutex_unlock(&vehicle->lock);
}

int execute_command(VehicleState *vehicle, const char *command, char *response) {
    if (!vehicle || !command || !response) return 0;
    pthread_mutex_lock(&vehicle->lock);
    int success = 0;
    char err[64] = "";
    if (strcmp(command, "SPUP") == 0) {
        if (vehicle->battery < 20) {
            strcpy(err, "LOW_BATTERY");
        } else if (vehicle->speed >= 100.0) {
            strcpy(err, "SPEED_LIMIT");
        } else {
            vehicle->speed += 10.0;
            if (vehicle->speed > 100.0) vehicle->speed = 100.0;
            vehicle->battery -= 2;
            vehicle->temperature += 1.0;
            success = 1;
        }
    } else if (strcmp(command, "SPDN") == 0) {
        if (vehicle->speed <= 0.0) {
            strcpy(err, "SPEED_LIMIT");
        } else {
            vehicle->speed -= 10.0;
            if (vehicle->speed < 0.0) vehicle->speed = 0.0;
            vehicle->temperature -= 0.5;
            success = 1;
        }
    } else if (strcmp(command, "TNLF") == 0) {
        // girar izquierda
        vehicle->direction = (vehicle->direction + 3) % 4;
        success = 1;
    } else if (strcmp(command, "TNRT") == 0) {
        // girar derecha
        vehicle->direction = (vehicle->direction + 1) % 4;
        success = 1;
    } else {
        strcpy(err, "UNKNOWN_CMD");
    }
    pthread_mutex_unlock(&vehicle->lock);

    if (success) build_message(response, "CMOK", "EXECUTED");
    else build_message(response, "CMER", err);

    return success;
}

/* ---------- Client list management ---------- */

void init_client_list(ClientList *list) {
    list->count = 0;
    pthread_mutex_init(&list->lock, NULL);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        list->clients[i].active = 0;
        list->clients[i].socket_fd = -1;
        list->clients[i].udp_port = 0;
        list->clients[i].client_id[0] = '\0';
    }
}

int add_client(ClientList *list, int socket_fd, struct sockaddr_in addr, UserType type) {
    pthread_mutex_lock(&list->lock);
    if (list->count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&list->lock);
        return -1;
    }
    int idx = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!list->clients[i].active) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        pthread_mutex_unlock(&list->lock);
        return -1;
    }
    ClientInfo *c = &list->clients[idx];
    c->active = 1;
    c->socket_fd = socket_fd;
    c->addr = addr;
    c->type = type;
    c->udp_port = 0;
    c->last_activity = time(NULL);
    snprintf(c->client_id, CLIENT_ID_LEN, "%c%04d", (type == USER_ADMIN ? 'A' : 'O'), idx + 1);
    list->count++;
    pthread_mutex_unlock(&list->lock);
    return idx;
}

void remove_client(ClientList *list, int socket_fd) {
    pthread_mutex_lock(&list->lock);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (list->clients[i].active && list->clients[i].socket_fd == socket_fd) {
            list->clients[i].active = 0;
            list->clients[i].socket_fd = -1;
            list->clients[i].udp_port = 0;
            list->clients[i].client_id[0] = '\0';
            list->count--;
            break;
        }
    }
    pthread_mutex_unlock(&list->lock);
}

ClientInfo* find_client(ClientList *list, int socket_fd) {
    pthread_mutex_lock(&list->lock);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (list->clients[i].active && list->clients[i].socket_fd == socket_fd) {
            pthread_mutex_unlock(&list->lock);
            return &list->clients[i];
        }
    }
    pthread_mutex_unlock(&list->lock);
    return NULL;
}

void get_client_list_string(ClientList *list, char *buffer) {
    if (!buffer) return;
    buffer[0] = '\0';
    pthread_mutex_lock(&list->lock);
    char temp[256];

    // empezar con el número de clientes
    snprintf(temp, sizeof(temp), "%d", list->count);
    strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (list->clients[i].active) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &list->clients[i].addr.sin_addr, ip, sizeof(ip));
            snprintf(temp, sizeof(temp), "|%s:%d:%s:UDP=%d",
                     ip, ntohs(list->clients[i].addr.sin_port),
                     (list->clients[i].type == USER_ADMIN ? "ADMIN" : "OBSERVER"),
                     list->clients[i].udp_port);
            strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);
        }
    }
    pthread_mutex_unlock(&list->lock);
}

/* ---------- Protocol helpers ---------- */

void parse_message(const char *buffer, char *type, char *data) {
    if (!buffer || !type || !data) return;
    type[0] = '\0';
    data[0] = '\0';
    // buscar primer '|'
    const char *p1 = strchr(buffer, '|');
    if (!p1) return;
    size_t tlen = p1 - buffer;
    if (tlen >= 16) tlen = 15;
    strncpy(type, buffer, tlen);
    type[tlen] = '\0';
    const char *p2 = strchr(p1 + 1, '|');
    if (!p2) {
        strncpy(data, p1 + 1, BUFFER_SIZE - 1);
        data[BUFFER_SIZE - 1] = '\0';
        return;
    }
    const char *end = strchr(p2 + 1, '\n');
    size_t dlen = end ? (size_t)(end - (p2 + 1)) : strlen(p2 + 1);
    if (dlen >= BUFFER_SIZE) dlen = BUFFER_SIZE - 1;
    strncpy(data, p2 + 1, dlen);
    data[dlen] = '\0';
}

void build_message(char *buffer, const char *type, const char *data) {
    if (!buffer || !type) return;
    if (!data) data = "";
    int data_len = (int)strlen(data);
    snprintf(buffer, BUFFER_SIZE, "%s|%04d|%s\n", type, data_len, data);
}

/* ---------- Logging ---------- */

void log_message(FILE *log_file, pthread_mutex_t *log_lock,
                 const char *client_id, const char *client_ip, int client_port, const char *message) {
    if (!log_lock) return;
    pthread_mutex_lock(log_lock);
    char ts[64];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    if (!client_id) client_id = "-";
    if (!client_ip) client_ip = "-";
    if (!message) message = "-";
    printf("[%s] [%s] %s:%d - %s\n", ts, client_id, client_ip, client_port, message);
    if (log_file) {
        fprintf(log_file, "[%s] [%s] %s:%d - %s\n", ts, client_id, client_ip, client_port, message);
        fflush(log_file);
    }
    pthread_mutex_unlock(log_lock);
}

