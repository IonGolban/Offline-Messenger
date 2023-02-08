#include <iostream>

#include <sys/types.h>

#include <sys/socket.h>

#include <sys/time.h>

#include <netinet/in.h>

#include <unistd.h>

#include <errno.h>

#include <stdio.h>

#include <arpa/inet.h>

#include <signal.h>

#include <pthread.h>

#include <string.h>

#include <stdbool.h>

#include <vector>

#include "User.h"

#include <mysql/mysql.h>

#define PORT 2729
const char* fusers = "/tmp/users.txt";
struct sockaddr_in server;
struct sockaddr_in from;
int sd, client;
int optval = 1;
int fd;
int nfds;
int len;
int pid;
pthread_t th[100]; //Identificatorii thread-urilor care se vor crea
int i = 0;
MYSQL* con; // the connection

static void* treat(void* arg);
void user_logged_in(char* username, void* arg);
int login(void* arg);
bool register_user(char* username, char* password);
void raspunde(void* arg);
int verify_username(const char* username);
int verify_username_and_password(const char* username,const char* password);
void init_users();
int listen_clients();
int init();
int read_command(void* arg);
void logout(void* arg);
char* get_username(void* arg);
bool send_msg(void* arg);
MYSQL* mysql_connection_setup(struct connection_details mysql_details);
MYSQL_RES* mysql_perform_query(MYSQL* connection,const char* sql_query);
void finish_with_error(MYSQL* con);
void init_messages();
void insert_message_bd(Message* message);
void read_msg(void* arg);
void read_archive(void* arg);
void set_seen_msg(User* user);
int reply_msg(void* arg);

int reply_msg(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char final_msg[2048], msg[2048];
    bzero(msg, sizeof(msg));
    bzero(final_msg, sizeof(msg));
    User* user = get_user(&tdL);
    Message* message = user->arhiva.back();
    int msg_len;
    if (msg_len = read(tdL.cl, msg, 100) < 0) {
        perror("Eroare la read() de la client.\n");
        return 0;
    }
    msg[msg_len - 1] = '\0';

    strcat(final_msg, "[Reply] la : ");
    strcat(final_msg, message->msg);
    strcat(final_msg, "\nCu mesajul : ");
    strcat(final_msg, msg);
    strcat(final_msg, "\n");

    Message* message_to_send = new Message(msg, get_username(&tdL), message->from_username, 1, final_msg);
    User* to_user = get_user(message->from_username);
    to_user->mesaje.push_back(message_to_send);
    insert_message_bd(message_to_send);

}
void set_seen_msg(User* user) {
    char query[2048];
    strcat(query, " update T_MESSAGE set seen = 1 where user = '");
    strcat(query, user->username);
    strcat(query, "';");
    mysql_query(con, query);

}
void read_archive(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char msg[2048];
    bzero(msg, sizeof(msg));
    User* user = get_user(&tdL);
    if (user->arhiva.empty()) strcat(msg, "Nu ai mesaje in arhiva\n");
    else {
        for (Message* message : user->arhiva) {
            strcat(msg, message->from_username);
            strcat(msg, ": ");
            strcat(msg, message->msg);
            strcat(msg, "\n");
        }
    }
    if (write(tdL.cl, msg, strlen(msg)) < 0) {
        perror("[server] Eroare la scriere spre client in read_msg \n");
    }
}
void read_msg(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char msg[2048];
    bzero(msg, sizeof(msg));
    User* user = get_user(&tdL);
    if (user->mesaje.empty()) strcat(msg, "Nu ai mesaje noi\n");
    while (!user->mesaje.empty()) {
        Message* message = user->mesaje.back();
        strcat(msg, message->from_username);
        strcat(msg, ": ");
        if (message->is_reply) strcat(msg, message->reply_message);
        else strcat(msg, message->msg);
        strcat(msg, "\n");
        user->arhiva.push_back(message);
        set_seen_msg(user);
        user->mesaje.pop_back();
    }
    if (write(tdL.cl, msg, strlen(msg)) < 0) {
        perror("[server] Eroare la scriere spre client in read_msg \n");
    }

}

bool send_msg(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char username[100], msg[100];
    int msg_len;
    bzero(username, sizeof(username));
    bzero(msg, sizeof(msg));
    if (msg_len = read(tdL.cl, username, 100) < 0) {
        perror("Eroare la read() de la client.\n");
        return 0;
    }
    username[msg_len - 1] = '\0';

    if (msg_len = read(tdL.cl, msg, 100) < 0) {
        perror("Eroare la read() de la client.\n");
        return 0;
    }
    msg[msg_len - 1] = '\0';

    if (verify_username(username)) {
        printf("A intrat in if de inserare a mesajului \n");
        Message* message = new Message(msg, get_username(&tdL), username);
        User* user = get_user(username);
        user->mesaje.push_back(message);
        insert_message_bd(message);
        return 1;
    }
    return 1;
}

void insert_message_bd(Message* message) {

    char query[1023];
    strcat(query, " INSERT INTO T_MESSAGE (message,user,user_from,seen) VALUES ('");
    strcat(query, message->msg);
    strcat(query, "','");
    strcat(query, message->to_username);
    strcat(query, "','");
    strcat(query, message->from_username);
    strcat(query, "',0);");
    printf("[server] query : %s \n", query);
    if (mysql_query(con, query)) {
        finish_with_error(con);
    }
}

char* get_username(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    for (User* user : users) {
        if (user->current_fd == tdL.cl) {
            return user->username;
        }
    }
    return "0";
}

void user_logged_in(char* username, void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    for (User* user : users) {
        printf("compara cu %s\n", user->username);
        if (!strcmp(username, user->username)) {
            printf("[server] User a fost inregistrat\n");
            user->isLogged = true;
            user->current_fd = tdL.cl;
            break;
        }
    }
}

void remove_spaces(char* s) {
    char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while (*s++ = *d++);
}

bool register_user(char* username, char* password) {
    char query[2048];
    bzero(query, sizeof(query));
    strcat(query, "INSERT INTO T_USER (username,password) VALUES ('");
    strcat(query, username);
    strcat(query, "','");
    strcat(query, password);
    strcat(query, "');");

    if (mysql_query(con, query)) {
        finish_with_error(con);
    }
    users.push_back((User*) new User(users.back()->id + 1, username, password));
    return true;
}
int login(void* arg) {
    int login = 0;
    fflush(stdout);
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char password[100], username[100], msg[100];
    int msg_len;

    signal(SIGPIPE, SIG_IGN);
    while (true) {

        bzero(password, sizeof(password));
        bzero(username, sizeof(username));
        bzero(msg, sizeof(msg));
        if (msg_len = read(tdL.cl, msg, 100) < 1) {
            perror("Eroare la read() de la client.\n");
            return 0;
        }
        msg[msg_len - 1] = '\0';

        if (msg_len = read(tdL.cl, username, 100) < 1) {
            perror("Eroare la read() de la client.\n");
            return 0;
        }

        printf("Mesaj: [%s]\n", username);
        if (msg_len = read(tdL.cl, password, 100) < 1) {
            perror("Eroare la read() de la client.\n");
            return 0;
        }

        printf("Mesaj: [%s]\n", password);

        if (!(strcmp(msg, "login"))) login = 1;
        else login = 0;

        if (login) {
            if (verify_username_and_password(username, password)) {
                write(tdL.cl, "1", 1);
                user_logged_in(username, &tdL);
                return 1;
            }
            else write(tdL.cl, "0", 1);
        }
        else {
            if (verify_username(username)) {
                write(tdL.cl, "0", 1);
            }
            else {
                write(tdL.cl, "1", 1);
                register_user(username, password);
                user_logged_in(username, &tdL);
                return 1;
            }
        }
    }
}

void raspunde(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    while (true) {
        if (login(&tdL)) {
            printf("Clientul cu descriptorul %d a fost logat\n", tdL.cl);
            int state = read_command(&tdL);
            if (state == 0) {
                logout(&tdL);
                break;
            }
            else if (state == 1) {
                logout(&tdL);
            }
            else break;
        }
        else {
            printf("Clientul cu descriptorul %d nu a reusit sa se logheze \n", tdL.cl);
        }
    }

}

void logout(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    for (User* user : users) {
        if (user->isLogged && user->current_fd == tdL.cl) {
            user->isLogged = false;
            user->current_fd = -1;
            break;
        }
    }
}

static void* treat(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    raspunde((struct thData*)arg);
    printf("Se inchide clientul cu descriptorul %d\n", tdL.cl);
    close((intptr_t)arg);
    return (NULL);

};

char* conv_addr(struct sockaddr_in address) {
    static char str[25];
    char port[7];
    strcpy(str, inet_ntoa(address.sin_addr));
    printf("ip-address client = %s \n", str);
    bzero(port, 7);
    sprintf(port, ":%d", ntohs(address.sin_port));
    strcat(str, port);
    return (str);
}
int verify_username(const char* username) {

    for (User* user : users) {
        if (!strcmp(user->username, username)) {
            return 1;
        }
    }
    return 0;
}
int verify_username_and_password(const char* username,
    const char* password) {

    for (User* user : users) {
        if (!strcmp(user->username, username) && !strcmp(user->password, password)) {
            return 1;
        }
    }
    return 0;
}

void init_users() {
    MYSQL_RES* res;
    MYSQL_ROW row;
    users.clear();
    res = mysql_perform_query(con, "SELECT * FROM T_USER;");
    while ((row = mysql_fetch_row(res)) != NULL) {
        User* user = new User(atoi(row[0]), row[1], row[2]);
        users.push_back(user);
    }
    for (User* user : users) {
        printf("Usernmae : [%s]\n", user->username);
    }

    mysql_free_result(res);

}
void init_messages() {
    MYSQL_RES* res;
    MYSQL_ROW row;
    res = mysql_perform_query(con, "SELECT * FROM T_MESSAGE;");
    while ((row = mysql_fetch_row(res)) != NULL) {
        Message* message = new Message(row[1], row[3], row[2]);
        int seen = atoi(row[4]);
        User* user = get_user(row[2]);
        if (user != NULL) {
            seen ? user->arhiva.push_back(message) : user->mesaje.push_back(message);
        }
    }
    mysql_free_result(res);
}

void get_online_users(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char online_users[1000];
    bzero(&online_users, sizeof(online_users));

    for (User* user : users) {
        if (user->isLogged) {
            printf("%s\n", user->username);
            strcat(online_users, user->username);
            strcat(online_users, "\n");
        }
    }
    printf("[server] %s", online_users);
    if (write(tdL.cl, online_users, (int)strlen(online_users)) < 0) {
        perror("[server] Eroare la scrire in client");
    }

}

int read_command(void* arg) {
    struct thData tdL;
    tdL = *((struct thData*)arg);
    char command[100];
    int len;
    while (true) {
        if (!get_user(&tdL)->mesaje.empty()) {
            if (write(tdL.cl, "Ai mesaje noi", 14) < 0) {
                perror("[server]Eroare la scriere in client in read_command -> mesaje noi ");
            }
        }
        bzero(&command, sizeof(command));
        if ((len = read(tdL.cl, command, 100)) < 0) {
            perror("Eroare la citirea comenzii");
        }
        if (len == 0) {
            logout(&tdL);
            return 0;
        }
        command[len] = '\0';
        printf("Comanda= [%s], lungimea = [%d]\n", command, (int)strlen(command));
        if (!strcmp(command, "online")) {
            fflush(stdout);
            printf("A intrat in if de comparare\n");
            get_online_users(&tdL);
        }
        else if (!strcmp(command, "quit")) {
            logout(&tdL);
            write(tdL.cl, "quit", 5);
            return 0;
        }
        else if (!strcmp(command, "send")) {
            send_msg(&tdL);
        }
        else if (!strcmp(command, "read")) {
            read_msg(&tdL);
        }
        else if (!strcmp(command, "archive")) {
            read_archive(&tdL);
        }
        else if (!strcmp(command, "reply")) {
            reply_msg(&tdL);
        }
        else if (!strcmp(command, "logout")) {
            logout(&tdL);
            write(tdL.cl, "quit", 5);
            return 1;
        }
    }
}
MYSQL* mysql_connection_setup(struct connection_details mysql_details) {
    MYSQL* connection = mysql_init(NULL); // mysql instance

    //connect database
    if (!mysql_real_connect(connection, mysql_details.server, mysql_details.user, mysql_details.password, mysql_details.database, 0, NULL, 0)) {
        std::cout << "Connection Error: " << mysql_error(connection) << std::endl;
        exit(1);
    }

    return connection;
}

// mysql_res = mysql result
MYSQL_RES* mysql_perform_query(MYSQL* connection,
    const char* sql_query) {
    //send query to db
    if (mysql_query(connection, sql_query)) {
        std::cout << "MySQL Query Error: " << mysql_error(connection) << std::endl;
        exit(1);
    }

    return mysql_use_result(connection);
}
void finish_with_error(MYSQL* con) {
    fprintf(stderr, "DB eroor : %s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}
void init_db() {

    struct connection_details mysqlD;
    mysqlD.server = "localhost"; // where the mysql database is
    mysqlD.user = "root"; // user
    mysqlD.password = "kilalu322"; // the password for the database
    mysqlD.database = "retele_proiect"; // the databse

    // connect to the mysql database
    con = mysql_connection_setup(mysqlD);
}

int listen_clients() {
    /* servim in mod concurent clientii... */

    while (1) {
        int client;
        thData* td; //parametru functia executata de thread    
        len = sizeof(from);
        bzero(&from, sizeof(from));
        client = accept(sd, (struct sockaddr*)&from, (socklen_t*)&len);
        printf("a acceptat unul\n");
        if (client < 0) {
            perror("[server] Eroare la accept().\n");
            continue;
        }
        int idThread; //id-ul threadului
        int cl; //descriptorul intors de accept

        td = (struct thData*)malloc(sizeof(struct thData));
        td->idThread = i++;
        td->cl = client;

        pthread_create(&th[i], NULL, &treat, td);
    }
}

int init() {
    init_db();
    init_users();
    init_messages();

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[server] Eroare la socket().\n");
        return errno;
    }

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr*)&server, sizeof(struct sockaddr)) == -1) {
        perror("[server] Eroare la bind().\n");
        return errno;
    }

    if (listen(sd, 5) == -1) {
        perror("[server] Eroare la listen().\n");
        return errno;
    }

    printf("[server] Asteptam la portul %d...\n", PORT);
    fflush(stdout);

}

int main() {
    signal(SIGPIPE, SIG_IGN);
    init();

    listen_clients();
}