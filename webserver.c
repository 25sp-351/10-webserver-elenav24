#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct Headers {
    char *content_type;
    char *content_length;
    struct Headers *next;
} Headers;

typedef struct hreq {
    char *method;
    char *path;
    char* version;
    Headers* headers;
    char* body;
    int body_length;

    void (*print)(struct hreq* req);
    void (*free)(struct hreq* req);

} Request;

void handle_static(int client_socket, const char *url) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), ".%s", url);

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        const char *response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 17\r\n\r\n"
            "404 Not Found\n";
        write(client_socket, response, strlen(response));
        return;
    }

    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    const char *mime_type = "application/octet-stream";
    if (strstr(file_path, ".html"))
        mime_type = "text/html";
    else if (strstr(file_path, ".jpg") || strstr(file_path, ".jpeg"))
        mime_type = "image/jpeg";
    else if (strstr(file_path, ".png"))
        mime_type = "image/png";
    else if (strstr(file_path, ".css"))
        mime_type = "text/css";
    else if (strstr(file_path, ".js"))
        mime_type = "application/javascript";
    else if (strstr(file_path, ".pdf"))
        mime_type = "application/pdf";
    else if (strstr(file_path, ".txt"))
        mime_type = "text/plain";
    else if (strstr(file_path, ".mp4"))
        mime_type = "video/mp4";
    else if (strstr(file_path, ".mp3"))
        mime_type = "audio/mpeg";

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lld\r\n\r\n",
             mime_type, (long long)file_size);
    write(client_socket, header, strlen(header));

    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(file_fd, buffer, sizeof(buffer))) > 0)
        write(client_socket, buffer, bytes);

    close(file_fd);
}

void handle_calc(int client_socket, const char *url) {
    int num1, num2, result;
    char operation[16];

    if (sscanf(url, "/calc/%15[^/]/%d/%d", operation, &num1, &num2) != 3) {
        const char *response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "400 Bad Request\n";
        write(client_socket, response, strlen(response));
        return;
    }

    if (strcmp(operation, "add") == 0) {
        result = num1 + num2;
    } else if (strcmp(operation, "mul") == 0) {
        result = num1 * num2;
    } else if (strcmp(operation, "div") == 0) {
        if (num2 == 0) {
            const char *response =
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n\r\n"
                "Division by zero\n";
            write(client_socket, response, strlen(response));
            return;
        }
        result = num1 / num2;
    } else {
        const char *response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Invalid operation\n";
        write(client_socket, response, strlen(response));
        return;
    }

    char body[256];
    snprintf(body, sizeof(body),
             "<html><body><h1>Result: %d</h1></body></html>", result);

    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n\r\n",
             strlen(body));    

    write(client_socket, header, strlen(header));
    write(client_socket, body, strlen(body));
}

// function to handle client requests by creating a new thread
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_socket);
        return NULL;
    }

    printf("Received request:\n%s\n", buffer);

    char *method = strtok(buffer, " ");
    char *url    = strtok(NULL, " ");

    if (!method || !url) {
        const char *response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "400 Bad Request\n";
        write(client_socket, response, strlen(response));
        close(client_socket);
        return NULL;
    }

    if (strcmp(method, "GET") != 0) {
        const char *response =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "405 Method Not Allowed\n";
        write(client_socket, response, strlen(response));
        close(client_socket);
        return NULL;
    }

    if (strncmp(url, "/static/", 8) == 0) {
        handle_static(client_socket, url);
    } else if (strncmp(url, "/calc/", 6) == 0) {
        handle_calc(client_socket, url);
    } else {
        const char *response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "404 Not Found\n";
        write(client_socket, response, strlen(response));
    }

    close(client_socket);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port_number;

    // argument checks
    if (argc < 2) {
        port_number = 80;
    } else if (strcmp(argv[1], "-p") == 0) {
        port_number = atoi(argv[2]);
    } else {
        fprintf(stderr, "Usage: %s [-p port_number]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(port_number);

    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Error listening on socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port_number);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_length = sizeof(client_address);
        int client_socket       = accept(
            server_socket, (struct sockaddr *)&client_address, &client_length);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }

        pthread_t thread_id;
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr     = client_socket;
        pthread_create(&thread_id, NULL, handle_client, client_socket_ptr);
        pthread_detach(thread_id);
    }

    return 0;
}
