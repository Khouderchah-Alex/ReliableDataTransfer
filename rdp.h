// File: rdp.h
// Description: Header containing the API definitions for
//              rdp (reliable datagram protocol).

#ifndef _RDP_H_
#define _RDP_H_

#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <list>

#include "rdp_structures.h"

class RdpConnection
{
public:
	RdpConnection();
	~RdpConnection();

	int Initialize();
	void Shutdown();

	/**
	 * @brief Begin 3-way handshake with specified host
	 * @note Blocks until the SYNACK is received
	 * @return 0 if successful, -1 if failed
	 */
	int Connect(const sockaddr *address, socklen_t address_len);

	/**
	 * @brief Send a file request to the server
	 * @note Blocks until the request is ACKed
	 * @return 0 if successful, -1 if failed
	 */
	int SendRequest(std::string filename);

	/**
	 * @brief Receive the file data
	 * @note Blocks until the entire file is received
	 * @return 0 if successful, -1 if failed
	 */
	int RecvFile(std::string outputFile);

	/**
	 * @brief Wait for FIN and then close connection
	 * @note Blocks until FIN is received
	 * @return 0 if successful, -1 if failed
	 */
	int WaitAndClose();


	/**
	 * @brief Bind current connection to specified address
	 * @return 0 if successful, -1 if failed
	 */
	int Bind(const sockaddr *address, socklen_t address_len);

	/**
	 * @brief Set up connection to listen for SYNs
	 * @return 0 if successful, -1 if failed
	 */
	int Listen(int backlog);

	/**
	 * @brief Accept first pending connection
	 * @return 0 if successful, -1 if failed to connect
	 */
	int Accept(sockaddr *address, socklen_t address_len);

	/**
	 * @brief Wait for a file request from the host
	 * @note Blocks until a file request is received
	 * @return 0 if successful, -1 if failed
	 */
	int RecvRequest(std::string &filename);

	/**
	 * @brief Send file contents to host
	 * @note Blocks until the file has been completely transferred
	 * @return 0 if succesful, -1 if failed
	 */
	int SendFile(std::string filename);

	/**
	 * @brief Send FIN
	 * @note Blocks until the FIN-ACK is received
	 * @return 0 if successful, -1 if failed
	 */
	int Close();

	void TestClient();
	void TestServer();

private:
	int _Init();

	/**
	 * @brief Updates the rdp state (sends/receives ACKs/data/SYNACKs/etc)
	 *
	 * This function should be placed inside of a loop such that the rdp protocol
	 * will behave as expected. Note that the typical socket API does not contain this
	 * function because such operations are performed by the OS usually.
	 *
	 * @return -1 on error, 0 for normal call
	 */
	int Update(RdpPacket *pPkt=nullptr);

	bool Send(RdpPacket *pPkt, bool isResend=false, bool isSyn=false);
	bool Recv(RdpPacket &pkt, sockaddr *pAddr=nullptr);
	void Resend(clock_t currTime);
	void Ack(UnackedPacket *pUnacked);

private:
	int m_UdpSocket;
	fd_set m_fdsMain;
	sockaddr m_LocalAddr;
	sockaddr *m_pAddr;
	socklen_t m_AddrLen;

	uint16_t m_WndSize;
	uint16_t m_WndCurr;

	// Listener variables
	bool m_IsListener;
	CircularBuffer<PendingConnection> m_PendingConnections;

	// Ack variables
	CircularBuffer<UnackedPacket> m_UnackedPackets;
	std::unordered_map<uint16_t,uint16_t> m_SeqToIndex; // Maps seq# to buffer index
	clock_t m_EarliestTimeout;
	UnackedPacket *m_pEarliestPacket;
	UnackedPacket *m_pLatestPacket;
	uint16_t m_NextSeq;
	uint16_t m_MinUnacked;
	int m_SynIndex;

	bool m_ReceivedFIN;

	#ifdef RDP_CLIENT
	std::list<uint16_t> m_ReceivedList;
	#endif
};

#endif //_RDP_H_
