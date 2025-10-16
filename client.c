#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1000


int main() {
    int server = 0;
    struct sockaddr_in address;
    int authorized = 0;

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("client error: socket couldn't create");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0) {
        perror("client error: invalid address");
        close(server);
        exit(EXIT_FAILURE);
    }

    if (connect(server, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("client error: server couldn't connect");
        close(server);
        exit(EXIT_FAILURE);
    }

    while (1) {
        char message[BUFFER_SIZE], username[100];
        char buffer[BUFFER_SIZE] = {0};

        if (!authorized) {
            printf("authorize > ");
            fgets(message, BUFFER_SIZE, stdin);
            message[strcspn(message, "\n")] = '\0';

            if (strcmp(message, "EXIT") == 0 || strcmp(message, "exit") == 0) break;

            char action[100], password[100];
            int parts = sscanf(message, "%s %s %s", action, username, password);

            if (parts == 3 && (strcmp(action, "login") == 0 || strcmp(action, "signup") == 0)) {
                send(server, message, strlen(message), 0);
                read(server, buffer, BUFFER_SIZE);
                if (strcmp(buffer, "authorized") == 0) authorized = 1;
                else {
                    printf("auth error: %s!\n", buffer);
                    continue;
                }
            }
            else {
                printf("client error: invalid format\n");
                continue;
            }
        }

        printf("%s > ", username);
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = '\0';

        send(server, message, strlen(message), 0);
        if (strcmp(message, "EXIT") == 0) break;
    }

    close(server);
    return 0;
}
