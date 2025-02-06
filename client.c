#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];

    // Creare socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Eroare la crearea socket-ului");
        return EXIT_FAILURE;
    }

    // Configurare adresa server
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("Adresa IP invalida");
        close(client_socket);
        return EXIT_FAILURE;
    }

    // Conectare la server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Conectare esuata");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Conectat la server.\n");

    while (1) {
        printf("Introduceti comanda: ");
        if (fgets(send_buffer, BUFFER_SIZE, stdin) == NULL) {
            perror("Eroare la citirea comenzii");
            break;
        }
        send_buffer[strcspn(send_buffer, "\n")] = '\0';

        // Trimitere mesaj
        if (send(client_socket, send_buffer, strlen(send_buffer), 0) < 0) {
            perror("Eroare la trimiterea mesajului");
            break;
        }

        // Citire raspuns
        ssize_t bytes_received = recv(client_socket, recv_buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("Eroare la citirea raspunsului");
            break;
        } else if (bytes_received == 0) {
            printf("Serverul a inchis conexiunea.\n");
            break;
        }
        recv_buffer[bytes_received] = '\0';
        printf("Raspuns: %s\n", recv_buffer);

        // Verificare comanda de iesire
        if (strcmp(send_buffer, "BYE") == 0) {
            printf("Deconectare...\n");
            break;
        }
    }

    // Inchidere socket
    close(client_socket);
    return EXIT_SUCCESS;
}

