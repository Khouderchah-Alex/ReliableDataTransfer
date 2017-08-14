// File: rdp.cpp

#include "rdp.h"
#include "rdp_structures.h"
#include "Error.h"
#include <unistd.h>
#include <iostream>
#include <fstream>

enum EUpdateResult
{
	EUR_SYN = 1,
	EUR_SYNACK,
	EUR_ACK,
	EUR_RQST,
	EUR_DATA,
	EUR_FIN,
	EUR_FINACK,
	EUR_DROPPED
};

RdpConnection::RdpConnection() :
	m_UdpSocket(-1), m_IsListener(false), m_pAddr(nullptr),
	m_WndSize(RDP_WNDSIZE), m_WndCurr(0), m_EarliestTimeout(0),
	m_pEarliestPacket(nullptr), m_pLatestPacket(nullptr), m_NextSeq(0),
	m_MinUnacked(-1), m_SynIndex(-1), m_ReceivedFIN(false)
{
}

RdpConnection::~RdpConnection()
{
	Shutdown();
}

void RdpConnection::TestClient()
{
	RdpPacket *pPkt = new RdpPacket;
	pPkt->hdr.m_Flags = 0;
	pPkt->hdr.m_SeqNumber = m_NextSeq;
	pPkt->hdr.m_Reserved = 0;
	pPkt->hdr.m_MsgLen = sizeof(RdpHeader) + 2;
	pPkt->msg[sizeof(RdpHeader)] = 'H';
	pPkt->msg[sizeof(RdpHeader)+1] = '\0';
	Send(pPkt);
}

void RdpConnection::TestServer()
{
	int result;
	RdpPacket pkt;
	while((result = Update(&pkt)) != EUR_DATA){}

	std::cout << &(pkt.msg[sizeof(RdpHeader)]) << "\n";
}

int RdpConnection::Initialize()
{
	if(m_UdpSocket != -1)
	{
		return 0;
	}

	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
	{
		ERROR(ERR_SOCKET, false);
		return -1;
	}

	m_UdpSocket = sock;
	m_ReceivedFIN = false;
	return _Init();
}

int RdpConnection::_Init()
{
	static bool firstInit = true;
	if(firstInit)
	{
		srand(time(0));
		firstInit = false;
	}

	FD_ZERO(&m_fdsMain);
	FD_SET(m_UdpSocket, &m_fdsMain);

	#ifdef RDP_CLIENT
	m_ReceivedList.clear();
    #endif

	m_UnackedPackets.Initialize((MULT<RDP_WNDSIZE,2>::val / RDP_MSS) + 1);
	return 0;
}

void RdpConnection::Shutdown()
{
	if(m_UdpSocket != -1)
	{
		if(close(m_UdpSocket) == -1)
		{
			ERROR(ERR_CLOSE, false);
		}

		m_UdpSocket = -1;
	}

	m_pAddr = nullptr;

	m_PendingConnections.Shutdown();
}

int RdpConnection::Connect(const sockaddr *address, socklen_t address_len)
{
	m_pAddr = (sockaddr*)address;
	m_AddrLen = address_len;

	// Send SYN
	RdpPacket *pSyn = new RdpPacket;
	pSyn->hdr.m_SeqNumber = rand() % RDP_MAX_SEQNUM;
	pSyn->hdr.m_Reserved = 0;
	pSyn->hdr.m_Flags = RdpHeader::FLAG_SYN;
	pSyn->hdr.m_MsgLen = sizeof(RdpHeader);
	Send(pSyn, false, true);

	// Wait for SYN-ACK (ACK will be sent by Update())
	int result;
	while((result = Update()) != EUR_SYNACK)
	{
		if(result == -1)
		{
			m_pAddr = nullptr;
			return -1;
		}
	}

	return 0;
}

int RdpConnection::SendRequest(std::string filename)
{
	// Ensure that request can be in a single packet
	if(filename.length() >= RDP_MAX_PKTSIZE - sizeof(RdpHeader))
	{
		return -1;
	}

	// Send RQST packet
	RdpPacket *pRequest = new RdpPacket;
	pRequest->hdr.m_SeqNumber = m_NextSeq;
	pRequest->hdr.m_Reserved = 0;
	pRequest->hdr.m_Flags = RdpHeader::FLAG_RQST;
	pRequest->hdr.m_MsgLen = filename.length() + sizeof(RdpHeader) + 1;

	// Copy name into packet
	filename.copy(&(pRequest->msg[sizeof(RdpHeader)]),
				  RDP_MAX_PKTSIZE-sizeof(RdpHeader));
	pRequest->msg[sizeof(RdpHeader)+filename.length()] = '\0';

	// Send packet
	Send(pRequest);

	// Wait until ACKed
	/*
	int result;
	while((result = Update()) != EUR_ACK)
	{
		if(result == -1)
		{
			return -1;
		}
	}
	*/

	return 0;
}

int RdpConnection::RecvFile(std::string outputFile)
{
	std::ofstream outFile(outputFile, std::ios::out
						  | std::ios::trunc | std::ios::binary);
	if(!outFile)
	{
		return -1;
	}

	RdpPacket pkt;
	std::string pktStr;
	std::unordered_map<uint16_t,std::string> seqToStr;
	uint16_t expectedSeq;
	bool bReceivedFirst = false;
	bool bReceivedLast = false;
	uint16_t lastSeq;
	while(1)
	{
		if(Update(&pkt) == EUR_DATA)
		{
			pktStr = std::string(&(pkt.msg[sizeof(RdpHeader)]), pkt.hdr.m_MsgLen - sizeof(RdpHeader));

			if(!bReceivedFirst && pkt.hdr.m_Flags & RdpHeader::FLAG_FIRST)
			{
				bReceivedFirst = true;
				expectedSeq = (pkt.hdr.m_SeqNumber + pkt.hdr.m_MsgLen) % RDP_MAX_SEQNUM;
				outFile.write(&pktStr[0], pktStr.length());
			}
			else if(bReceivedFirst && pkt.hdr.m_SeqNumber == expectedSeq)
			{
				expectedSeq = (expectedSeq + pkt.hdr.m_MsgLen) % RDP_MAX_SEQNUM;
				outFile.write(&pktStr[0], pktStr.length());

				if(pkt.hdr.m_Flags & RdpHeader::FLAG_LAST)
				{
					goto close;
				}

				std::unordered_map<uint16_t,std::string>::iterator iter;
				while((iter = seqToStr.find(expectedSeq)) != seqToStr.end())
				{
					outFile.write(&iter->second[0], iter->second.length());
					if(bReceivedLast && expectedSeq == lastSeq)
					{
						goto close;
					}

					expectedSeq = (expectedSeq + iter->second.length()
								   + sizeof(RdpHeader)) % RDP_MAX_SEQNUM;
					seqToStr.erase(iter);
				}
			}
			else
			{
				// Only store out-of-order packets that are within the window
				if(!bReceivedLast && pkt.hdr.m_Flags & RdpHeader::FLAG_LAST)
				{
					bReceivedLast = true;
					lastSeq = pkt.hdr.m_SeqNumber;
				}
				seqToStr[pkt.hdr.m_SeqNumber] = pktStr;
			}
		}
	}

close:
	outFile.close();
	return 0;
}

int RdpConnection::WaitAndClose()
{
	// Wait for FIN
	int result;
	while(!m_ReceivedFIN)
	{
		if(Update() == -1)
		{
			return -1;
		}
	}

	// Send FIN
	RdpPacket *pFin = new RdpPacket;
	pFin->hdr.m_SeqNumber = m_NextSeq;
	pFin->hdr.m_Reserved = 0;
	pFin->hdr.m_Flags = RdpHeader::FLAG_FIN;
	pFin->hdr.m_MsgLen = sizeof(RdpHeader);
	Send(pFin);

	// Wait for FIN-ACK
	while((result = Update()) != EUR_FINACK)
	{
		if(result == -1)
		{
			return -1;
		}
	}

	Shutdown();
	return 0;
}

int RdpConnection::Bind(const sockaddr *address, socklen_t address_len)
{
	return ::bind(m_UdpSocket, address, address_len);
}

int RdpConnection::Listen(int backlog)
{
	if(backlog < 1)
	{
		return -1;
	}

	if(!m_IsListener)
	{
		m_IsListener = true;
		m_PendingConnections.Initialize(backlog+1);
	}

	return 0;
}

int RdpConnection::Accept(sockaddr *address, socklen_t address_len)
{
	if(m_pAddr != nullptr){ return -1; }

	PendingConnection pending;
	while(!m_PendingConnections.Pop(&pending))
	{
		Update();
	}

	// Create new connection
	m_LocalAddr = pending.addr;
	m_pAddr = &m_LocalAddr;
	m_AddrLen = sizeof(sockaddr_in);

	// Send synack
	RdpPacket *pSyn = new RdpPacket;
	pSyn->hdr.m_SeqNumber = rand() % RDP_MAX_SEQNUM;
	pSyn->hdr.m_Reserved = 0;
	pSyn->hdr.m_Flags = RdpHeader::FLAG_SYN | RdpHeader::FLAG_ACK;
	pSyn->hdr.m_MsgLen = sizeof(RdpHeader);
	Send(pSyn);

	return 0;
}

int RdpConnection::RecvRequest(std::string &filename)
{
	// Wait for request packet from client
	int result;
	RdpPacket rqst;
	while((result = Update(&rqst)) != EUR_RQST)
	{
		if(result == -1)
		{
			return -1;
		}
	}

	filename = std::string(&(rqst.msg[sizeof(RdpHeader)]));
	return 0;
}

int RdpConnection::SendFile(std::string filename)
{
	std::ifstream inFile(filename, std::ios::binary | std::ios::ate);

	if(inFile)
	{
		// Get file length
		std::streamoff len = inFile.tellg();
		inFile.seekg(0, std::ios::beg);

		// While info left, send packets until window fills, then update
		bool bFirst = true;
		while(len != 0)
		{
			int msgLen = std::min((std::streamoff)RDP_MSS, len);
			len -= msgLen;

			// Create packet
			RdpPacket *pPkt = new RdpPacket;
			pPkt->hdr.m_SeqNumber = m_NextSeq;
			pPkt->hdr.m_Reserved = 0;
			if(bFirst)
			{
				pPkt->hdr.m_Flags = RdpHeader::FLAG_FIRST;
				bFirst = false;
			}
			else if(len == 0)
			{
				pPkt->hdr.m_Flags = RdpHeader::FLAG_LAST;
			}
			else
			{
				pPkt->hdr.m_Flags = 0;
			}
			pPkt->hdr.m_MsgLen = msgLen + sizeof(RdpHeader);
			inFile.read(&(pPkt->msg[sizeof(RdpHeader)]), msgLen);
			//std::cerr.write(&(pPkt->msg[sizeof(RdpHeader)]), msgLen);

			// Spin until we have room to send another packet
			uint16_t nextSeq = m_NextSeq;
			if(m_NextSeq < m_MinUnacked)
			{
				nextSeq += RDP_MAX_SEQNUM;
			}
			while(nextSeq + pPkt->hdr.m_MsgLen - m_MinUnacked > m_WndSize ||
				  m_UnackedPackets.IsFull())
			{
				Update();
				nextSeq = m_NextSeq;
				if(m_NextSeq < m_MinUnacked)
				{
					nextSeq += RDP_MAX_SEQNUM;
				}
			}

			Send(pPkt);
		}

		inFile.close();

		// Spin until no more unacked packets
		while(m_UnackedPackets.Size() > 0)
		{
			Update();
		}

		return 0;
	}

	return -1;
}

int RdpConnection::Close()
{
	// Send FIN
	RdpPacket *pFin = new RdpPacket;
	pFin->hdr.m_SeqNumber = m_NextSeq;
	pFin->hdr.m_Reserved = 0;
	pFin->hdr.m_Flags = RdpHeader::FLAG_FIN;
	pFin->hdr.m_MsgLen = sizeof(RdpHeader);
	Send(pFin);

	// Wait for FIN-ACK and FIN
	bool bFin = false, bFinAck = false;
	int result;
	while(!bFin || !bFinAck)
	{
		result = Update();

		switch(result)
		{
		case -1:
			return -1;
		case EUR_FIN:
			bFin = true;
			break;
		case EUR_FINACK:
			bFinAck = true;
			break;
		default:
			break;
		}
	}

	// Wait for amount of time then close
	clock_t finishTime = clock() + MULT<RDP_RTO_CLK, 2>::val;
	do
	{
		if(Update() == -1)
		{
			return -1;
		}
	} while(clock() < finishTime);

	Shutdown();
	return 0;
}

int RdpConnection::Update(RdpPacket *pPkt)
{
	// Resend as needed
	static clock_t currClock;
	currClock = clock();
	Resend(currClock);

	// Select w/timeout
	fd_set fds_read = m_fdsMain;
	int result;
	timeval tv;
	tv.tv_sec = 0;
	// TODO - although this should work anyways
	tv.tv_usec = 0;
	if((result = select(m_UdpSocket+1, &fds_read, NULL, NULL, &tv)) == -1)
	{
		ERROR(ERR_SELECT, false);
	}
	else if(result == 0)
	{
		// Resend as needed
		currClock = clock();
		Resend(currClock);
	}
	else
	{
		// Read from the udp socket
		RdpPacket localPkt;
		if(!pPkt){ pPkt = &localPkt; }

		sockaddr addr;
		if(!Recv(*pPkt, &addr)){ ERROR(ERR_RECV, false); return -1; }

		// If SYN, handle only if listener
		if(pPkt->hdr.m_Flags == RdpHeader::FLAG_SYN)
		{
			if(m_IsListener)
			{
				PendingConnection pending;
				pending.addr = addr;
				pending.seqNum = pPkt->hdr.m_SeqNumber;
				m_PendingConnections.Push(pending); // Ignore if no room
				return EUR_SYN;
			}
			return EUR_DROPPED;
		}
		// Only accept packets from the currently connected client
		else if(!m_pAddr || memcmp(&addr, m_pAddr, sizeof(sockaddr)) != 0)
		{
			std::cerr << "Dropping packet!\n";
			return EUR_DROPPED;
		}
		// If SYNACK
		else if(pPkt->hdr.m_Flags == (RdpHeader::FLAG_ACK | RdpHeader::FLAG_SYN))
		{
			if(m_SynIndex != -1)
			{
				Ack(m_UnackedPackets[m_SynIndex]);
				m_SynIndex = -1;
			}

			// Send ACK
			RdpPacket ack = *pPkt;
			ack.hdr.m_Flags = RdpHeader::FLAG_ACK;
			ack.hdr.m_MsgLen = sizeof(RdpHeader);
			ack.hdr.m_Reserved = 0;
			Send(&ack);

			return EUR_SYNACK;
		}
		// If ACK, find in unacked buffer & change m_WndCurr
		else if(pPkt->hdr.m_Flags & RdpHeader::FLAG_ACK)
		{
			auto iter = m_SeqToIndex.find(pPkt->hdr.m_SeqNumber);
			if(iter != m_SeqToIndex.end())
			{
				Ack(m_UnackedPackets[iter->second]);
				m_SeqToIndex.erase(iter);
			}

			if(pPkt->hdr.m_Flags & RdpHeader::FLAG_FIN)
			{
				return EUR_FINACK;
			}
			return EUR_ACK;
		}
		// Else if FIN, handle
		else if(pPkt->hdr.m_Flags == RdpHeader::FLAG_FIN)
		{
			m_ReceivedFIN = true;

			// Send FINACK (don't new the finack!)
			pPkt->hdr.m_MsgLen = sizeof(RdpHeader);
			pPkt->hdr.m_Reserved = 0;
			pPkt->hdr.m_Flags = RdpHeader::FLAG_ACK | RdpHeader::FLAG_FIN;
			Send(pPkt);
			return EUR_FIN;
		}
		// Else, store packet message and send ACK
		else
		{
			// Send ACK
			RdpPacket ack = *pPkt;
			ack.hdr.m_Flags = RdpHeader::FLAG_ACK;
			ack.hdr.m_MsgLen = sizeof(RdpHeader);
			ack.hdr.m_Reserved = 0;
			Send(&ack);

			if(pPkt->hdr.m_Flags & RdpHeader::FLAG_RQST)
			{
				return EUR_RQST;
			}
			else
			{
				#ifdef RDP_CLIENT
				// Remove expired elements from received list
				bool bAlreadyExists = false;
				for(auto iter = m_ReceivedList.begin(); iter != m_ReceivedList.end();)
				{
					if(*iter == pPkt->hdr.m_SeqNumber)
					{
						bAlreadyExists = true;
					}

					auto diff = std::abs(*iter - pPkt->hdr.m_SeqNumber);
					if(RDP_WNDSIZE < diff && diff < RDP_MAX_SEQNUM - RDP_WNDSIZE)
					{
						iter = m_ReceivedList.erase(iter);
					}
					else
					{
						++iter;
					}
				}

				// Add current element to received list if needed
				if(!bAlreadyExists)
				{
					m_ReceivedList.push_back(pPkt->hdr.m_SeqNumber);
				}
				else
				{
					return 0; // Only return EUR_DATA first time a pkt is received
				}
				#endif

				return EUR_DATA;
			}
		}
	}

	return 0;
}

void RdpConnection::Resend(clock_t currTime)
{
	while(currTime >= m_EarliestTimeout && m_pEarliestPacket != nullptr)
	{
		// Resend packet
		Send(m_pEarliestPacket->m_pPacket, true);
		m_pEarliestPacket->m_ResendTime = clock() + RDP_RTO_CLK;

		// Update linked list
		if(m_pEarliestPacket != m_pLatestPacket)
		{
			m_pLatestPacket->m_pNext = m_pEarliestPacket;
			m_pLatestPacket = m_pEarliestPacket;
			m_pEarliestPacket = m_pEarliestPacket->m_pNext;
			m_pLatestPacket->m_pNext = nullptr;
		}

		m_EarliestTimeout = m_pEarliestPacket->m_ResendTime;
	}
}

bool RdpConnection::Send(RdpPacket *pPkt, bool isResend, bool isSyn)
{
	auto len = pPkt->hdr.m_MsgLen;

	if(!isResend && pPkt->hdr.m_Flags != RdpHeader::FLAG_ACK &&
	   pPkt->hdr.m_Flags != (RdpHeader::FLAG_ACK | RdpHeader::FLAG_FIN))
	{
		// Create unacked packet
		UnackedPacket unacked;
		unacked.m_ResendTime = clock() + RDP_RTO_CLK;
		unacked.m_pNext = nullptr;
		unacked.m_pPacket = pPkt;

		// Place into circular buffer
		int index;
		if(!m_UnackedPackets.Push(unacked, &index))
		{
			assert(0);
			return false;
		}

		m_pLatestPacket = m_UnackedPackets[index];
		if(m_pEarliestPacket == nullptr)
		{
			m_pEarliestPacket = m_pLatestPacket;
			m_EarliestTimeout = unacked.m_ResendTime;
			m_MinUnacked = pPkt->hdr.m_SeqNumber;
		}
		else
		{
			UnackedPacket *pTest = m_pEarliestPacket;
			while(pTest->m_pNext != nullptr)
			{
				pTest = pTest->m_pNext;
			}

			pTest->m_pNext = m_pLatestPacket;
		}

		if(isSyn){ m_SynIndex = index; }
		else{ m_SeqToIndex[pPkt->hdr.m_SeqNumber] = index; }

		// Update variables
		m_WndCurr += len;
		m_NextSeq = (pPkt->hdr.m_SeqNumber + len) % RDP_MAX_SEQNUM;
	}

	pPkt->hdr.hton();

	if(sendto(m_UdpSocket, pPkt->msg, len, 0, m_pAddr, m_AddrLen) == -1)
	{
		pPkt->hdr.ntoh();
		ERROR(ERR_SEND, false);
		return false;
	}

	pPkt->hdr.ntoh();

	// Print message
#if defined(RDP_SERVER) && !defined(RDP_CLIENT)
	std::cout << "Sending packet " << pPkt->hdr.m_SeqNumber << " " << m_WndSize;
	if(isResend){ std::cout << " Retransmission"; }
	if(pPkt->hdr.m_Flags & RdpHeader::FLAG_SYN){ std::cout << " SYN"; }
	if(pPkt->hdr.m_Flags & RdpHeader::FLAG_FIN){ std::cout << " FIN"; }
	std::cout << "\n";
#elif !defined(RDP_SERVER) && defined(RDP_CLIENT)
	std::cout << "Sending packet " << pPkt->hdr.m_SeqNumber;
	if(isResend){ std::cout << " Retransmission"; }
	if(pPkt->hdr.m_Flags & RdpHeader::FLAG_SYN){ std::cout << " SYN"; }
	if(pPkt->hdr.m_Flags & RdpHeader::FLAG_FIN){ std::cout << " FIN"; }
	std::cout << "\n";
#else
#error Exactly one of RDP_SERVER and RDP_CLIENT needs to be defined
#endif

	return true;
}

bool RdpConnection::Recv(RdpPacket &pkt, sockaddr *pAddr)
{
	static socklen_t len = sizeof(sockaddr_in);
	if(recvfrom(m_UdpSocket, pkt.msg, RDP_MAX_PKTSIZE, 0, pAddr, &len) == -1)
	{
		ERROR(ERR_RECV, false);
		return false;
	}

	pkt.hdr.ntoh();

	// Print message
	std::cout << "Receiving packet " << pkt.hdr.m_SeqNumber;

	#ifdef RDP_CLIENT
	for(auto i : m_ReceivedList)
	{
		if(i == pkt.hdr.m_SeqNumber)
		{
			std::cout << " Retransmission";
			break;
		}
	}
	#endif

	std::cout << "\n";

	return true;
}

void RdpConnection::Ack(UnackedPacket *pUnacked)
{
	if(pUnacked->m_pPacket == nullptr)
	{
		return;
	}

	if(!m_pEarliestPacket)
	{
		goto clear;
	}

	if(pUnacked == m_pEarliestPacket)
	{
		if(pUnacked == m_pLatestPacket)
		{
			m_pEarliestPacket = m_pLatestPacket = nullptr;
		}
		else
		{
			m_pEarliestPacket = m_pEarliestPacket->m_pNext;
		}
	}
	else
	{
		UnackedPacket *pTest = m_pEarliestPacket;
		while(pTest->m_pNext != pUnacked)
		{
			pTest = pTest->m_pNext;
		}

		pTest->m_pNext = pUnacked->m_pNext;

		if(pUnacked == m_pLatestPacket)
		{
			m_pLatestPacket = pTest;
		}
	}

	m_WndCurr -= pUnacked->m_pPacket->hdr.m_MsgLen;
	delete pUnacked->m_pPacket;
	pUnacked->m_pPacket = nullptr;

clear:
	if(pUnacked == m_UnackedPackets.Peek())
	{
		while((pUnacked = m_UnackedPackets.Peek()) &&
			  pUnacked->m_pPacket == nullptr)
		{
			m_UnackedPackets.Pop(nullptr);
		}
	}

	if((pUnacked = m_UnackedPackets.Peek()))
	{
		assert(pUnacked->m_pPacket);
		m_MinUnacked = pUnacked->m_pPacket->hdr.m_SeqNumber;
	}
	else
	{
		m_MinUnacked = -1;
	}
}

void RdpHeader::hton()
{
	m_SeqNumber = htons(m_SeqNumber);
	m_Reserved = htons(m_Reserved);
	m_MsgLen = htons(m_MsgLen);
	m_Flags = htons(m_Flags);
}

void RdpHeader::ntoh()
{
	m_SeqNumber = ntohs(m_SeqNumber);
	m_Reserved = ntohs(m_Reserved);
	m_MsgLen = ntohs(m_MsgLen);
	m_Flags = ntohs(m_Flags);
}
