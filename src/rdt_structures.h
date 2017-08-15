// File: rdt_structures.h
// Description: Header containing the internal structs and classes used
//              by the rdt protocol. Should not be included by user code.

#ifndef _RDT_STRUCTURES_H_
#define _RDT_STRUCTURES_H_

#include <cstdint>
#include <arpa/inet.h>
#include <cassert>

template<int N, int M> struct DIV{ enum{ val = N/M }; };
template<int N, int M> struct MULT{ enum{ val = N * M }; };

#define RDT_MAX_SEQNUM 30720 // Sequence numbers are in bytes
#define RDT_HALF_SEQSIZE DIV<RDT_MAX_SEQNUM,2>::val
#define RDT_WNDSIZE 5120 // Window size defined in bytes
#define RDT_RTO_MS 500 // Defined in ms
#define RDT_RTO_CLK DIV<MULT<RDT_RTO_MS,CLOCKS_PER_SEC>::val, 1000>::val
#define RDT_MAX_PKTSIZE 1024
#define RDT_MSS (RDT_MAX_PKTSIZE - sizeof(RdtHeader) - 1)
#define RDT_MAX_CONNECTIONS 64

template<typename T>
class CircularBuffer
{
public:
	CircularBuffer() : m_Size(0), m_ReadIndex(0), m_WriteIndex(0),
							m_pData(nullptr){}
	~CircularBuffer(){ Shutdown(); }

	void Initialize(int i){ assert(i > 0); m_Size = i; m_pData = new T[m_Size]; }
	void Shutdown()
	{
		if(m_pData)
		{
			delete[] m_pData;
			m_pData = nullptr;
			m_ReadIndex = 0;
			m_WriteIndex = 0;
		}
	}

	bool Push(const T &elem, int *pIndex=nullptr)
	{
		if(abs(m_ReadIndex - m_WriteIndex) >= m_Size-1){ return false; }
		m_pData[m_WriteIndex] = elem;
		if(pIndex){ *pIndex = m_WriteIndex; }
		m_WriteIndex = (m_WriteIndex+1) % m_Size;
		return true;
	}

	bool Pop(T *pElem)
	{
		if(m_ReadIndex == m_WriteIndex){ return false; }
		if(pElem){ *pElem = m_pData[m_ReadIndex]; }
		m_ReadIndex = (m_ReadIndex+1) % m_Size;
		return true;
	}

	size_t Size() const{ return abs(m_ReadIndex - m_WriteIndex); }

	bool IsFull() const{ return Size() >= m_Size-1; }

	void Clear(){ m_WriteIndex = m_ReadIndex; }

	T *operator[](int index)
	{
		if((unsigned)index >= (unsigned)m_Size)
		{
			return nullptr;
		}

		return &m_pData[index];
	}

	T *Peek()
    {
		if(m_ReadIndex == m_WriteIndex){ return nullptr; }

		return &m_pData[m_ReadIndex];
	}

private:
	int m_Size;
	int m_ReadIndex;
	int m_WriteIndex;
	T *m_pData;
};

struct PendingConnection
{
	sockaddr addr;
	uint32_t seqNum;
};

struct SendQueueElem
{
	size_t bufLen;
	void *pBuffer;
};

/**
 * @brief Packet header definition
 *
 * @note We don't need port numbers in this struct since this is an application
 *       level protocol. Also UDP should be doing checksumming for us, so a
 *       checksum field isn't necessary (of course, we could implement a more
 *       robust checksum system if UDP's simple checksum wasn't good enough).
 */
struct RdtHeader
{
	uint16_t m_SeqNumber;
	uint16_t m_Reserved;

	uint16_t m_MsgLen;
	uint16_t m_Flags;

	enum EFlags
	{
		FLAG_SYN   =  0x1,
		FLAG_FIN   =  0x2,
		FLAG_ACK   =  0x4,
		FLAG_RQST  =  0x8,
		FLAG_FIRST =  0x10,
		FLAG_LAST  =  0x20,
	};

	void ntoh();
	void hton();
};

struct RdtPacket
{
	union
	{
		RdtHeader hdr;
		char msg[RDT_MAX_PKTSIZE];
	};
};

struct UnackedPacket
{
	UnackedPacket() : m_ResendTime(0), m_pNext(nullptr), m_pPacket(nullptr){}
	clock_t m_ResendTime;
	UnackedPacket *m_pNext;
	RdtPacket *m_pPacket;  // Points to packet if unacked, otherwise is nullptr
};

#endif //_RDT_STRUCTURES_H_
