/* File: rdt.h
 * Description: Header containing the API definitions for
 *              rdt (reliable datagram transfer).
 */

#ifndef _RDT_H_
#define _RDT_H_

#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <list>

// If this is not defined, simply include a custom ERROR function/macro
// in order to do something different when errors occur
#ifdef USE_RDT_ERROR
#include "rdt_error.h"
#endif

#include "rdt_structures.h"

/**
 * @brief Class providing the top-level API
 *
 * With an instance of this class, one can can use the provided API
 * (which is similar to that of the Berkeley Sockets API) in order to have
 * reliable data transfer over UDP.
 *
 * @note This implementation does not currently perform flow or congestion
 * control.
 */
class RdtConnection
{
public:
	RdtConnection();
	~RdtConnection();

	/**
	 * @brief Sets up the data structure and creates a UDP socket
	 * @return 0 if succesful, -1 if failed
	 */
	int Initialize();

	/**
	 * @brief Clears the data structure are closes the UDP socket
	 * @TODO reject pending any clients
	 * @return 0 if successful, -1 if failed
	 */
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

private:
	int _Init();

	/**
	 * @brief Updates the rdt state (sends/receives ACKs/data/SYNACKs/etc)
	 *
	 * This function should be placed inside of a loop such that the rdt protocol
	 * will behave as expected. Note that the typical socket API does not contain this
	 * function because such operations are performed by the OS usually.
	 *
	 * @return -1 on error, 0 for normal call
	 */
	int Update(RdtPacket *pPkt=nullptr);

	bool Send(RdtPacket *pPkt, bool isResend=false, bool isSyn=false);
	bool Recv(RdtPacket &pkt, sockaddr *pAddr=nullptr);
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

	std::list<uint16_t> m_ReceivedList;
};

#endif //_RDT_H_
