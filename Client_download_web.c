#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 1024

// Define a struct to pass download information to the threads
struct DownloadInfo{
	const char *url;
	const char *filename;
};


int downloadFile(const char *url,const char *filename){
	int sockfd;
	struct sockaddr_in server_addr;
	struct hostent *server;
	char request[MAX_BUFFER_SIZE];
	char response[MAX_BUFFER_SIZE];
	FILE *file;

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
 if (sockfd < 0) {
        perror("Error creating socket");
        return 1;
    }

 // Parse URL to extract hostname and path
    char hostname[MAX_BUFFER_SIZE];
    int port = 80; // Default HTTP port
    char path[MAX_BUFFER_SIZE];
    if ((sscanf(url, "http://%[^:/]/%s", hostname, path) != 2)&& (sscanf(url, "http://%[^:/]:%d/%s", hostname, &port, path) != 3)) {
        perror("Invalid URL format");
 return 1;
    }

    server = gethostbyname(hostname);//gethostbyname function is used to resolve hostname into an IP address
    if (server == NULL) {
        perror("Error resolving hostname");
        return 1;
    }
 // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));//memset() is used to fill a block of memory with a particular value.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to the server");
        return 1;
    }

    // Send an HTTP GET request
    snprintf(request, sizeof(request), "GET /%s HTTP/1.1\r\n"
                                      "Host: %s\r\n"
                                      "Connection: close\r\n\r\n", path, hostname);
    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("Error sending request");
        return 1;
}


    // Create a file for saving the response content
    file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Error opening file for writing");
    return 1;
    }
  ssize_t bytes_received;
    int responseHeadersDone = 0;

 while ((bytes_received = recv(sockfd, response, sizeof(response), 0)) > 0) {
        if (!responseHeadersDone) {
            char *headersEnd = strstr(response, "\r\n\r\n");            // Find the end of HTTP response headers
            if (headersEnd != NULL) {                                    // Calculate the length of the headers
                size_t headersLength = headersEnd - response + 4;

                // Display the HTTP response headers to the terminal (stdout)
            //    fwrite(response, 1, headersLength, stdout);
 if (strstr(response, "HTTP/1.1 400 Bad Request") != NULL ||
                strstr(response, "HTTP/1.1 404 Not Found") != NULL) {
                printf("Bad Request or Not Found: %s\n", url);
                close(sockfd);
                return 1; // Exit without saving the file
            }

                // Write the rest of the data or file content to the file
                fwrite(response + headersLength, 1, bytes_received - headersLength, file);

                responseHeadersDone = 1;
            } else {
                // If headersEnd is NULL, the headers haven't ended yet; write the entire buffer
                fwrite(response, 1, bytes_received, stdout);
            }
        } else {
            // Write the file content to the file
            fwrite(response, 1, bytes_received, file);
        }
    }

    if (bytes_received < 0) {
        perror("Error receiving data");
        return 1;
    }

    fclose(file); // Close the file stream for the downloaded file
    close(sockfd);

    return 0;
}
// This function is executed by each thread
void *downloadThread(void *arg) {
    struct DownloadInfo *info = (struct DownloadInfo *)arg;
    if (downloadFile(info->url, info->filename) == 0) {
        printf("Downloaded: %s\n", info->filename);
    } else {
        printf("Failed to download: %s\n", info->filename);
    }

        return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <URL1> [<URL2> <URL3> ...]\n", argv[0]);
        return 1;
    }

    // Create an array of pointers to DownloadInfo structs
    struct DownloadInfo **infoArray = malloc((argc - 1) * sizeof(struct DownloadInfo ));
//	printf(" %d %d", sizeof(struct DownloadInfo *),sizeof(struct DownloadInfo));
if (infoArray == NULL) {
        perror("Memory allocation error");
        return 1;
    }

    // Create threads for each URL provided
    pthread_t *threads = malloc((argc - 1) * sizeof(pthread_t));
    if (threads == NULL) {
        perror("Memory allocation error");
        free(infoArray);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        // Create a DownloadInfo struct for each URL
        infoArray[i - 1] = malloc(sizeof(struct DownloadInfo));
        if (infoArray[i - 1] == NULL) {
            perror("Memory allocation error");
            free(infoArray);
            free(threads);
            return 1;
        }

        infoArray[i - 1]->url = argv[i];
        infoArray[i - 1]->filename = strrchr(argv[i], '/') + 1; // Extract filename from URL

        // Create a thread for each URL
        if (pthread_create(&threads[i - 1], NULL, downloadThread, (void *)infoArray[i - 1]) != 0) {
            perror("Thread creation error");
            free(infoArray);
            free(threads);
            return 1;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < argc - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    // Free memory
    for (int i = 0; i < argc - 1; i++) {
        free(infoArray[i]);
    }
    free(infoArray);
    free(threads);

    return 0;
}

