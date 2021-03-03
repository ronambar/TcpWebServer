
#include "TcpWebServer.h"		
void main()
{
	WSAData wsaData;
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(LISTEN_PORT);
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	addSocket(listenSocket, LISTEN);
	while (true)
	{
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					time_t now;
					time(&now);
					if (now - sockets[i].timeEntered > THREE_MINUTES)
					{
						closesocket(sockets[i].id);
						removeSocket(i);
					}
					else
						sendMessage(i);
					break;
				}
			}
		}
	}

	cout << "Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index) // index probably be 0 - its the only one listenSocket we created and add first to the sockets array
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr *)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else 
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; 
		cout << "Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = GET;
				memcpy(sockets[index].buffer, &sockets[index].buffer[3], sockets[index].len - 3);
				sockets[index].len -= 3;
				sockets[index].buffer[sockets[index].len] = '\0';
				time(&sockets[index].timeEntered);
				return;
			}
			if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = HEAD;
				memcpy(sockets[index].buffer, &sockets[index].buffer[4], sockets[index].len - 4);
				sockets[index].len -= 4;
				sockets[index].buffer[sockets[index].len] = '\0';
				time(&sockets[index].timeEntered);
				return;
			}
			if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = OPTIONS;
				memcpy(sockets[index].buffer, &sockets[index].buffer[7], sockets[index].len - 7);
				sockets[index].len -= 7;
				sockets[index].buffer[sockets[index].len] = '\0';
				time(&sockets[index].timeEntered);
				return;
			}
			if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = PUT;
				memcpy(sockets[index].buffer, &sockets[index].buffer[3], sockets[index].len - 3);
				sockets[index].len -= 3;
				sockets[index].buffer[sockets[index].len] = '\0';
				time(&sockets[index].timeEntered);
				return;
			}
			if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = POST;
				memcpy(sockets[index].buffer, &sockets[index].buffer[4], sockets[index].len - 4);
				sockets[index].len -= 4;
				sockets[index].buffer[sockets[index].len] = '\0';
				time(&sockets[index].timeEntered);
				return;
			}
			if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = DELET_RESOURCE;
				memcpy(sockets[index].buffer, &sockets[index].buffer[6], sockets[index].len - 6);
				sockets[index].len -= 6;
				sockets[index].buffer[sockets[index].len] = '\0';
				time(&sockets[index].timeEntered);
				return;
			}
			if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = TRACE;
				time(&sockets[index].timeEntered);
				return;
			}
			
			sockets[index].send = SEND;
			sockets[index].sendSubType = INVALID_REQUEST;
			time(&sockets[index].timeEntered);
			return;

		}

	}
}

void sendMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;
	int bytesSent = 0, logSize = 0, phySize = 255;
	char* sendBuff = (char*)malloc(sizeof(char)*phySize);
	char tmpBuffer[MAX_STR_LEN];
	char filePath[MAX_STR_LEN];
	char fileType[20];
	char language[5];
	char currTimeStr[50];
	getTimeStr(currTimeStr);
	char *fileName;
	int fileLen, numOfLines;
	char fileLenStr[20];

	if (sockets[index].sendSubType == GET || sockets[index].sendSubType == HEAD)
	{
		strcpy(tmpBuffer, sockets[index].buffer + 2);
		fileName = tmpBuffer;
		fileName = strtok(fileName, " ");
		getFileType(fileName, fileType);
		getLangQueryString(fileName, language); 

		if (strcmp(language, "NONE") != 0 && strcmp(language, "he") != 0 && strcmp(language, "en") != 0)
			objectNotFound(sendBuff, currTimeStr);
		else
		{
			if (strcmp(language, "he") == 0)
				strcpy(filePath, HE_PUBLIC_FILES);
			else
				strcpy(filePath, EN_PUBLIC_FILES);

			strcpy(filePath + strlen(filePath), fileName);
			FILE* fp = fopen(filePath, "r");

			if (fp == NULL)
				objectNotFound(sendBuff, currTimeStr);
			else
			{
				fseek(fp, 0, SEEK_END);
				fileLen = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				numOfLines = getNumOfLines(fp);
				fileLen -= numOfLines;
				_itoa(fileLen, fileLenStr, 10);
				sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s%s", OK_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH,
					fileLenStr, NEWLINE, strcmp(fileType, "html") == 0 ? CONTENT_TYPE_HTML : CONTENT_TYPE_TXT, NEWLINE, NEWLINE);
				logSize = strlen(sendBuff);
				if (sockets[index].sendSubType == GET)
				{
					readFromFile(&sendBuff, fp , &logSize, &phySize);
				}

				fclose(fp);
			}

		}
	}

	else if (sockets[index].sendSubType == OPTIONS)
	{
		sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s%s",
			OK_MSG, NEWLINE, ALLOW_OPTIONS, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE, CONTENT_TYPE_HTML, NEWLINE, NEWLINE);
	}

	else if (sockets[index].sendSubType == PUT)
	{
		FILE* fp;
		strcpy(tmpBuffer, sockets[index].buffer + 2);
		fileName = tmpBuffer;
		fileName = strtok(fileName, " ");
		getFileType(fileName, fileType);
		getLangQueryString(fileName, language);
		strcpy(filePath, EN_PUBLIC_FILES);
		strcpy(filePath + strlen(filePath), fileName);
		char bodyBuff[MAX_STR_LEN] = "\0";
		getBodyContent(sockets[index].buffer, bodyBuff);
		fp = fopen(filePath, "r");
		if (fp == NULL) 
		{
			fp = fopen(filePath, "w");
			if (fp != NULL) // create new file , code 201
			{
				if (strlen(bodyBuff) == 0)
				{
					sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
						NO_CONTENT_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE,
						strcmp(fileType, "html") == 0 ? CONTENT_TYPE_HTML : CONTENT_TYPE_TXT, NEWLINE, NEWLINE);
				}
				else
				{
					fprintf(fp, "%s", bodyBuff);
					sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
						CREATED_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE,
						strcmp(fileType, "html") == 0 ? CONTENT_TYPE_HTML : CONTENT_TYPE_TXT, NEWLINE, NEWLINE);
				}

				fclose(fp);
			}
			else // can not open/create file , code 501
			{
				sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
					NOT_IMPLEMENTED_MSG, NEWLINE, currTimeStr, NEWLINE, NEWLINE, CONTENT_LENGTH0, NEWLINE, 
					strcmp(fileType, "html") == 0 ? CONTENT_TYPE_HTML : CONTENT_TYPE_TXT, NEWLINE, NEWLINE);
			}
		}
		else // file exist , update the file , code 200
		{
			fclose(fp);
			fp = fopen(filePath, "w");
			if (strlen(bodyBuff) == 0) // no body, code 204
			{
				sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
					NO_CONTENT_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE, 
					strcmp(fileType, "html") == 0 ? CONTENT_TYPE_HTML : CONTENT_TYPE_TXT, NEWLINE, NEWLINE);
			}
			else
			{
				fprintf(fp, "%s", bodyBuff);
				sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
					OK_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE, 
					strcmp(fileType, "html") == 0 ? CONTENT_TYPE_HTML : CONTENT_TYPE_TXT, NEWLINE, NEWLINE);
			}

			fclose(fp);
		}

	}

	else if (sockets[index].sendSubType == POST)
	{
		char bodyBuff[MAX_STR_LEN];
		getBodyContent(sockets[index].buffer, bodyBuff);
		cout << "Clinet POST: " << bodyBuff << endl;
		sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
			OK_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE, CONTENT_TYPE_HTML, NEWLINE, NEWLINE);
	}

	else if (sockets[index].sendSubType == DELET_RESOURCE)
	{
		int hasBeenDeletedHe, hasBeenDeletedEn;
		strcpy(tmpBuffer, sockets[index].buffer + 2);
		fileName = tmpBuffer;
		fileName = strtok(fileName, " ");
		strcpy(filePath, HE_PUBLIC_FILES);
		strcpy(filePath + strlen(filePath), fileName);
		hasBeenDeletedHe = remove(filePath);

		strcpy(filePath, EN_PUBLIC_FILES);
		strcpy(filePath + strlen(filePath), fileName);

		hasBeenDeletedEn = remove(filePath);

		if (hasBeenDeletedHe != 0 && hasBeenDeletedEn != 0)
		{
			sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s", NO_CONTENT_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0,
				NEWLINE, CONTENT_TYPE_HTML, NEWLINE, NEWLINE);
		}
		else
		{
			sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s", OK_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE,
				CONTENT_TYPE_HTML, NEWLINE, NEWLINE);
		}
	}

	else if (sockets[index].sendSubType == TRACE)
	{
		int len = strlen(sockets[index].buffer);
		_itoa(len, fileLenStr, 10);
		sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s%s", OK_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH, fileLenStr,
			NEWLINE, CONTENT_TYPE_TRACE, NEWLINE, NEWLINE);
		logSize = strlen(sendBuff);
		if (logSize + len > phySize)
		{
			phySize *= 2;
			sendBuff = (char*)realloc(sendBuff, sizeof(char)*phySize);
		}

		strcat(sendBuff, sockets[index].buffer);
		strcat(sendBuff, NEWLINE);
		logSize = strlen(sendBuff);
		sendBuff = (char*)realloc(sendBuff, sizeof(char)*logSize + 1);
		sendBuff[logSize] = '\0';
	}
	else if (sockets[index].sendSubType == INVALID_REQUEST)
	{
		sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s", INVALID_REQUEST_TYPE_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE,
		CONTENT_TYPE_HTML, NEWLINE, NEWLINE);
	}

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "TCP Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "TCP Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";
	free(sendBuff);
	sockets[index].send = IDLE;
}

void getTimeStr(char* bufferTime)
{
	strcpy(bufferTime, DATE_MSG);
	struct tm *currTime;
	time_t localTime;
	localTime = time(&localTime);
	strcpy(bufferTime + strlen(bufferTime), ctime(&localTime));
	bufferTime[strlen(bufferTime) - 1] = '\0';
}

void getLangQueryString(char* fileName, char* language)
{
	char* quertStr = fileName;
	quertStr = strtok(quertStr, "?");
	quertStr = strtok(NULL, "?");
	if (quertStr == NULL)
		strcpy(language, "NONE");
	else
		strcpy(language, quertStr + 5);
}

void getFileType(char* fileName, char*  fileType)
{
	char tmpFileName[MAX_STR_LEN];
	strcpy(tmpFileName, fileName);
	char* quertStr = tmpFileName;
	quertStr = strtok(quertStr, ".");
	quertStr = strtok(NULL, "?");
	strcpy(fileType, quertStr);
}

void readFromFile(char** sendBuff, FILE* fp, int* logSize,  int* phySize)
{
	char letter;
	char line[100];
	while (!feof(fp))
	{
		if (*logSize + 100 > *phySize)
		{
			*phySize *= 2;
			*sendBuff = (char*)realloc(*sendBuff, sizeof(char)*(*phySize));
		}

		fgets(line, 100, fp);
		strcat(*sendBuff, line);
		*logSize = strlen(*sendBuff);
	}

	*sendBuff = (char*)realloc(*sendBuff, sizeof(char)*((*logSize) + 1));
	(*sendBuff)[*logSize] = '\0';
}

int getNumOfLines(FILE* fp)
{
	char letter;
	int numOfLines = 0;
	while (!feof(fp))
	{
		letter = fgetc(fp);
		if (letter == '\n')
			numOfLines++;
	}

	fseek(fp, 0, SEEK_SET);
	return numOfLines;
}

void getBodyContent(char* recvBuffer, char* bodyBuffer)
{
	printf("%s \n", recvBuffer);
	if (strlen(recvBuffer) < 4) {
		cout << "Error at sendBuff\n";
	}

	char doubleNewLine[4] = { '\r','\n','\r','\n' };
	int doubleNewLineIndex = 0;
	int resIndex = -1;

	while (recvBuffer[doubleNewLineIndex + 4] != '\0')
	{
		if (strncmp(recvBuffer + doubleNewLineIndex, doubleNewLine, 4) == 0) 
		{
			resIndex = doubleNewLineIndex;
			break;
		}

		doubleNewLineIndex++;
	}

	if (resIndex != -1)
		strcpy(bodyBuffer, recvBuffer + doubleNewLineIndex + 4);


}

void objectNotFound(char* sendBuff, char* currTimeStr)
{
	sprintf(sendBuff, "%s%s%s%s%s%s%s%s%s",
		NOT_FOUND_MSG, NEWLINE, currTimeStr, NEWLINE, CONTENT_LENGTH0, NEWLINE, CONTENT_TYPE_HTML, NEWLINE, NEWLINE);
}