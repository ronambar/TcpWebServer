
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[1024];
	int len;
	time_t timeEntered;
};

const int MAX_STR_LEN = 1024;
const int LISTEN_PORT = 8080;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int SEND_TIME = 1;
const int SEND_SECONDS = 2;

const int GET = 3;
const int HEAD = 4;
const int OPTIONS = 5;
const int PUT = 6;
const int POST = 7;
const int DELET_RESOURCE = 8;
const int TRACE = 9;
const int INVALID_REQUEST = 10;

const char* OK_MSG = "HTTP/1.1 200 OK";
const char* NEWLINE = "\r\n";
const char* CONTENT_LENGTH = "Content-Length: ";
const char* CONTENT_LENGTH0 = "Content-Length: 0";
const char* CONTENT_TYPE_HTML = "Content-Type: text/html";
const char* CONTENT_TYPE_TXT = "Content-Type: text/plain";
const char* CONTENT_TYPE_TRACE = "Content-Type: message/http";
const char* NOT_FOUND_MSG = "HTTP/1.1 404 Not Found";
const char* NO_CONTENT_MSG = "HTTP/1.1 204 No Content";
const char* INVALID_REQUEST_TYPE_MSG = "HTTP/1.1 405 Method Not Allowed";
const char* NOT_IMPLEMENTED_MSG = "HTTP/1.1 501 Not Implemented";
const char* CREATED_MSG = "HTTP/1.1 201 Created";
const char* ALLOW_OPTIONS = "Allow: GET, HEAD, PUT, DELETE, OPTIONS, TRACE\n";
const char* DATE_MSG = "Date: ";

const char* HE_PUBLIC_FILES = "publicFiles\\he\\";
const char* EN_PUBLIC_FILES = "publicFiles\\en\\";

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
void getTimeStr(char* bufferTime);
void getLangQueryString(char* fileName, char* language);
void readFromFile(char** sendBuff, FILE* fp, int* logSize, int* phySize);
void getFileType(char* fileName, char*  fileType);
int getNumOfLines(FILE* fp);
void getBodyContent(char* recvBuffer, char* bodyBuffer);
void objectNotFound(char* sendBuff, char* currTimeStr);

#define THREE_MINUTES 180

