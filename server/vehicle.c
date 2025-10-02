#include "vehicle.h"


// FUNCIONES DEL VEHÍCULO


// Inicializa el estado del vehículo con valores por defecto
void init_vehicle(VehicleState *vehicle) {
    vehicle->speed = 0.0;           // Empieza detenido
    vehicle->battery = 100;         // Batería llena
    vehicle->temperature = 25.0;    // Temperatura ambiente
    vehicle->direction = DIR_NORTH; // Mirando al norte
    pthread_mutex_init(&vehicle->lock, NULL);
}

// Convierte la dirección a string para mostrar
const char* direction_to_string(Direction dir) {
    switch(dir) {
        case DIR_NORTH: return "NORTH";
        case DIR_SOUTH: return "SOUTH";
        case DIR_EAST:  return "EAST";
        case DIR_WEST:  return "WEST";
        default:        return "UNKNOWN";
    }
}

// Genera el mensaje de telemetría en formato del protocolo
// Formato: TELE|LONGITUD|SPEED:45.5|BATTERY:78|TEMP:35.2|DIR:NORTH\n
void get_telemetry_string(VehicleState *vehicle, char *buffer) {
    pthread_mutex_lock(&vehicle->lock);
    
    // Construir los datos de telemetría
    char data[256];
    snprintf(data, sizeof(data), "SPEED:%.1f|BATTERY:%d|TEMP:%.1f|DIR:%s",
             vehicle->speed, 
             vehicle->battery, 
             vehicle->temperature,
             direction_to_string(vehicle->direction));
    
    // Construir mensaje completo: TIPO|LONGITUD|DATOS\n
    int data_len = strlen(data);
    snprintf(buffer, BUFFER_SIZE, "TELE|%04d|%s\n", data_len, data);
    
    pthread_mutex_unlock(&vehicle->lock);
}

// Ejecuta un comando del administrador y devuelve la respuesta
// Retorna 1 si fue exitoso, 0 si hubo error
int execute_command(VehicleState *vehicle, const char *command, char *response) {
    pthread_mutex_lock(&vehicle->lock);
    
    int success = 0;
    char error_msg[50] = "";
    
    // COMANDO: SPEED UP (aumentar velocidad)
    if (strcmp(command, "SPUP") == 0) {
        if (vehicle->battery < 20) {
            strcpy(error_msg, "LOW_BATTERY");
        } else if (vehicle->speed >= 100.0) {
            strcpy(error_msg, "SPEED_LIMIT");
        } else {
            vehicle->speed += 10.0;
            if (vehicle->speed > 100.0) vehicle->speed = 100.0;
            vehicle->battery -= 2;      // Consume batería
            vehicle->temperature += 1.0; // Aumenta temperatura
            success = 1;
        }
    } 
    // COMANDO: SLOW DOWN (disminuir velocidad)
    else if (strcmp(command, "SPDN") == 0) {
        if (vehicle->speed <= 0.0) {
            strcpy(error_msg, "SPEED_LIMIT");
        } else {
            vehicle->speed -= 10.0;
            if (vehicle->speed < 0.0) vehicle->speed = 0.0;
            vehicle->temperature -= 0.5; // Disminuye temperatura
            success = 1;
        }
    } 
    // COMANDO: TURN LEFT (girar izquierda)
    else if (strcmp(command, "TNLF") == 0) {
        switch(vehicle->direction) {
            case DIR_NORTH: vehicle->direction = DIR_WEST; break;
            case DIR_WEST:  vehicle->direction = DIR_SOUTH; break;
            case DIR_SOUTH: vehicle->direction = DIR_EAST; break;
            case DIR_EAST:  vehicle->direction = DIR_NORTH; break;
        }
        success = 1;
    } 
    // COMANDO: TURN RIGHT (girar derecha)
    else if (strcmp(command, "TNRT") == 0) {
        switch(vehicle->direction) {
            case DIR_NORTH: vehicle->direction = DIR_EAST; break;
            case DIR_EAST:  vehicle->direction = DIR_SOUTH; break;
            case DIR_SOUTH: vehicle->direction = DIR_WEST; break;
            case DIR_WEST:  vehicle->direction = DIR_NORTH; break;
        }
        success = 1;
    }
    
    pthread_mutex_unlock(&vehicle->lock);
    
    // Construir respuesta según el protocolo
    if (success) {
        build_message(response, "CMOK", "EXECUTED");
    } else {
        build_message(response, "CMER", error_msg);
    }
    
    return success;
}


// FUNCIONES DE GESTIÓN DE CLIENTES


// Inicializa la lista de clientes vacía
void init_client_list(ClientList *list) {
    list->count = 0;
    pthread_mutex_init(&list->lock, NULL);
    
    // Marcar todos los slots como inactivos
    for (int i = 0; i < MAX_CLIENTS; i++) {
        list->clients[i].active = 0;
    }
}

// Agrega un nuevo cliente a la lista
// Retorna el índice del cliente agregado, o -1 si está llena
int add_client(ClientList *list, int socket_fd, struct sockaddr_in addr, UserType type) {
    pthread_mutex_lock(&list->lock);
    
    // Verificar si hay espacio
    if (list->count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&list->lock);
        return -1;
    }
    
    // Buscar un slot libre
    int index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!list->clients[i].active) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        pthread_mutex_unlock(&list->lock);
        return -1;
    }
    
    // Configurar el cliente
    list->clients[index].socket_fd = socket_fd;
    list->clients[index].addr = addr;
    list->clients[index].type = type;
    snprintf(list->clients[index].client_id, 16, "CLI%03d", index + 1);
    list->clients[index].active = 1;
    list->clients[index].last_activity = time(NULL);
    list->count++;
    
    pthread_mutex_unlock(&list->lock);
    return index;
}

// Remueve un cliente de la lista por su socket
void remove_client(ClientList *list, int socket_fd) {
    pthread_mutex_lock(&list->lock);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (list->clients[i].active && list->clients[i].socket_fd == socket_fd) {
            list->clients[i].active = 0;
            list->count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&list->lock);
}

// Busca un cliente por su socket
// Retorna puntero al cliente o NULL si no existe
ClientInfo* find_client(ClientList *list, int socket_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (list->clients[i].active && list->clients[i].socket_fd == socket_fd) {
            return &list->clients[i];
        }
    }
    return NULL;
}

// Genera un string con la lista de todos los clientes conectados
// Formato: ULST|LONG|COUNT|IP1:PORT1:TYPE1|IP2:PORT2:TYPE2|...\n
void get_client_list_string(ClientList *list, char *buffer) {
    pthread_mutex_lock(&list->lock);
    
    char data[512];
    char temp[128];
    
    // Empezar con el número de clientes
    snprintf(data, sizeof(data), "%d", list->count);
    
    // Agregar info de cada cliente
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (list->clients[i].active) {
            snprintf(temp, sizeof(temp), "|%s:%d:%s",
                     inet_ntoa(list->clients[i].addr.sin_addr),
                     ntohs(list->clients[i].addr.sin_port),
                     list->clients[i].type == USER_ADMIN ? "ADMIN" : "OBSERVER");
            strcat(data, temp);
        }
    }
    
    build_message(buffer, "ULST", data);
    
    pthread_mutex_unlock(&list->lock);
}


// FUNCIONES DE LOGGING


// Escribe un mensaje tanto en consola como en archivo de log
void log_message(FILE *log_file, pthread_mutex_t *log_lock, 
                 const char *client_id, const char *client_ip, 
                 int client_port, const char *message) {
    pthread_mutex_lock(log_lock);
    
    // Obtener timestamp actual
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Escribir en consola
    printf("[%s] [%s] %s:%d - %s\n", 
           time_str, client_id, client_ip, client_port, message);
    
    // Escribir en archivo si está disponible
    if (log_file) {
        fprintf(log_file, "[%s] [%s] %s:%d - %s\n", 
                time_str, client_id, client_ip, client_port, message);
        fflush(log_file);  // Asegurar que se escriba inmediatamente
    }
    
    pthread_mutex_unlock(log_lock);
}


// FUNCIONES DEL PROTOCOLO


// Parsea un mensaje recibido en sus componentes
// Entrada: "TIPO|LONGITUD|DATOS\n"
// Salida: type="TIPO", data="DATOS"
void parse_message(const char *buffer, char *type, char *data) {
    // Usar sscanf para extraer las partes
    // %4s lee exactamente 4 caracteres para el tipo
    // %*4d lee 4 dígitos pero los ignora (el * significa "no guardar")
    // %[^\n] lee todo hasta encontrar \n
    sscanf(buffer, "%4s|%*4d|%[^\n]", type, data);
}

// Construye un mensaje para enviar según el protocolo
// Entrada: type="CMOK", data="EXECUTED"
// Salida: buffer="CMOK|0008|EXECUTED\n"
void build_message(char *buffer, const char *type, const char *data) {
    int data_len = strlen(data);
    snprintf(buffer, BUFFER_SIZE, "%s|%04d|%s\n", type, data_len, data);
}