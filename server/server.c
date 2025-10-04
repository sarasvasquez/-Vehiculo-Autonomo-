#include "vehicle.h"
#include <signal.h>


// VARIABLES GLOBALES (para poder cerrar desde signal handler)

int tcp_socket_fd = -1;
int udp_socket_fd = -1;
FILE *global_log_file = NULL;

// Manejador de señal para cerrar el servidor limpiamente con Ctrl+C
void signal_handler(int signum) {
    printf("\n[SERVER] Cerrando servidor...\n");
    if (tcp_socket_fd != -1) close(tcp_socket_fd);
    if (udp_socket_fd != -1) close(udp_socket_fd);
    if (global_log_file) fclose(global_log_file);
    exit(0);
}


// THREAD: MANEJO DE CADA CLIENTE TCP

void* handle_client(void* arg) {
    ThreadData *data = (ThreadData*)arg;
    int client_socket = data->client_socket;
    struct sockaddr_in client_addr = data->client_addr;
    VehicleState *vehicle = data->vehicle;
    ClientList *client_list = data->client_list;
    FILE *log_file = data->log_file;
    pthread_mutex_t *log_lock = data->log_lock;
    
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    ClientInfo *client = NULL;
    
    // ========== FASE 1: CONEXIÓN INICIAL ==========
    // Leer primer mensaje (debe ser CONN)
    int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        free(data);
        return NULL;
    }
    buffer[bytes_read] = '\0';
    
    log_message(log_file, log_lock, "NEW", client_ip, client_port, buffer);
    
    // Parsear mensaje
    char msg_type[5];
    char msg_data[BUFFER_SIZE];
    parse_message(buffer, msg_type, msg_data);
    
    char response[BUFFER_SIZE];
    
    // Verificar que sea mensaje CONN
    if (strcmp(msg_type, "CONN") == 0) {
        UserType user_type;
        
        // ¿Es ADMIN o OBSERVER?
        if (strncmp(msg_data, "ADMIN:", 6) == 0) {
            // Extraer password después de "ADMIN:"
            char *password = msg_data + 6;
            
            if (strcmp(password, ADMIN_PASSWORD) == 0) {
                // Password correcto
                user_type = USER_ADMIN;
                int index = add_client(client_list, client_socket, client_addr, user_type);
                
                if (index >= 0) {
                    client = find_client(client_list, client_socket);
                    build_message(response, "CACK", client->client_id);
                    send(client_socket, response, strlen(response), 0);
                    log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
                } else {
                    // Lista llena
                    build_message(response, "CERR", "MAX_CLIENTS_REACHED");
                    send(client_socket, response, strlen(response), 0);
                    close(client_socket);
                    free(data);
                    return NULL;
                }
            } else {
                // Password incorrecto
                build_message(response, "CERR", "INVALID_CREDENTIALS");
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, "AUTH_FAIL", client_ip, client_port, response);
                close(client_socket);
                free(data);
                return NULL;
            }
        } else if (strcmp(msg_data, "OBSERVER") == 0) {
            // Conectar como observador
            user_type = USER_OBSERVER;
            int index = add_client(client_list, client_socket, client_addr, user_type);
            
            if (index >= 0) {
                client = find_client(client_list, client_socket);
                build_message(response, "CACK", client->client_id);
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            } else {
                build_message(response, "CERR", "MAX_CLIENTS_REACHED");
                send(client_socket, response, strlen(response), 0);
                close(client_socket);
                free(data);
                return NULL;
            }
        } else {
            // Formato inválido
            build_message(response, "CERR", "INVALID_MESSAGE");
            send(client_socket, response, strlen(response), 0);
            close(client_socket);
            free(data);
            return NULL;
        }
    } else {
        // No envió CONN como primer mensaje
        build_message(response, "CERR", "INVALID_MESSAGE");
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        free(data);
        return NULL;
    }
    
    // ========== FASE 2: LOOP PRINCIPAL - RECIBIR COMANDOS ==========
    while (1) {
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read <= 0) {
            // Cliente desconectado
            log_message(log_file, log_lock, client->client_id, client_ip, client_port, "DISCONNECTED");
            break;
        }
        
        buffer[bytes_read] = '\0';
        log_message(log_file, log_lock, client->client_id, client_ip, client_port, buffer);
        
        parse_message(buffer, msg_type, msg_data);
        
        // ===== COMANDOS DE MOVIMIENTO =====
        if (strcmp(msg_type, "SPUP") == 0 || strcmp(msg_type, "SPDN") == 0 ||
            strcmp(msg_type, "TNLF") == 0 || strcmp(msg_type, "TNRT") == 0) {
            
            // Verificar permisos de administrador
            if (client->type != USER_ADMIN) {
                build_message(response, "CMER", "NO_PERMISSION");
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            } else {
                // Ejecutar comando
                execute_command(vehicle, msg_type, response);
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            }
        } 
        // ===== COMANDO: LISTAR USUARIOS =====
        else if (strcmp(msg_type, "LIST") == 0) {
            if (client->type != USER_ADMIN) {
                build_message(response, "CMER", "NO_PERMISSION");
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            } else {
                get_client_list_string(client_list, response);
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            }
        }
        // ===== COMANDO: DESCONECTAR =====
        else if (strcmp(msg_type, "DISC") == 0) {
            build_message(response, "DACK", "GOODBYE");
            send(client_socket, response, strlen(response), 0);
            log_message(log_file, log_lock, client->client_id, client_ip, client_port, "DISCONNECT");
            break;
        }
        // ===== MENSAJE INVÁLIDO =====
        else {
            build_message(response, "CERR", "INVALID_MESSAGE");
            send(client_socket, response, strlen(response), 0);
            log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
        }
    }
    
    // ========== FASE 3: LIMPIEZA ==========
    remove_client(client_list, client_socket);
    close(client_socket);
    free(data);
    
    return NULL;
}


// THREAD: ENVÍO DE TELEMETRÍA UDP CADA 10 SEGUNDOS

void* send_telemetry(void* arg) {
    ThreadData *data = (ThreadData*)arg;
    VehicleState *vehicle = data->vehicle;
    ClientList *client_list = data->client_list;
    
    // Crear socket UDP
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("Error creando socket UDP");
        return NULL;
    }
    
    // IMPORTANTE: Habilitar broadcast
    int broadcast = 1;
    if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("Error configurando broadcast");
    }
    
    udp_socket_fd = udp_sock;
    
    char buffer[BUFFER_SIZE];
    
    printf("[TELEMETRY] Thread iniciado. Enviando cada %d segundos.\n", TELEMETRY_INTERVAL);
    
    while (1) {
        sleep(TELEMETRY_INTERVAL);
        
        // Obtener telemetría actual
        get_telemetry_string(vehicle, buffer);
        
        // Enviar a todos los clientes activos
        pthread_mutex_lock(&client_list->lock);
        
        int sent_count = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_list->clients[i].active) {
                struct sockaddr_in client_addr = client_list->clients[i].addr;
                
                // CAMBIO CRÍTICO: Configurar puerto UDP correcto
                client_addr.sin_port = htons(5001);
                
                // Enviar por UDP
                ssize_t sent = sendto(udp_sock, buffer, strlen(buffer), 0,
                       (struct sockaddr*)&client_addr, sizeof(client_addr));
                
                if (sent > 0) {
                    sent_count++;
                    printf("[TELEMETRY] Enviado a %s:%d (%zd bytes)\n", 
                           inet_ntoa(client_addr.sin_addr), 
                           ntohs(client_addr.sin_port), 
                           sent);
                } else {
                    printf("[TELEMETRY] Error enviando a %s:%d\n",
                           inet_ntoa(client_addr.sin_addr),
                           ntohs(client_addr.sin_port));
                }
            }
        }
        
        pthread_mutex_unlock(&client_list->lock);
        
        printf("[TELEMETRY] Enviado a %d clientes: %s", sent_count, buffer);
    }
    
    close(udp_sock);
    return NULL;
}


// FUNCIÓN PRINCIPAL

int main(int argc, char *argv[]) {
    // Verificar argumentos
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <puerto> <archivo_log>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 5000 logs/server.log\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    char *log_filename = argv[2];
    
    // Configurar manejador de señales (Ctrl+C)
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Abrir archivo de log
    FILE *log_file = fopen(log_filename, "a");
    if (!log_file) {
        perror("Error abriendo archivo de log");
        return 1;
    }
    global_log_file = log_file;
    pthread_mutex_t log_lock;
    pthread_mutex_init(&log_lock, NULL);
    
    printf("===========================================\n");
    printf("  SERVIDOR VEHÍCULO AUTÓNOMO\n");
    printf("===========================================\n");
    
    // Inicializar vehículo
    VehicleState vehicle;
    init_vehicle(&vehicle);
    printf("[VEHICLE] Inicializado: Speed=%.1f, Battery=%d%%, Temp=%.1f°C, Dir=%s\n",
           vehicle.speed, vehicle.battery, vehicle.temperature, 
           direction_to_string(vehicle.direction));
    
    // Inicializar lista de clientes
    ClientList client_list;
    init_client_list(&client_list);
    printf("[CLIENTS] Lista inicializada (max: %d)\n", MAX_CLIENTS);
    
    // Crear socket TCP
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("Error creando socket TCP");
        return 1;
    }
    tcp_socket_fd = tcp_sock;
    
    // Permitir reutilización del puerto
    int opt = 1;
    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configurar dirección del servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Escuchar en todas las interfaces
    server_addr.sin_port = htons(port);
    
    // Bind (asociar socket a puerto)
    if (bind(tcp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(tcp_sock);
        return 1;
    }
    
    // Listen (escuchar conexiones)
    if (listen(tcp_sock, 10) < 0) {
        perror("Error en listen");
        close(tcp_sock);
        return 1;
    }
    
    printf("[SERVER] Escuchando en puerto %d (TCP)\n", port);
    printf("[LOG] Archivo: %s\n", log_filename);
    printf("===========================================\n");
    printf("Presiona Ctrl+C para detener el servidor\n");
    printf("===========================================\n\n");
    
    // Iniciar thread de telemetría
    pthread_t telemetry_thread;
    ThreadData *telemetry_data = malloc(sizeof(ThreadData));
    telemetry_data->vehicle = &vehicle;
    telemetry_data->client_list = &client_list;
    pthread_create(&telemetry_thread, NULL, send_telemetry, telemetry_data);
    pthread_detach(telemetry_thread);
    
    // Loop principal: aceptar clientes
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        printf("[SERVER] Esperando conexiones...\n");
        int client_socket = accept(tcp_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            perror("Error en accept");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[SERVER] Nueva conexión desde %s:%d\n", 
               client_ip, ntohs(client_addr.sin_port));
        
        // Crear thread para manejar este cliente
        ThreadData *thread_data = malloc(sizeof(ThreadData));
        thread_data->client_socket = client_socket;
        thread_data->client_addr = client_addr;
        thread_data->vehicle = &vehicle;
        thread_data->client_list = &client_list;
        thread_data->log_file = log_file;
        thread_data->log_lock = &log_lock;
        
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, thread_data);
        pthread_detach(client_thread);  // No necesitamos esperar por este thread
    }
    
    // Nunca llegamos aquí en ejecución normal
    close(tcp_sock);
    fclose(log_file);
    
    return 0;
}