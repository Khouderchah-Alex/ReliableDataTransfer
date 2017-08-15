// File: client.cpp
// Description: Source code for the Project2 client application

#include "rdt.h"
#include <netdb.h>
#include <iostream>

using namespace std;

void printHelp(char **argv);

int main(int argc, char **argv)
{
	uint16_t portNum;
	if(argc != 4 || (portNum = atol(argv[2])) == 0)
	{
		printHelp(argv);
		return -1;
	}

	RdtConnection server;
	if(server.Initialize())
	{
		ERROR(ERR_SOCKET, true);
	}

	hostent *pServer = gethostbyname(argv[1]);
	if(pServer == NULL)
	{
		ERROR(ERR_HOST, true);
	}

	sockaddr_in serv_addr;
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char*)pServer->h_addr,
		  (char*)&serv_addr.sin_addr.s_addr,
		  pServer->h_length);
	serv_addr.sin_port = htons(portNum);
	if(server.Connect((sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		ERROR(ERR_CONNECT, true);
	}

	if(server.SendRequest(argv[3]) == -1)
	{
		ERROR(ERR_SEND, true);
	}

	if(server.RecvFile("received.data") == -1)
	{
		ERROR(ERR_RECV, true);
	}

	if(server.WaitAndClose() == -1)
	{
		ERROR(ERR_CLOSE, false);
	}

	return 0;
}

void printHelp(char **argv)
{
	cout << "usage: " << argv[0] << " serverName serverPort fileName\n\n";
	cout << "Runs the rdt (reliable data protocol) client, connects to serverName:serverPort, and requests the specified file.\n";
}
