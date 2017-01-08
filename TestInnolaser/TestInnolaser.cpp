// TestInnolaser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#define _USE_MATH_DEFINES

#include <math.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512

char PHPSESSID[DEFAULT_BUFLEN];
char BUFFER[DEFAULT_BUFLEN] = "\0";

bool InitializeWinSock2()
{
	int iResult;
	WSADATA wsadata;

	// Initialize winsock2
	iResult = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (iResult != 0)
	{
		printf("Unable to start winsock: %d", iResult);
		return false;
	}

	return true;
}

bool connectToServer(SOCKET &connectionSocket, char* Host, char* Port)
{
	addrinfo *result = NULL, *ptr = NULL, hints;
	int iResult;

	ZeroMemory(&hints, sizeof(addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(Host, Port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return false;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		connectionSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (connectionSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			return false;
		}

		// Connect to server.
		iResult = connect(connectionSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(connectionSocket);
			connectionSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (connectionSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		return false;
	}

	return true;
}

bool reqHTTP(char* host, char* port, char* request, char* response, int buflen)
{
	int iResult;

	// connect to the server
	SOCKET connectionSocket = INVALID_SOCKET;
	connectToServer(connectionSocket, host, port);

	// send request
	iResult = send(connectionSocket, request, strlen(request), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(connectionSocket);
		return false;
	}

	// wait for incoming data
	iResult = recv(connectionSocket, response, buflen, 0);

	// if no data received
	if (iResult == 0)
	{
		closesocket(connectionSocket);
		return false;
	}

	// End of response
	response[iResult] = 0; 
	closesocket(connectionSocket);
	return true;
}

bool parseHTTP(char* responseHTTP, bool (*header_callback)(char* name, char* value))
{
	char* context = NULL;
	char* subcontext = NULL;

	char* header = strtok_s(responseHTTP, "\r\n", &context);

	do
	{
		char* name = strtok_s(header, ":", &subcontext);
		char* value = strtok_s(NULL, "\r\n", &subcontext);

		if (header_callback(name, value))
			return true;

		header = strtok_s(NULL, "\r\n", &context);
	} while (header != NULL);

	return false;
}

bool cookie_handler(char* name, char* value)
{
	if (strcmp(name, "Set-Cookie") == 0)
	{
		char* context = NULL;
		char* subcontext = NULL;
		char* option = strtok_s(value, "; ", &context);

		do
		{
			char* name = strtok_s(option, "=", &subcontext);
			char* value = strtok_s(NULL, ";= ", &subcontext);

			if (strcmp(name, "PHPSESSID") == 0)
			{ 
				strcpy_s(PHPSESSID, DEFAULT_BUFLEN, value);
				return true;
			}

			option = strtok_s(NULL, ";", &context);
		} while (option != NULL);
	}

	return false;
}

int main()
{
	char* authenticationStr = "GET http://innolaser-service:3000/device_auth.php?login=vlad&password=ovchin_1988 HTTP/1.1\r\nHost: innolaser-service.ru\r\n\r\n";
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int iResult;

	if (InitializeWinSock2())
	{

		reqHTTP("innolaser-service.ru", "3000", authenticationStr, recvbuf, recvbuflen);

		if (parseHTTP(recvbuf, cookie_handler))
		{
			float temperature = 0.0f;
			int frequency = 10;
			int power = 100;

			for (int i = 0; i < 10000; i++)
			{
				temperature = 20.0f + 10.0f * sinf(float(i) * 0.01f * M_PI * 2.0f);
				power = i % 100;
				frequency = i % 10;
				sprintf_s(BUFFER, DEFAULT_BUFLEN,
					"GET http://innolaser-service:3000/device_update.php?cooling_level=%d&working=%d&cooling=%d&peltier=%d&temperature=%.1f&frequency=%d&power=%d \
				HTTP/1.1\r\nHost: innolaser-service.ru\r\nCookie: PHPSESSID=%s; path=/\r\n\r\n", 6, 1, 1, 1, temperature, frequency, power, PHPSESSID);
				reqHTTP("innolaser-service.ru", "3000", BUFFER, recvbuf, recvbuflen);

				Sleep(100);
			}
		}
	}

	WSACleanup();

    return 0;
}

