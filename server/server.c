
// server.c
// Servidor que utiliza vehicle.h / vehicle.c
// Compilar: gcc -pthread -o server server.c vehicle.c -Wall
// Uso: ./server <tcp_port> <logfile>

#include "vehicle.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>

int tcp_socket_fd = -1;
int udp_socket_fd = -1;
FILE *global_log_file = NULL;

static void safe_close_int(int *fd) {
    if (fd && *fd != -1) {
        close(*fd);
        *fd = -1;
    }
}

void signal_handler_server(int signum) {
    printf("\n[SERVER] Señal %d recibida, cerrando...\n", signum);
    if (global_log_file) {
        fflush(global_log_file);
        fclose(global_log_file);
        global_log_file = NULL;
    }
    safe_close_int(&tcp_socket_fd);
    safe_close_int(&udp_socket_fd);
    exit(0);
}

/* Thread: handler de cliente (TCP) */
void *handle_client_thread(void *arg) {
    ThreadData *td = (ThreadData*)arg;
    if (!td) return NULL;

    int client_socket = td->client_socket;
    struct sockaddr_in client_addr = td->client_addr;
    VehicleState *vehicle = td->vehicle;
    ClientList *client_list = td->client_list;
    FILE *log_file = td->log_file;
    pthread_mutex_t *log_lock = td->log_lock;

    char buf[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    // Recibir primer mensaje (handshake)
    ssize_t n = recv(client_socket, buf, BUFFER_SIZE - 1, 0);
    if (n <= 0) {
        close(client_socket);
        free(td);
        return NULL;
    }
    buf[n] = '\0';
    log_message(log_file, log_lock, "NEW", client_ip, client_port, buf);

    char msg_type[32] = {0};
    char msg_data[BUFFER_SIZE] = {0};
    parse_message(buf, msg_type, msg_data);

    char response[BUFFER_SIZE] = {0};

    if (strcmp(msg_type, "CONN") != 0) {
        build_message(response, "CERR", "INVALID_MESSAGE");
        send(client_socket, response, strlen(response), 0);
        log_message(log_file, log_lock, "BAD_HANDSHAKE", client_ip, client_port, response);
        close(client_socket);
        free(td);
        return NULL;
    }

    // parse msg_data: OBSERVER:udp or ADMIN:pass:udp
    UserType utype = USER_OBSERVER;
    int udp_port = 0;
    char tmp[BUFFER_SIZE];
    strncpy(tmp, msg_data, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';

    if (strncmp(tmp, "ADMIN:", 6) == 0) {
        char *rest = tmp + 6;
        char *p = strchr(rest, ':');
        char passwd[128];
        if (p) { *p = '\0'; strncpy(passwd, rest, sizeof(passwd)-1); passwd[sizeof(passwd)-1] = '\0'; udp_port = atoi(p+1); }
        else { strncpy(passwd, rest, sizeof(passwd)-1); passwd[sizeof(passwd)-1] = '\0'; }
        if (strcmp(passwd, ADMIN_PASSWORD) != 0) {
            build_message(response, "CERR", "INVALID_CREDENTIALS");
            send(client_socket, response, strlen(response), 0);
            log_message(log_file, log_lock, "AUTH_FAIL", client_ip, client_port, response);
            close(client_socket);
            free(td);
            return NULL;
        }
        utype = USER_ADMIN;
    } else if (strncmp(tmp, "OBSERVER", 8) == 0) {
        char *p = strchr(tmp, ':');
        if (p) udp_port = atoi(p+1);
        utype = USER_OBSERVER;
    } else {
        build_message(response, "CERR", "INVALID_MESSAGE");
        send(client_socket, response, strlen(response), 0);
        log_message(log_file, log_lock, "BAD_HANDSHAKE_FMT", client_ip, client_port, response);
        close(client_socket);
        free(td);
        return NULL;
    }

    // registrar cliente
    int idx = add_client(client_list, client_socket, client_addr, utype);
    if (idx < 0) {
        build_message(response, "CERR", "MAX_CLIENTS_REACHED");
        send(client_socket, response, strlen(response), 0);
        log_message(log_file, log_lock, "MAX_CLIENTS", client_ip, client_port, response);
        close(client_socket);
        free(td);
        return NULL;
    }
    // guardar puerto UDP si existe
    if (udp_port > 0) {
        pthread_mutex_lock(&client_list->lock);
        client_list->clients[idx].udp_port = udp_port;
        pthread_mutex_unlock(&client_list->lock);
    }
    ClientInfo *client = find_client(client_list, client_socket);
    if (!client) {
        build_message(response, "CERR", "INTERNAL_ERROR");
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        free(td);
        return NULL;
    }

    build_message(response, "CACK", client->client_id);
    send(client_socket, response, strlen(response), 0);
    log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);

    // Loop principal de comandos
    while (1) {
        ssize_t r = recv(client_socket, buf, BUFFER_SIZE - 1, 0);
        if (r <= 0) {
            log_message(log_file, log_lock, client->client_id, client_ip, client_port, "DISCONNECTED");
            break;
        }
        buf[r] = '\0';
        log_message(log_file, log_lock, client->client_id, client_ip, client_port, buf);

        char in_type[32] = {0};
        char in_data[BUFFER_SIZE] = {0};
        parse_message(buf, in_type, in_data);

        if (strcmp(in_type, "SPUP") == 0 || strcmp(in_type, "SPDN") == 0 ||
            strcmp(in_type, "TNLF") == 0 || strcmp(in_type, "TNRT") == 0) {

            if (client->type != USER_ADMIN) {
                build_message(response, "CMER", "NO_PERMISSION");
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            } else {
                execute_command(vehicle, in_type, response);
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            }

        } else if (strcmp(in_type, "LIST") == 0) {
            if (client->type != USER_ADMIN) {
                build_message(response, "CMER", "NO_PERMISSION");
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            } else {
                char listbuf[BUFFER_SIZE];
                get_client_list_string(client_list, listbuf);
                build_message(response, "LIST", listbuf);
                send(client_socket, response, strlen(response), 0);
                log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
            }
        } else if (strcmp(in_type, "DISC") == 0) {
            build_message(response, "DACK", "GOODBYE");
            send(client_socket, response, strlen(response), 0);
            log_message(log_file, log_lock, client->client_id, client_ip, client_port, "DISCONNECT");
            break;
        } else {
            build_message(response, "CERR", "INVALID_MESSAGE");
            send(client_socket, response, strlen(response), 0);
            log_message(log_file, log_lock, client->client_id, client_ip, client_port, response);
        }
    }

    remove_client(client_list, client_socket);
    close(client_socket);
    free(td);
    return NULL;
}

/* Thread: enviar telemetría por UDP a cada cliente registrado */
void *telemetry_thread(void *arg) {
    ThreadData *td = (ThreadData*)arg;
    if (!td) return NULL;
    VehicleState *vehicle = td->vehicle;
    ClientList *client_list = td->client_list;
    FILE *log_file = td->log_file;
    pthread_mutex_t *log_lock = td->log_lock;

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("socket UDP");
        return NULL;
    }
    udp_socket_fd = udp_sock;

    char telebuf[BUFFER_SIZE];

    while (1) {
        sleep(TELEMETRY_INTERVAL);
        // generar telemetría
        get_telemetry_string(vehicle, telebuf);

        pthread_mutex_lock(&client_list->lock);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!client_list->clients[i].active) continue;
            ClientInfo *c = &client_list->clients[i];
            struct sockaddr_in dest = c->addr;
            int target_port = (c->udp_port > 0) ? c->udp_port : UDP_FALLBACK_PORT;
            dest.sin_port = htons(target_port);
            ssize_t sent = sendto(udp_sock, telebuf, strlen(telebuf), 0, (struct sockaddr*)&dest, sizeof(dest));
            if (sent < 0) {
                char ipbuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &c->addr.sin_addr, ipbuf, sizeof(ipbuf));
                char emsg[256];
                snprintf(emsg, sizeof(emsg), "UDP_SEND_ERR: %s", strerror(errno));
                log_message(log_file, log_lock, c->client_id, ipbuf, target_port, emsg);
            } else {
                // opcional: log de envío si se desea (desactivado por defecto)
            }
        }
        pthread_mutex_unlock(&client_list->lock);
    }

    close(udp_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <tcp_port> <log_file>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    char *log_filename = argv[2];

    srand((unsigned)time(NULL));

    // abrir log
    FILE *log_file = fopen(log_filename, "a");
    if (!log_file) {
        perror("fopen log");
        return 1;
    }
    global_log_file = log_file;

    signal(SIGINT, signal_handler_server);
    signal(SIGTERM, signal_handler_server);

    pthread_mutex_t log_lock;
    pthread_mutex_init(&log_lock, NULL);

    printf("===========================================\n");
    printf("  SERVIDOR VEHÍCULO AUTÓNOMO (C) \n");
    printf("===========================================\n");

    VehicleState vehicle;
    init_vehicle(&vehicle);

    ClientList client_list;
    init_client_list(&client_list);

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("socket");
        fclose(log_file);
        return 1;
    }
    tcp_socket_fd = tcp_sock;
    int opt = 1;
    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(tcp_sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind");
        close(tcp_sock);
        fclose(log_file);
        return 1;
    }

    if (listen(tcp_sock, 16) < 0) {
        perror("listen");
        close(tcp_sock);
        fclose(log_file);
        return 1;
    }

    // iniciar telemetry thread
    pthread_t tid_tel;
    ThreadData *td = malloc(sizeof(ThreadData));
    td->vehicle = &vehicle;
    td->client_list = &client_list;
    td->log_file = log_file;
    td->log_lock = &log_lock;
    if (pthread_create(&tid_tel, NULL, telemetry_thread, td) != 0) {
        perror("pthread_create telemetry");
        free(td);
        close(tcp_sock);
        fclose(log_file);
        return 1;
    }
    pthread_detach(tid_tel);

    // accept loop
    while (1) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int client_sock = accept(tcp_sock, (struct sockaddr*)&cli, &len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }
        ThreadData *tdata = malloc(sizeof(ThreadData));
        tdata->client_socket = client_sock;
        tdata->client_addr = cli;
        tdata->vehicle = &vehicle;
        tdata->client_list = &client_list;
        tdata->log_file = log_file;
        tdata->log_lock = &log_lock;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client_thread, tdata) != 0) {
            perror("pthread_create client");
            close(client_sock);
            free(tdata);
            continue;
        }
        pthread_detach(tid);
    }

    close(tcp_sock);
    fclose(log_file);
    return 0;
}

