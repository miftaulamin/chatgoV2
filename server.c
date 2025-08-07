#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <direct.h>  // For _getcwd
#include <errno.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

SOCKET user[MAX_CLIENTS];
int clientcount = 0;
CRITICAL_SECTION cs;

// Function to test file creation at startup
void test_file_access() {
    char cwd[1024];
    _getcwd(cwd, sizeof(cwd));
    printf("Server running from: %s\n", cwd);
    
    FILE *test = fopen("userdata.dat", "a+");
    if (test) {
        printf("File access test: SUCCESS - userdata.dat can be created/accessed\n");
        fclose(test);
    } else {
        printf("File access test: FAILED - Cannot create/access userdata.dat\n");
        printf("Error: %s\n", strerror(errno));
        printf("Make sure the server has write permissions in this directory.\n");
    }
}

// Function to check if user exists
int user_exists(const char* username) {
    FILE *f = fopen("userdata.dat", "r");
    if (!f) return 0; // File doesn't exist, so user doesn't exist
    
    char u[50], p[50];
    while (fscanf(f, "%s %s", u, p) == 2) {
        if (strcmp(u, username) == 0) {
            fclose(f);
            return 1; // User exists
        }
    }
    fclose(f);
    return 0; // User doesn't exist
}

// Function to validate login credentials
int validate_login(const char* username, const char* password) {
    FILE *f = fopen("userdata.dat", "r");
    if (!f) return 0; // File doesn't exist, login invalid
    
    char u[50], p[50];
    while (fscanf(f, "%s %s", u, p) == 2) {
        if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
            fclose(f);
            return 1; // Valid login
        }
    }
    fclose(f);
    return 0; // Invalid login
}

// Function to add new user
int add_user(const char* username, const char* password) {
    FILE *f = fopen("userdata.dat", "a+");
    if (!f) {
        printf("Error: Cannot open userdata.dat for writing\n");
        printf("Error details: %s\n", strerror(errno));
        return 0; // Failed to open file
    }
    
    fprintf(f, "%s %s\n", username, password);
    fflush(f); // Force write to disk
    fclose(f);
    printf("User added successfully: %s\n", username);
    return 1; // Success
}

// Handle authentication for a client
int handle_auth(SOCKET cd, char username[50]) {
    char buffer[BUFFER_SIZE];
    char command[20], uname[50], pass[50];
    int bytes;

    printf("Starting authentication for new client...\n");

    while (1) {
        // Clear buffer
        memset(buffer, 0, sizeof(buffer));
        
        bytes = recv(cd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            printf("Client disconnected during authentication\n");
            return 0;
        }

        buffer[bytes] = '\0';
        
        // Parse the command
        int parsed = sscanf(buffer, "%s %s %s", command, uname, pass);
        if (parsed < 3) {
            printf("Invalid command format received: %s\n", buffer);
            send(cd, "INVALID_FORMAT", strlen("INVALID_FORMAT"), 0);
            continue;
        }
        
        printf("Received: %s %s %s\n", command, uname, pass);

        if (strcmp(command, "SIGNUP") == 0) {
            if (user_exists(uname)) {
                printf("Signup failed: User '%s' already exists\n", uname);
                send(cd, "USER_EXISTS", strlen("USER_EXISTS"), 0);
            } else {
                if (add_user(uname, pass)) {
                    printf("Signup successful for user: %s\n", uname);
                    send(cd, "OK", strlen("OK"), 0);
                    strcpy(username, uname);
                    return 1; // Authentication successful
                } else {
                    printf("Signup failed: Could not save user data\n");
                    send(cd, "FAIL", strlen("FAIL"), 0);
                }
            }
        }
        else if (strcmp(command, "LOGIN") == 0) {
            if (validate_login(uname, pass)) {
                printf("Login successful for user: %s\n", uname);
                send(cd, "OK", strlen("OK"), 0);
                strcpy(username, uname);
                return 1; // Authentication successful
            } else {
                printf("Login failed for user: %s\n", uname);
                send(cd, "FAIL", strlen("FAIL"), 0);
            }
        }
        else {
            printf("Unknown command received: %s\n", command);
            send(cd, "UNKNOWN_COMMAND", strlen("UNKNOWN_COMMAND"), 0);
        }
    }
}

// Broadcast message to all clients except sender
void broadcast_message(const char* message, SOCKET sender) {
    EnterCriticalSection(&cs);
    for (int i = 0; i < clientcount; i++) {
        if (user[i] != sender) {
            int result = send(user[i], message, strlen(message), 0);
            if (result == SOCKET_ERROR) {
                printf("Failed to send message to client %d\n", i);
            }
        }
    }
    LeaveCriticalSection(&cs);
}

// Remove disconnected client from array
void remove_client(SOCKET cd) {
    EnterCriticalSection(&cs);
    for (int i = 0; i < clientcount; i++) {
        if (user[i] == cd) {
            // Shift remaining clients
            for (int j = i; j < clientcount - 1; j++) {
                user[j] = user[j + 1];
            }
            clientcount--;
            printf("Client removed. Total clients: %d\n", clientcount);
            break;
        }
    }
    LeaveCriticalSection(&cs);
}

// Thread function to handle each client
DWORD WINAPI MessageHandle(LPVOID clientSocket) {
    SOCKET cd = *(SOCKET *)clientSocket;
    char username[50] = {0};
    free(clientSocket);

    printf("New client thread started\n");

    // Handle authentication first
    if (!handle_auth(cd, username)) {
        printf("Authentication failed for client\n");
        closesocket(cd);
        remove_client(cd);
        return 0;
    }

    printf("Client '%s' authenticated successfully\n", username);

    // Send join notification to other clients
    char joinMsg[200];
    snprintf(joinMsg, sizeof(joinMsg), "*** %s joined the chat! ***", username);
    broadcast_message(joinMsg, cd);

    // Main message loop
    char rcv[BUFFER_SIZE];
    int bytes;
    
    while ((bytes = recv(cd, rcv, sizeof(rcv) - 1, 0)) > 0) {
        rcv[bytes] = '\0';
        
        // Log the message
        printf("Message from %s: %s\n", username, rcv);
        
        // Broadcast to all other clients
        broadcast_message(rcv, cd);
    }

    // Client disconnected
    printf("Client '%s' disconnected\n", username);
    
    // Send leave notification
    char leaveMsg[200];
    snprintf(leaveMsg, sizeof(leaveMsg), "*** %s left the chat ***", username);
    broadcast_message(leaveMsg, cd);

    // Clean up
    remove_client(cd);
    closesocket(cd);
    return 0;
}

int main() {
    printf("=== CHATGO SERVER ===\n");
    
    // Test file access at startup
    test_file_access();
    
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSA Startup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Winsock initialized successfully\n");

    SOCKET sd, cd;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    printf("Socket created successfully\n");

    // Enable socket reuse
    int opt = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in server, client;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(sd);
        WSACleanup();
        return 1;
    }
    printf("Socket bound to port 8888\n");

    if (listen(sd, 5) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        closesocket(sd);
        WSACleanup();
        return 1;
    }

    InitializeCriticalSection(&cs);
    printf("Server started and listening on port 8888...\n");
    printf("Waiting for clients to connect...\n\n");

    int c = sizeof(struct sockaddr_in);
    while (1) {
        cd = accept(sd, (struct sockaddr *)&client, &c);
        if (cd == INVALID_SOCKET) {
            printf("Accept failed. Error Code: %d\n", WSAGetLastError());
            continue;
        }

        printf("New client connected from %s:%d\n", 
               inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        EnterCriticalSection(&cs);
        if (clientcount < MAX_CLIENTS) {
            user[clientcount++] = cd;
            printf("Client added. Total clients: %d\n", clientcount);
            
            // Create thread for new client
            SOCKET *newClient = malloc(sizeof(SOCKET));
            if (newClient) {
                *newClient = cd;
                HANDLE hThread = CreateThread(NULL, 0, MessageHandle, newClient, 0, NULL);
                if (hThread) {
                    CloseHandle(hThread); // We don't need to keep the handle
                } else {
                    printf("Failed to create thread for client\n");
                    free(newClient);
                    closesocket(cd);
                    clientcount--;
                }
            } else {
                printf("Memory allocation failed for new client\n");
                closesocket(cd);
                clientcount--;
            }
        } else {
            printf("Maximum clients reached (%d). Connection rejected.\n", MAX_CLIENTS);
            send(cd, "SERVER_FULL", strlen("SERVER_FULL"), 0);
            closesocket(cd);
        }
        LeaveCriticalSection(&cs);
    }

    // Cleanup (this code won't be reached in normal execution)
    DeleteCriticalSection(&cs);
    closesocket(sd);
    WSACleanup();
    return 0;
}