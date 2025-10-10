#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1000

struct USER {
    int sock;
    char username[100];
    char password[100];
    int authorized;
};


void INIT(int *server, struct sockaddr_in *address) {

    *server = socket(AF_INET, SOCK_STREAM, 0);
    if (*server < 0) {
        perror("server error: socket couldn't create\n");
        exit(EXIT_FAILURE);
    }

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    socklen_t addrlen = sizeof(*address);

    if (bind(*server, (struct sockaddr*)address, addrlen) < 0) {
        perror("server error: server couldn't bind\n");
        close(*server);
        exit(EXIT_FAILURE);
    }

    if (listen(*server, 3) < 0) {
        perror("server error: server couldn't listen\n");
        close(*server);
        exit(EXIT_FAILURE);
    }
}

int AUTHORIZE(char* action, char* username, char* password) {
    char line[256], name[100], key[100];
    FILE *file = fopen("users.txt", "r");
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "username: %99[^,], password: %99s", name, key) == 2) {
            if (strcmp(name, username) == 0 && strcmp(key, password) == 0) {
                fclose(file);
                return (strcmp(action, "signup") == 0) ? 0 : 1;
            }
        }
    }
    fclose(file);
    if (strcmp(action, "signup") == 0) {
        file = fopen("users.txt", "a");
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

void HANDLE_CLIENT(struct USER *client) {
    client->authorized = 0;
    char buffer[BUFFER_SIZE] = {0};

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(client->sock, buffer, BUFFER_SIZE);
        if (bytes <= 0) {
            printf("client error: connection lost\n");
            break;
        }

        if (strcmp(buffer, "EXIT") == 0) {
            printf("%s disconnected\n", client->username[0] ? client->username : "client");
            break;
        }

        if (!client->authorized) {
            char action[100];
            sscanf(buffer, "%s %s %s", action, client->username, client->password);
            client->authorized = AUTHORIZE(action, client->username, client->password);
            if (client->authorized) {
                send(client->sock, "authorized", strlen("authorized"), 0);
                printf("Welcome %s!\n", client->username);
            }
            else send(client->sock, "invalid credentials", strlen("invalid credentials"), 0);
            continue;
        }


        if (strcmp(buffer, "UPLOAD") == 0) UPLOAD(&client->sock);
        else if (strcmp(buffer, "DOWNLOAD") == 0) DOWNLOAD(&client->sock);
        else if (strcmp(buffer, "DELETE") == 0) DELETE(&client->sock);
        else if (strcmp(buffer, "LIST") == 0) LIST(&client->sock);
        else {
            send(client->sock, "invalid command", strlen("invalid command"), 0);
            printf("%s < invalid command\n", client->username);
        }
    }
    close(client->sock);
}

int main() {
    int server;
    struct sockaddr_in address;

    INIT(&server, &address);

    while (1) {
        struct USER client;
        socklen_t addrlen = sizeof(address);
        client.sock = accept(server, (struct sockaddr*)&address, &addrlen);
        if (client.sock < 0) {
            perror("server error: couldn't accept client\n");
            continue;
        }

        HANDLE_CLIENT(&client);
    }

    close(server);
    return 0;    
}