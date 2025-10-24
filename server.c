#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1000

struct USER {
    int sock;
    char username[100];
    char password[100];
    int authorized;
};

struct TASK {
    char client[100];
    char command[100];
    char filename[100];
};

void INIT(int* server, struct sockaddr_in* address) {

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
    FILE *file = fopen("assets/users.txt", "r");
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
        file = fopen("assets/users.txt", "a");
        fprintf(file, "username: %s, password: %s\n", username, password);
        fclose(file);
        char folder[100];
        snprintf(folder, sizeof(folder), "assets/users/%s", username);
        mkdir(folder, 0777);
        return 1;
    }
    return 0;
}

void UPLOAD(struct USER* client, struct TASK* task) {
    char source[256] = {0};
    char destinition[256] = {0};
    char buffer[BUFFER_SIZE] = {0};

    snprintf(source, sizeof(source), "assets/files/%s", task->filename);
    snprintf(destinition, sizeof(destinition), "assets/users/%s/%s", client->username, task->filename);

    FILE* src = fopen(source, "rb");
    if (!src) {
        snprintf(buffer, sizeof(buffer), "server > Server doesn't contain the requested file");
        send(client->sock, buffer, strlen(buffer), 0);
        return;
    }

    FILE* dest = fopen(destinition, "rb");
    if (dest) {
        snprintf(buffer, sizeof(buffer), "server > File already exists in your directory");
        send(client->sock, buffer, strlen(buffer), 0);
        fclose(src);
        return;
    }

    dest = fopen(destinition, "wb");
    size_t size;
    while ((size = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, size, dest);    

    fclose(src);
    fclose(dest);

    snprintf(buffer, sizeof(buffer), "server > File successfully uploaded");
    send(client->sock, buffer, strlen(buffer), 0);
}

void DOWNLOAD(struct USER* client, struct TASK* task) {
    char filepath[256] = {0};
    char buffer[BUFFER_SIZE] = {0};

    snprintf(filepath, sizeof(filepath), "assets/users/%s/%s", client->username, task->filename);

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        snprintf(buffer, sizeof(buffer), "server > File could not found");
        send(client->sock, buffer, strlen(buffer), 0);
        return;
    }

    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client->sock, buffer, bytesRead, 0);
    }

    fclose(file);
}

void DELETE(struct USER* client, struct TASK* task) {
    char filepath[256] = {0};
    char buffer[BUFFER_SIZE] = {0};

    snprintf(filepath, sizeof(filepath), "assets/users/%s/%s", client->username, task->filename);

    if (access(filepath, F_OK) != 0) {
        snprintf(buffer, sizeof(buffer), "server > File could not found");
        send(client->sock, buffer, strlen(buffer), 0);
        return;
    }

    if (remove(filepath) == 0) {
        snprintf(buffer, sizeof(buffer), "server > File successfully deleted");
    } else {
        snprintf(buffer, sizeof(buffer), "server > File could not delete. Try again.");
    }
    send(client->sock, buffer, strlen(buffer), 0);
}

void LIST(struct USER* client) {
    char path[256];
    snprintf(path, sizeof(path), "assets/users/%s", client->username);

    DIR* dir = opendir(path);

    struct dirent* entry;
    char buffer[BUFFER_SIZE] = {0};
    char line[512];

    snprintf(buffer, sizeof(buffer), "name\t\tsize\n-------------------------\n");

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0) {
            snprintf(line, sizeof(line), "%s\t%ld bytes\n", entry->d_name, st.st_size);
            strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
        }
    }

    closedir(dir);

    send(client->sock, buffer, strlen(buffer), 0);
}

void HANDLE_CLIENT(struct USER* client) {
    client->authorized = 0;
    char buffer[BUFFER_SIZE] = {0};

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(client->sock, buffer, BUFFER_SIZE);
        if (bytes <= 0) {
            printf("client error: connection lost\n");
            break;
        }

        if (strcmp(buffer, "EXIT") == 0 || strcmp(buffer, "exit") == 0) {
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

        struct TASK task;
        char message[100] = {0};
        strcpy(task.client, client->username);
        memset(task.filename, 0, sizeof(task.filename));
        memset(message, 0, sizeof(message));
        sscanf(buffer, "%99s %99[^\n]", task.command, task.filename);

        if (strcmp(task.command, "LIST") == 0) {
            if (task.filename[0] != '\0') {
                snprintf(message, sizeof(message), "server: invalid format");
                send(client->sock, message, strlen(message), 0);
                printf("%s < %s\n", client->username, message);
            }
            else LIST(client);
        }
        else {
            if (task.filename[0] == '\0' || strchr(task.filename, ' ') != NULL) {
                snprintf(message, sizeof(message), "server: invalid format");
                send(client->sock, message, strlen(message), 0);
                printf("%s < %s\n", client->username, message);
            }
            else if (strcmp(task.command, "UPLOAD") == 0) UPLOAD(client, &task);
            else if (strcmp(task.command, "DOWNLOAD") == 0) DOWNLOAD(client, &task);
            else if (strcmp(task.command, "DELETE") == 0) DELETE(client, &task);
            else {
                snprintf(message, sizeof(message), "server: invalid command");
                send(client->sock, message, strlen(message), 0);
                printf("%s < %s\n", client->username, message);
            }
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