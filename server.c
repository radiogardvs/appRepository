#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <cjson/cJSON.h>

#define SERVER_PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Functie pentru citirea fisierului JSON si returnarea obiectului cJSON
cJSON* readapps(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Nu se poate deschide fisierul JSON");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = (char*)malloc(length + 1);
    if (data == NULL) {
        perror("Eroare la alocarea memoriei");
        fclose(file);
        return NULL;
    }

    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cJSON* json = cJSON_Parse(data);
    free(data);

    if (json == NULL) {
        fprintf(stderr, "Eroare la parsarea JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }

    return json;
}

// Funcție pentru gestionarea comenzilor primite de la client
void process_command(const char *command, char *response) {
    if (strcmp(command, "HELLO") == 0) {
        strcpy(response, "Comanda recunoscuta: HELLO");
        return;
    }
    if (strcmp(command, "BYE") == 0) {
        strcpy(response, "Comanda recunoscuta: BYE");
        return;
    }
    if (strcmp(command, "APPS") == 0) {
        cJSON* json = readapps("aplicatii.json");
        if (json == NULL) {
            strcpy(response, "Eroare la citirea aplicatiilor");
            return;
        }

        cJSON* aplicatii = cJSON_GetObjectItem(json, "aplicatii");
        if (!cJSON_IsArray(aplicatii)) {
            strcpy(response, "Format JSON invalid");
            cJSON_Delete(json);
            return;
        }

        char buffer[BUFFER_SIZE] = "";
        cJSON* aplicatie;
        cJSON_ArrayForEach(aplicatie, aplicatii) {
            const cJSON* nume = cJSON_GetObjectItem(aplicatie, "nume");
            const cJSON* versiune = cJSON_GetObjectItem(aplicatie, "versiune");
            const cJSON* dezvoltator = cJSON_GetObjectItem(aplicatie, "dezvoltator");
            const cJSON* descriere = cJSON_GetObjectItem(aplicatie, "descriere");
            const cJSON* data_lansare = cJSON_GetObjectItem(aplicatie, "data_lansare");
            const cJSON* licenta = cJSON_GetObjectItem(aplicatie, "licenta");

            if (cJSON_IsString(nume) && cJSON_IsString(versiune) && cJSON_IsString(dezvoltator) &&
                cJSON_IsString(descriere) && cJSON_IsString(data_lansare) && cJSON_IsString(licenta)) {
                char linie[256];
                snprintf(linie, sizeof(linie), "Nume: %s,\n Versiune: %s,\n Dezvoltator: %s,\n Descriere: %s, \n Data lansare: %s,\n Licenta: %s\n",
                         nume->valuestring, versiune->valuestring, dezvoltator->valuestring,
                         descriere->valuestring, data_lansare->valuestring, licenta->valuestring);
                strncat(buffer, linie, sizeof(buffer) - strlen(buffer) - 1);
            }
        }

        strncpy(response, buffer, BUFFER_SIZE - 1);
        cJSON_Delete(json);
        return;
    } else {
        strcpy(response, "Comanda necunoscuta");
        return;
    }
}


int main() {
    int server_socket, client_socket, client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in server_address, client_address;
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len;
    int max_sd, sd, activity;

    // Creare socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Eroare la crearea socket-ului server");
        exit(EXIT_FAILURE);
    }

    // Configurare adresa 
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    // Legare socket la adresa
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Eroare la bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Ascultare conexiuni
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Eroare la listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Serverul asculta pe portul %d\n", SERVER_PORT);

    while (1) {
        // Initializare set descriptori
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        max_sd = server_socket;

        // Adaugare socket-uri client in set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0)
                FD_SET(sd, &read_fds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Asteptare activitate 
        activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("Eroare la select");
        }

        // Verificare conexiuni noi
        if (FD_ISSET(server_socket, &read_fds)) {
            addr_len = sizeof(client_address);
            client_socket = accept(server_socket, (struct sockaddr *)&client_address, &addr_len);
            if (client_socket < 0) {
                perror("Eroare la accept");
                continue;
            }
            printf("Conexiune noua: socket fd %d, IP %s, PORT %d\n",
                   client_socket, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

            // Adaugare client in lista de socket-uri
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_socket;
                    break;
                }
            }
        }

        // Gestionare activitate pe socket-uri existente
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &read_fds)) {
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Clientul a deconectat
                    getpeername(sd, (struct sockaddr *)&client_address, &addr_len);
                    printf("Host deconectat, IP %s, PORT %d\n",
                           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    char response[BUFFER_SIZE];
                    process_command(buffer, response);
                    send(sd, response, strlen(response), 0);
                }
            }
        }
    }

    return 0;
}

