#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <errno.h>

#include <unistd.h>

#include <stdio.h>

#include <iostream>

#include <stdlib.h>

#include <netdb.h>

#include <string.h>

#include <stdbool.h>

#include <signal.h>

#include <sys/wait.h>


extern int errno;
int port, sd;
struct sockaddr_in server;
bool logged = 0;
int pid;
int reply();
bool register_user();
int connect_user(char* host, char* port_from_user) {
    port = atoi(port_from_user);
    printf("portul = %d\n Hostul = %s\n", port, host);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[client] Eroare la socket().\n");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr*)&server, sizeof(struct sockaddr)) == -1) {
        perror("[client]Eroare la connect().\n");
        return -1;
    }
    return 1;

}

int reply() {
    int len;
    char msg[100];
    fflush(stdout);
    bzero(msg, 100);
    printf("Introduceti mesajul: \n");
    if ((len = read(0, msg, 100)) < 1) {
        perror("Eraore la scriere in reply \n");
    }
    msg[len - 1] = '\0';

    if (write(sd, msg, strlen(msg)) < 0) {
        perror("[client]Eroare la write() spre server.\n");
        return -1;
    }
    printf("Mesajul a fost transmis.\n");
    return 1;
}

int write_message() {
    char username[100], msg[100];
    int len;
    fflush(stdout);
    bzero(msg, 100);
    bzero(username, 100);
    printf("Introduceti numele utilizitorului : \n");
    scanf("%s", username);

    if (write(sd, username, strlen(username)) < 0) {
        perror("[client]Eroare la write() spre server.\n");
    }

    fflush(stdout);

    printf("Introduceti mesajul: \n");
    len = read(0, msg, 100);
    msg[len - 1] = '\0';
    if (write(sd, msg, strlen(msg)) < 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    printf("Mesajul a fost transmis.\n");
    return 1;
}

bool login() {
    int len;
    char username[100], password[100], response[2];
    bzero(username, 100);
    bzero(password, 100);
    if (write(sd, "login", 5) < 0) {
        perror("[client]Eroare la scriere");
        return 0;
    }
    fflush(stdout);
    printf("Username: \n");
    if ((len = read(0, username, 100)) < 1) {
        perror("Eroare la read de la utilizator \n");
        return 0;
    }
    username[len - 1] = '\0';

    printf("Password: \n");
    if (write(sd, username, (int)strlen(username)) < 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    fflush(stdout);
    if ((len = read(0, password, 100)) < 1) {
        perror("Eroare la read de la utilizator \n");
        return 0;
    }
    password[len - 1] = '\0';

    if (write(sd, password, (int)strlen(password)) < 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    if (read(sd, response, 1) < 1) {
        perror("[client]Eroare la read() spre server.\n");
        return 0;
    }

    if (response[0] == '1') {
        logged = 1;
        printf("[client]User is logged succ!\n");

        return 1;
    }
    else {
        printf("Username or password is incorrect!\n");
        return 0;
    }
}
bool register_user() {
    char username[128];
    char password[128];
    bzero(username, sizeof(username));
    bzero(password, sizeof(password));

    // Send request to server to register a new user
    if (write(sd, "register", 8) < 0) {
        perror("[client]Eroare la scriere");
        return 0;
    }

    // Get the username and password from the use
    printf("Username :\n");
    scanf("%s", username);
    if (write(sd, username, strlen(username)) < 0) {
        perror("[client]Eroare la scriere");
        return 0;
    }

    printf("Password :\n");
    scanf("%s", password);
    if (write(sd, password, strlen(password)) < 0) {
        perror("[client]Eroare la scriere");
        return 0;
    }

    // Receive response from server
    char response[1024];
    int len;

    bzero(response, sizeof(response));
    if ((len = read(sd, response, 100)) < 1) {
        perror("[client] Eroare la citire");
        return 0;
    }
    if (response[0] == '0') {
        printf("Username already exists\n");
        return 0;
    }
    else {
        printf("The user has been registered\n");
        printf("User is logged succ\n");
        return 1;

    }
}

bool choose_login_or_register() {
    char command[256];
    bzero(command, sizeof(command));
    fflush(stdout);
    printf("To exit, type quit!\n");
    printf("login or register ?\n");

    scanf("%s", command);
    if (!(strcmp(command, "login"))) {
        if (login()) return true;
        else choose_login_or_register();
    }
    else if (!(strcmp(command, "register"))) {
        if (register_user()) return true;
        else choose_login_or_register();
    }
    else if (!(strcmp(command, "quit"))) {
        if (write(sd, "quit", 4) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
        return 0;
    }
    else {
        printf("[client]Command is incorrect.\n");
        choose_login_or_register();
    }

}

void meniu() {
    fflush(stdout);
    printf("Alegeti una din optiuni :\n");
    printf("send [transmite mesajul]\n");
    printf("online [vezi utilizatorii online]\n");
    printf("read [citeste mesajele noi]\n");
    printf("archive [citeste mesajele arhiva]\n");
    printf("reply [reply la ultimul mesaj primit]\n");
    printf("logout [iesi din cont]\n");
    printf("quit [iesi din app]\n");
}

int read_command() {
    char command[100], msgFrom[100];
    bzero(command, 100);
    bzero(msgFrom, 100);
    fflush(stdout);
    int len = read(0, command, 100);
    command[len - 1] = '\0';
    if (!strcmp(command, "online")) {
        if (write(sd, "online", 6) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
        return 1;
    }
    else if (!strcmp(command, "send")) {
        if (write(sd, "send", 4) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
        write_message();
        return 1;
    }
    else if (!strcmp(command, "read")) {
        if (write(sd, "read", 4) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
    }
    else if (!strcmp(command, "archive")) {
        if (write(sd, "archive", 7) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
    }
    else if (!strcmp(command, "reply")) {
        if (write(sd, "reply", 5) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
        reply();
        return 1;
    }
    else if (!strcmp(command, "quit")) {
        if (write(sd, "quit", 4) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
        return -1;
    }
    else if (!strcmp(command, "logout")) {
        if (write(sd, "logout", 6) < 0) {
            perror("[client]Eroare la write() spre server.\n");
        }
        return 0;
    }
    else {
        printf("Comanda este incorecta, va rog introduceti una dintre aceste comenzi !\n");
        meniu();
    }
    return 1;
}

bool read_from_server() {
    char msg[1000];
    int len;
    while (true) {
        bzero(msg, sizeof(msg));

        if ((len = read(sd, msg, sizeof(msg))) < 1) {
            perror("[client]Se inchide serverul\n");
            return 0;
        }
        msg[len - 1] = '\0';
        if (!strcmp("quit", msg)) {
            return 0;
        }
        fflush(stdout);
        printf("%s\n", msg);
    }
}
void kill_child(int sig) {
    kill(pid, SIGKILL);
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    if (argc != 3) {
        printf("[client] Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    if (connect_user(argv[1], argv[2]) != 1) {
        printf("Eroare la connect\n");
        close(sd);
        return -1;
    }

    signal(SIGALRM, (void(*)(int)) kill_child);
    while (true) {
        int stat;
        if (choose_login_or_register()) { // Daca nu e logat nu poate intra in read_command si read_from_server

            if ((pid = fork()) < 0) {
                perror("Eroare la fork() in client");
            }
            if (pid) {
                meniu();
                while (true) {
                    stat = read_command();
                    if (stat < 1) {
                        break;
                    }
                }

                wait(NULL);

                if (stat == -1) {
                    close(sd);
                    return 0;
                }
            }
            else {
                read_from_server();
                exit(0);
            }
        }
        else {
            close(sd);
            return 0;
        }
    }

    close(sd);
    return 0;
}