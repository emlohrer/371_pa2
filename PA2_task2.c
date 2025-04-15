// PA2 - Task 2: UDP + Sequence Number + ARQ + Retransmission Support
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
#include <errno.h>

#define MESSAGE_SIZE 16
#define TIMEOUT_MS 100
#define DEFAULT_CLIENT_THREADS 4

char *server_ip;
int server_port;
int num_client_threads;
int num_requests;

typedef struct {
    int client_id;
    int sequence_number;
    char payload[MESSAGE_SIZE - 8];
} packet_t;

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    int client_id;
    long tx_cnt;
    long rx_cnt;
    long retransmissions;
    long long total_rtt;
} client_thread_data_t;

void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct timeval start, end;
    packet_t pkt, recv_pkt;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    for (int i = 0; i < num_requests; i++) {
        pkt.client_id = data->client_id;
        pkt.sequence_number = i;
        memcpy(pkt.payload, "ABCDEFGHIJKMLNOP", sizeof(pkt.payload));

        int acknowledged = 0;
        int attempts = 0;

        while (!acknowledged) {
            attempts++;
            data->tx_cnt++;
            gettimeofday(&start, NULL);

            sendto(data->socket_fd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&data->server_addr, sizeof(data->server_addr));

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(data->socket_fd, &readfds);

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = TIMEOUT_MS * 1000;

            int activity = select(data->socket_fd + 1, &readfds, NULL, NULL, &timeout);

            if (activity > 0 && FD_ISSET(data->socket_fd, &readfds)) {
                recvfrom(data->socket_fd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL);
                if (recv_pkt.sequence_number == i && recv_pkt.client_id == data->client_id) {
                    gettimeofday(&end, NULL);
                    acknowledged = 1;
                    data->rx_cnt++;
                    long long start_us = (long long)start.tv_sec * 1000000 + start.tv_usec;
                    long long end_us = (long long)end.tv_sec * 1000000 + end.tv_usec;
                    data->total_rtt += (end_us - start_us);
                }
            } else {
                data->retransmissions++;
            }
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
        thread_data[i].retransmissions = 0;
        thread_data[i].total_rtt = 0;

        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    long total_tx = 0, total_rx = 0, total_retx = 0;
    long long total_rtt = 0;

    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
        total_retx += thread_data[i].retransmissions;
        total_rtt += thread_data[i].total_rtt;
        close(thread_data[i].socket_fd);
    }

    printf("Total TX: %ld\n", total_tx);
    printf("Total RX: %ld\n", total_rx);
    printf("Retransmissions: %ld\n", total_retx);
    if (total_rx > 0) {
        printf("Avg RTT: %lld us\n", total_rtt / total_rx);
        printf("Loss Rate: %.2f%%\n", 100.0 * (total_tx - total_rx) / total_tx);
    }
}

void run_server() {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in srv_addr, cli_addr;
    socklen_t addr_len = sizeof(cli_addr);
    packet_t buffer;

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &srv_addr.sin_addr);
    bind(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));

    while (1) {
        int n = recvfrom(sock_fd, &buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&cli_addr, &addr_len);
        if (n > 0) {
            sendto(sock_fd, &buffer, sizeof(buffer), 0,
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
