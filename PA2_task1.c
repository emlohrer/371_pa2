// PA2 - Task 1: UDP "Stop-and-Wait" + Packet Loss Tracking
// Team member 1: Emma Lohrer
// Team member 2: Jp McNerney
// Team member 3: Clayton Curry

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>

#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip;
int server_port;
int num_client_threads;
int num_requests;

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    int client_id;
    long tx_cnt;
    long rx_cnt;
} client_thread_data_t;

void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP";
    char recv_buf[MESSAGE_SIZE];
    socklen_t addr_len = sizeof(struct sockaddr_in);

    for (int i = 0; i < num_requests; i++) {
        data->tx_cnt++;
        sendto(data->socket_fd, send_buf, MESSAGE_SIZE, 0,
               (struct sockaddr *)&data->server_addr, sizeof(data->server_addr));

        int n = recvfrom(data->socket_fd, recv_buf, MESSAGE_SIZE, 0, NULL, NULL);
        if (n == MESSAGE_SIZE) {
            data->rx_cnt++;
        }
    }
    pthread_exit(NULL);
}

void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];

    for (int i = 0; i < num_client_threads; i++) {
        thread_data[i].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        thread_data[i].server_addr.sin_family = AF_INET;
        thread_data[i].server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip, &thread_data[i].server_addr.sin_addr);

        thread_data[i].client_id = i;
        thread_data[i].tx_cnt = 0;
        thread_data[i].rx_cnt = 0;

        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    long total_tx = 0, total_rx = 0;
    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
        close(thread_data[i].socket_fd);
    }

    printf("Total TX: %ld\n", total_tx);
    printf("Total RX: %ld\n", total_rx);
    printf("Packet Loss: %ld\n", total_tx - total_rx);
    if (total_tx > 0) {
        printf("Loss Rate: %.2f%%\n", 100.0 * (total_tx - total_rx) / total_tx);
    }
}

void run_server() {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in srv_addr, cli_addr;
    socklen_t addr_len = sizeof(cli_addr);
    char buffer[MESSAGE_SIZE];

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &srv_addr.sin_addr);
    bind(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));

    while (1) {
        int n = recvfrom(sock_fd, buffer, MESSAGE_SIZE, 0,
                         (struct sockaddr *)&cli_addr, &addr_len);
        if (n == MESSAGE_SIZE) {
            sendto(sock_fd, buffer, MESSAGE_SIZE, 0,
                   (struct sockaddr *)&cli_addr, addr_len);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("Usage: %s <server|client> <server_ip> <server_port> <num_threads> <num_requests>\n", argv[0]);
        return -1;
    }

    server_ip = argv[2];
    server_port = atoi(argv[3]);
    num_client_threads = atoi(argv[4]);
    num_requests = atoi(argv[5]);

    if (strcmp(argv[1], "server") == 0) run_server();
    else if (strcmp(argv[1], "client") == 0) run_client();
    else printf("Invalid usage.\n");
    return 0;
}
