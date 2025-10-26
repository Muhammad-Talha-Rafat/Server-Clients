#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>

#define PORT 8080
#define BUFFER_SIZE 1000
#define MAX_TASK_LIMIT 5
#define MAX_CLIENT_LIMIT 2

struct USER {
    int sock;
    char username[100];
    char password[100];
    int authorized;
};

struct TASK {
    struct USER* client;
    char command[100];
    char filename[100];
};

struct TASK_QUEUE {
    int front;
    int rear;
    struct TASK list[MAX_TASK_LIMIT];
};

int server;
struct sockaddr_in address;
struct TASK_QUEUE tasks = { .front = 0, .rear = 0 };
pthread_mutex_t queue_mutex;
sem_t clients;

int isFull(struct TASK_QUEUE* queue) {
    return queue->rear == MAX_TASK_LIMIT;
}

int isEmpty(struct TASK_QUEUE* queue) {
    return queue->front == queue->rear;
}

int enqueue(struct TASK_QUEUE* queue, struct TASK item) {
    if (isFull(queue)) return -1;
    queue->list[queue->rear++] = item;
    return 1;
}

int dequeue(struct TASK_QUEUE* queue, struct TASK* item) {
    if (isEmpty(queue)) return -1;
    *item = queue->list[queue->front++];
    if (queue->front == queue->rear) queue->front = queue->rear = 0;
    return 1;
}

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
    memset(buffer, 0, sizeof(buffer));
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
        task.client = client;
        memset(task.filename, 0, sizeof(task.filename));
        memset(message, 0, sizeof(message));
        sscanf(buffer, "%99s %99[^\n]", task.command, task.filename);

        if (strcmp(task.command, "LIST") == 0) {
            if (task.filename[0] != '\0') {
                snprintf(message, sizeof(message), "server > invalid format");
                send(client->sock, message, strlen(message), 0);
                printf("%s < %s\n", client->username, message);
            }
            else {
                pthread_mutex_lock(&queue_mutex);
                int temp = enqueue(&tasks, task);
                pthread_mutex_unlock(&queue_mutex);
                if (temp == -1) {
                    snprintf(message, sizeof(message), "server > Queue is full, please wait for a bit");
                    send(client->sock, message, strlen(message), 0);
                }
            }
        }
        else {
            if (task.filename[0] == '\0' || strchr(task.filename, ' ') != NULL) {
                snprintf(message, sizeof(message), "server > invalid format");
                send(client->sock, message, strlen(message), 0);
                printf("%s < %s\n", client->username, message);
            }
            else if (strcmp(task.command, "UPLOAD") == 0 || strcmp(task.command, "DOWNLOAD") == 0 || strcmp(task.command, "DELETE") == 0) {
                pthread_mutex_lock(&queue_mutex);
                int temp = enqueue(&tasks, task);
                pthread_mutex_unlock(&queue_mutex);
                if (temp == -1) {
                    snprintf(message, sizeof(message), "server > Queue is full, please wait for a bit");
                    send(client->sock, message, strlen(message), 0);
                }
            }
            else {
                snprintf(message, sizeof(message), "server > invalid command");
                send(client->sock, message, strlen(message), 0);
                printf("%s < %s\n", client->username, message);
            }
        }
    }
    close(client->sock);
}

void* CLIENT_HANDLER(void* arg) {
    struct USER* client = (struct USER*)arg;
    HANDLE_CLIENT(client);
    free(client);
    sem_post(&clients);
    return NULL;
}

void* TASK_PROCESSOR(void* arg) {
    while (1) {
        struct TASK task;
        int notEmpty = 0;

        pthread_mutex_lock(&queue_mutex);

        if (!isEmpty(&tasks)) {
            dequeue(&tasks, &task);
            notEmpty = 1;
        }

        pthread_mutex_unlock(&queue_mutex);

        if (notEmpty) {
            if (strcmp(task.command, "LIST") == 0)
                LIST(task.client);
            else if (strcmp(task.command, "UPLOAD") == 0)
                UPLOAD(task.client, &task);
            else if (strcmp(task.command, "DOWNLOAD") == 0)
                DOWNLOAD(task.client, &task);
            else if (strcmp(task.command, "DELETE") == 0)
                DELETE(task.client, &task);
            sleep(5);
        }
        else usleep(100000);
    }
    return NULL;
}

int main() {

    INIT(&server, &address);
    pthread_mutex_init(&queue_mutex, NULL);
    sem_init(&clients, 0, MAX_CLIENT_LIMIT);

    pthread_t task_thread;
    pthread_create(&task_thread, NULL, (void*)TASK_PROCESSOR, NULL);

    while (1) {
        struct USER* client = malloc(sizeof(struct USER));
        socklen_t addrlen = sizeof(address);

        client->sock = accept(server, (struct sockaddr*)&address, &addrlen);
        if (client->sock < 0) {
            perror("server error: couldn't accept client\n");
            free(client);
            continue;
        }

        if (sem_trywait(&clients) == -1) {
            char message[100];
            snprintf(message, sizeof(message), "server error > client queue is full, come back after a while");
            send(client->sock, message, strlen(message), 0);
            close(client->sock);
            free(client);
            continue;
        }

        pthread_t client_task_thread;
        pthread_create(&client_task_thread, NULL, CLIENT_HANDLER, client);
        pthread_detach(client_task_thread);
    }

    close(server);
    pthread_mutex_destroy(&queue_mutex);
    sem_destroy(&clients);
    return 0;    
}