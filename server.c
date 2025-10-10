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

int AUTHORIZE(char* action, char* username, char* password) {
    if (strcmp(action, "login") == 0) {
        char line[256], u[100], p[100];
        FILE *file = fopen("users.txt", "r");
        while (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "username: %99[^,], password: %99s", u, p) == 2) {
                if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
                    fclose(file);
                    return 1;
                }
            }
        }
        fclose(file);
    }
    else {
        FILE *file = fopen("users.txt", "a");
        fprintf(file, "username: %s, password: %s\n", username, password);
        fclose(file);
        return 1;
    }
    return 0;
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

struct USER {
    int sock;
    char username[100];
    char password[100];
    int authorized;
};

int main() {
    int server;
    struct USER client;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    INIT(&server, &client.sock, &address, &addrlen);
    client.authorized = 0;

    while (1) {
        char buffer[BUFFER_SIZE] = {0};
        read(client.sock, buffer, BUFFER_SIZE);

        if (strcmp(buffer, "EXIT") == 0) {
            printf("client disconnected\n");
            break;
        }

        while (!client.authorized) {
            char action[100];
            sscanf(buffer, "%s %s %s", action, client.username, client.password);
            client.authorized = AUTHORIZE(action, client.username, client.password);
            if (client.authorized) {
                send(client.sock, "authorized", strlen("authorized"), 0);
                printf("Welcome %s!\n", client.username);
                break;
            }
        }

        if (strcmp(buffer, "UPLOAD") == 0) UPLOAD(&client.sock);
        else if (strcmp(buffer, "DOWNLOAD") == 0) DOWNLOAD(&client.sock);
        else if (strcmp(buffer, "DELETE") == 0) DELETE(&client.sock);
        else if (strcmp(buffer, "LIST") == 0) LIST(&client.sock);
        else {
            char* message = "invalid command";
            if (strcmp(buffer, "EXIT") == 0) {
                message = "disconnected";
                printf("server > %s\n", message);
                break;
            }
            send(client.sock, message, strlen(message), 0);
            printf("server > %s\n", message);
        }
    }

    close(server);
    return 0;    
}