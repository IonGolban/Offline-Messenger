#pragma once
#include <string>
class Message {
public:
	char msg[2048];
	char from_username[20];
	char to_username[20];
	bool is_reply = 0;
	char reply_message[2048];

		
	Message(const char* msg, char* from, char* to) { strcpy(this->msg, msg); strcpy(this->from_username, from); strcpy(this->to_username, to); };
	Message(const char* msg, char* from, char* to, bool is_reply, char* reply_message) { strcpy(this->msg, msg); strcpy(this->from_username, from); strcpy(this->to_username, to); this->is_reply = is_reply; strcat(this->reply_message,reply_message); };
	Message(const char* msg) { strcpy(this->msg, msg); };
};

class User {
public :
	int id;
	char username[20];
	char password[20];
	std::vector<Message*>mesaje;
	std::vector<Message*> arhiva;
	int current_fd;
	bool isLogged = 0;

	User(int id, const char* username, const char* password, int fd) { this->id = id; strcpy(this->username, username); strcpy(this->password, password); this->current_fd = fd; };
	User(int id, const char* username, const char* password) { this->id = id; strcpy(this->username, username); strcpy(this->password, password); };
};
struct connection_details
{
	const char* server, * user, * password, * database;
};

typedef struct thData {
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

std::vector<User*> users;

//some polymorphism :)
User* get_user(char* username) {
	for (User* user : users) {
		if (!strcmp(user->username ,username)) {
			return user;
		}
	}
	return NULL;
}
User* get_user(void* arg) {
	struct thData tdL;
	tdL = *((struct thData*)arg);
	for (User* user : users) {
		if (user->current_fd== tdL.cl) {
			return user;
		}
	}
	return NULL;
}