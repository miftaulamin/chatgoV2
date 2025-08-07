#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define MAX_LEN 1024

struct userinformation {
    char username[50];
    char email[60];
    char password[50];
};

struct userinformation user1;
char password[50];

// Input function
void takeinput(char ch[50]) {
    fgets(ch, 50, stdin);
    size_t len = strlen(ch);
    if (len > 0 && ch[len - 1] == '\n') {
        ch[len - 1] = '\0';
    }
}

// Generate username from email
void generateUsername(char email[50], char username[50]) {
    int i;
    for (i = 0; i < strlen(email); i++) {
        if (email[i] == '@') {
            break;
        }
        username[i] = email[i];
    }
    username[i] = '\0'; // Null terminate
}

// Verify user function
int verifyUser(SOCKET sd) {
    int attempt = 3;
    char buffer[200];
    char temp_username[50], temp_password[50];

    while (attempt > 0) {
        printf("Enter Your username: ");
        takeinput(temp_username);
        printf("Enter Your password: ");
        takeinput(temp_password);
        system("cls");

        // Send LOGIN command to server
        snprintf(buffer, sizeof(buffer), "LOGIN %s %s", temp_username, temp_password);
        send(sd, buffer, strlen(buffer), 0);

        // Wait for server response
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(sd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                printf("Server disconnected\n");
                return 0;
            }
            buffer[bytes] = '\0';

            if (strcmp(buffer, "OK") == 0) {
                printf("Successfully Logged in!\n");
                // Only set the username after successful login
                strcpy(user1.username, temp_username);
                strcpy(user1.password, temp_password);
                Sleep(1000); // Give a moment for user to see the success message
                return 1;
            } else if (strcmp(buffer, "FAIL") == 0) {
                attempt--;
                printf("Your credentials don't match. %d attempt(s) left.\n", attempt);
                if (attempt > 0) {
                    printf("Press Enter to try again...");
                    getchar();
                }
                break; // break out of while(1) to retry
            } else {
                // Ignore unrelated messages (like join/leave notifications)
                continue;
            }
        }
    }
    
    printf("Maximum login attempts exceeded.\n");
    return 0;
}

// Authentication Function
int auth(SOCKET sd) {
    int choice;
    int attempt = 3;
    char buffer[200];

    printf("\t\t\t\t\t|-----------------------------------|\n");
    printf("\t\t\t\t\t|            C H A T G O            |\n");
    printf("\t\t\t\t\t|-----------------------------------|\n\n");
    printf("1. Signup\n");
    printf("2. Login\n");
    printf("3. Exit\n");
    printf("Enter your choice: ");
    scanf("%d", &choice);
    getchar();
    system("cls");

    switch (choice) {
        case 1: {
            printf("Enter your Email: ");
            scanf("%s", user1.email);
            getchar();
            generateUsername(user1.email, user1.username);
            printf("Generated username: %s\n", user1.username);

            while (attempt > 0) {
                printf("Enter your Password: ");
                scanf("%s", user1.password);
                getchar();
                printf("Please Confirm Your Password: ");
                scanf("%s", password);
                getchar();
                
                if (strcmp(user1.password, password) == 0) {
                    printf("Password matched!!\n");
                    break;
                } else {
                    printf("Passwords don't match, try again!\n");
                    attempt--;
                    printf("You have %d attempt(s) left\n", attempt);
                    Beep(750, 400);
                }
            }

            if (attempt == 0) {
                printf("Maximum attempts exceeded. Goodbye!\n");
                return 0;
            }

            // Send SIGNUP command to server
            snprintf(buffer, sizeof(buffer), "SIGNUP %s %s", user1.username, user1.password);
            send(sd, buffer, strlen(buffer), 0);

            // Wait for server response
            while (1) {
                memset(buffer, 0, sizeof(buffer));
                int bytes = recv(sd, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                    printf("Server disconnected\n");
                    return 0;
                }
                buffer[bytes] = '\0';

                if (strcmp(buffer, "USER_EXISTS") == 0) {
                    printf("Username already exists. Please try again.\n");
                    return auth(sd); // Recursive call to try again
                } else if (strcmp(buffer, "OK") == 0) {
                    printf("Signup successful!\n");
                    return 1;
                } else if (strcmp(buffer, "FAIL") == 0) {
                    printf("Signup failed. Please try again.\n");
                    return 0;
                } else {
                    // Ignore unrelated messages (like join/leave notifications)
                    continue;
                }
            }
            break;
        }
        
        case 2: {
            int check = verifyUser(sd);
            if (check == 1) {
                return 1;
            } else {
                return 0;
            }
            break;
        }
        
        case 3: {
            printf("Goodbye!\n");
            return 0;
        }
        
        default: {
            printf("Invalid choice. Please try again.\n");
            return auth(sd); // Recursive call for invalid choice
        }
    }
}

// Thread function to receive messages from the server
DWORD WINAPI receiveMessages(LPVOID socketDesc) {
    SOCKET sock = *(SOCKET *)socketDesc;
    char buffer[MAX_LEN];
    int bytes;

    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        
        // Check if it's an authentication response
        if (strcmp(buffer, "OK") == 0 || 
            strcmp(buffer, "FAIL") == 0 || 
            strcmp(buffer, "USER_EXISTS") == 0 ||
            strcmp(buffer, "AUTH_REQUIRED") == 0) {
            // Skip authentication responses in chat mode
            continue;
        }
        
        printf("\n%s\n", buffer);
        printf("[%s]: ", user1.username);
        fflush(stdout);
    }

    printf("\nDisconnected from server.\n");
    return 0;
}

int main() {
    system("color 0A");

    char message[MAX_LEN], fullMessage[MAX_LEN + 100];
    SOCKET sd;
    struct sockaddr_in server;
    WSADATA wsa;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == INVALID_SOCKET) {
        printf("Could not create socket. Error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Server details
    server.sin_addr.s_addr = inet_addr("20.197.12.181"); // 20.197.12.181
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);

    // Connect to server FIRST
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Connection failed.\n");
        closesocket(sd);
        WSACleanup();
        return 1;
    }

    printf("Connected to the server.\n");

    // THEN authenticate
    int authResult = auth(sd);
    if (authResult == 0) {
        printf("Authentication failed or cancelled.\n");
        closesocket(sd);
        WSACleanup();
        return 0;
    }

    printf("Authentication successful! You can start chatting.\n");
    printf("Type your messages and press Enter to send. Type '/quit' to exit.\n");
    printf("=========================================\n");

    // Clear any remaining input buffer
    fflush(stdin);

    // Start the receive thread
    CreateThread(NULL, 0, receiveMessages, &sd, 0, NULL);

    // Main loop to send messages
    while (1) {
        printf("[%s]: ", user1.username);
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = '\0'; // Remove newline

        // Check for quit command
        if (strcmp(message, "/quit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        // Format message with username
        snprintf(fullMessage, sizeof(fullMessage), "[%s]: %s", user1.username, message);
        
        // Send message to server
        if (send(sd, fullMessage, strlen(fullMessage), 0) == SOCKET_ERROR) {
            printf("Failed to send message.\n");
            break;
        }
    }

    // Cleanup
    closesocket(sd);
    WSACleanup();
    return 0;
}