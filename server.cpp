// File: server.cpp
// Description: Source code for the Project2 server application

#include "rdp.h"
#include "Error.h"
#include <iostream>

#define BACKLOG 10

using namespace std;

void printHelp(char **argv);

int main(int argc, char **argv)
{
	uint16_t portNum;
	if(argc != 2 || (portNum = atol(argv[1])) == 0)
	{
		printHelp(argv);
		return -1;
	}

	RdpConnection listener;
	if(listener.Initialize() == -1)
	{
		ERROR(ERR_SOCKET, true);
	}

	sockaddr_in serv_addr;
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portNum);
	if(listener.Bind((sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		ERROR(ERR_BIND, true);
	}

	if(listener.Listen(BACKLOG) == -1)
	{
		ERROR(ERR_LISTEN, true);
	}

	sockaddr client_addr;
	if(listener.Accept(&client_addr, sizeof(client_addr)) == -1)
	{
		ERROR(ERR_ACCEPT, true);
	}

	std::string filename;
	if(listener.RecvRequest(filename) == -1)
	{
		ERROR(ERR_RECV, true);
	}

	if(listener.SendFile(filename) == -1)
	{
		ERROR(ERR_SEND, true);
	}

	if(listener.Close() == -1)
	{
		ERROR(ERR_CLOSE, true);
	}

	return 0;
}

void printHelp(char **argv)
{
	cout << "usage: " << argv[0] << " portNum\n\n";
	cout << "Runs the rdp (reliable data protocol) server with the given port number.\n";
}
