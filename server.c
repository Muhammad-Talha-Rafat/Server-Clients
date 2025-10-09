#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1000

void INIT(int *server, int *client, struct sockaddr_in *address, socklen_t *addrlen) {

    *server = socket(AF_INET, SOCK_STREAM, 0);
    if (*server < 0) {
        perror("server error: socket couldn't create\n");
        exit(EXIT_FAILURE);
    }

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    if (bind(*server, (struct sockaddr*)address, *addrlen) < 0) {
        perror("server error: server couldn't bind\n");
        close(*server);
        exit(EXIT_FAILURE);
    }

    if (listen(*server, 3) < 0) {
        perror("server error: server couldn't listen\n");
        close(*server);
        exit(EXIT_FAILURE);
    }

    *client = accept(*server, (struct sockaddr*)address, addrlen);
    if (*client < 0) {
        perror("server error: couldn't accept client\n");
        close(*server);
        exit(EXIT_FAILURE);
    }
}

void UPLOAD(int *client) {
    char* buffer = "UPLOAD";
    send(*client, buffer, strlen(buffer), 0);
    printf("server > %s\n", buffer);
}

void DOWNLOAD(int *client) {
    char* buffer = "DOWNLOAD";
    send(*client, buffer, strlen(buffer), 0);
    printf("server > %s\n", buffer);
}

void DELETE(int *client) {
    char* buffer = "DELETE";
    send(*client, buffer, strlen(buffer), 0);
    printf("server > %s\n", buffer);
}

void LIST(int *client) {
    char* buffer = "LIST";
    send(*client, buffer, strlen(buffer), 0);
    printf("server > %s\n", buffer);
}

int main() {
    int server, client;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    INIT(&server, &client, &address, &addrlen);

    while (1) {
        char buffer[BUFFER_SIZE] = {0};
        read(client, buffer, BUFFER_SIZE);

        if (strcmp(buffer, "UPLOAD") == 0) UPLOAD(&client);
        else if (strcmp(buffer, "DOWNLOAD") == 0) DOWNLOAD(&client);
        else if (strcmp(buffer, "DELETE") == 0) DELETE(&client);
        else if (strcmp(buffer, "LIST") == 0) LIST(&client);
        else {
            char* message = "invalid command";
            if (strcmp(buffer, "EXIT") == 0) {
                message = "disconnected";
                printf("server > %s\n", message);
                break;
            }
            send(client, message, strlen(message), 0);
            printf("server > %s\n", message);
        }
    }

    close(server);
    return 0;    
}