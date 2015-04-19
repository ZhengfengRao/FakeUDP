#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

typedef struct
{
    uint8_t ip_hl : 4;
    uint8_t ip_v : 4;
    uint8_t ip_tos;
    uint16_t ip_len;
    uint16_t ip_id;
    uint16_t ip_off;
    uint8_t ip_ttl;
    uint8_t ip_p;
    uint16_t ip_sum;
    uint32_t ip_src;
    uint32_t ip_dst;
} __attribute__((packed)) IpHeader;

typedef struct
{
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
} __attribute__((packed)) UdpHeader;

typedef struct
{
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_len;
} __attribute__((packed)) PseudoUdpHeader;

struct FakeUDP
{
public:
    char *m_pUserData;
    size_t m_nMaxUserDataLen;

    FakeUDP(const char* pSourceIp
            , uint16_t nSourcePort
            , const char* pDestinationIp
            , uint16_t nDestinationPort
            , size_t nMaxUserDataLen = 1024) :
    m_pSourceIp(pSourceIp)
    , m_nSourcePort(nSourcePort)
    , m_pDestinationIp(pDestinationIp)
    , m_nDestinationPort(nDestinationPort)
    , m_nMaxUserDataLen(nMaxUserDataLen)
    , m_nSocket(0)
    , m_pBuffer(NULL)
    , m_pUserData(NULL)
    , m_pIpHeader(NULL)
    , m_pUdpHeader(NULL)
    , m_pPseudoHeader(NULL)
    , m_bInited(false)
    , m_uSourceIp(0)
    , m_uDestinationIp(0)
    {
        m_nDestionationAddrLen = sizeof (m_DestinationAddr);
        m_nIpHeaderLen = sizeof (IpHeader);
        m_nUdpHeaderLen = sizeof (UdpHeader);
        m_nPseudoHeaderLen = sizeof (PseudoUdpHeader);
        m_nBufferLen = m_nIpHeaderLen + m_nUdpHeaderLen + m_nMaxUserDataLen;
    }

    ~FakeUDP()
    {
        DeInit();
    }

    bool Init()
    {
        if (true == m_bInited)
        {
            printf("This object has been init already, do not involk Init() more then once!");
            return false;
        }

        int on = 1;

        //check param
        if (NULL == m_pSourceIp || NULL == m_pDestinationIp)
        {
            printf("Init() failed, invalid param[pSourceIp:%s, pDestinationIp:%s]\n", m_pSourceIp, m_pDestinationIp);
            goto ERROR;
        }

        if (inet_pton(AF_INET, m_pSourceIp, &m_uSourceIp) <= 0)
        {
            printf("Init() failed, invalid param[pSourceIp:%s]\n", m_pSourceIp);
            goto ERROR;
        }
        if (inet_pton(AF_INET, m_pDestinationIp, &m_uDestinationIp) <= 0)
        {
            printf("Init() failed, invalid param[pDestinationIp:%s]\n", m_pDestinationIp);
            goto ERROR;
        }
        memset(&m_DestinationAddr, 0, m_nDestionationAddrLen);
        m_DestinationAddr.sin_family = AF_INET;
        m_DestinationAddr.sin_addr.s_addr = m_uDestinationIp;
        m_DestinationAddr.sin_port = htons(m_nDestinationPort);

        //create socket
        if ((m_nSocket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) == -1)
        {
            printf("Init() failed, create socket failed! %s\n", strerror(errno));
            goto ERROR;
        }
        if (setsockopt(m_nSocket, IPPROTO_IP, IP_HDRINCL, &on, sizeof (on)) == -1)
        {
            printf("Init() failed, setsockopt failed! %s\n", strerror(errno));
            goto ERROR;
        }

        //allocate buffer
        m_pBuffer = new char[m_nBufferLen];
        memset(m_pBuffer, 0, m_nBufferLen);
        if (NULL == m_pBuffer)
        {
            printf("Init() failed, could not allocate %d bytes, out of memory!\n", m_nBufferLen);
            goto ERROR;
        }

        m_pIpHeader = (IpHeader *) m_pBuffer;
        m_pUdpHeader = (UdpHeader *) (m_pIpHeader + 1);
        m_pUserData = (char *) (m_pUdpHeader + 1);
        m_pPseudoHeader = (PseudoUdpHeader *) ((char *) m_pUdpHeader - m_nPseudoHeaderLen);

        //set section
        m_pPseudoHeader->src_ip = m_uSourceIp;
        m_pPseudoHeader->dst_ip = m_uDestinationIp;
        m_pPseudoHeader->zero = 0;
        m_pPseudoHeader->protocol = IPPROTO_UDP;
        m_pUdpHeader->source = htons(m_nSourcePort);
        m_pUdpHeader->dest = htons(m_nDestinationPort);
        m_pUdpHeader->check = 0;

        m_bInited = true;
        return m_bInited;

ERROR:
        DeInit();
        return false;
    }

    void DeInit()
    {
        if (m_nSocket > 0)
        {
            close(m_nSocket);
        }

        if (m_pBuffer != NULL)
        {
            delete m_pBuffer;
            m_pBuffer = NULL;
        }

        m_nSocket = 0;
        m_pBuffer = NULL;
        m_pUserData = NULL;
        m_pIpHeader = NULL;
        m_pUdpHeader = NULL;
        m_pPseudoHeader = NULL;
        m_bInited = false;
    }

    int Send(size_t nUserDataLen)
    {
        m_pPseudoHeader->udp_len = htons(m_nUdpHeaderLen + nUserDataLen);
        m_pUdpHeader->len = htons(m_nUdpHeaderLen + nUserDataLen);

        size_t udp_check_len = m_nPseudoHeaderLen + m_nUdpHeaderLen + nUserDataLen;
        if (nUserDataLen % 2 != 0)
        {
		    udp_check_len += 1;
		    *(m_pUserData + nUserDataLen) = 0;
        }
        m_pUdpHeader->check = CalcChecksum(m_pPseudoHeader, udp_check_len);

        m_pIpHeader->ip_hl = sizeof (*m_pIpHeader) / sizeof (uint32_t);
        m_pIpHeader->ip_v = 4;
        m_pIpHeader->ip_tos = 0;
        m_pIpHeader->ip_len = htons(m_nBufferLen);
        m_pIpHeader->ip_id = htons(0); //为0，协议栈自动设置
        m_pIpHeader->ip_off = htons(0);
        m_pIpHeader->ip_ttl = 255;
        m_pIpHeader->ip_p = IPPROTO_UDP;
        m_pIpHeader->ip_src = m_uSourceIp;
        m_pIpHeader->ip_dst = m_uDestinationIp;
        m_pIpHeader->ip_sum = 0;
        //协议栈总是自动计算与填充
        //m_pIpHeader->ip_sum = CalcChecksum(ip_header, sizeof(*ip_header));

        size_t nPacketLen = m_nIpHeaderLen + m_nUdpHeaderLen + nUserDataLen;
        if(sendto(m_nSocket, m_pBuffer, nPacketLen, 0, (struct sockaddr *) &m_DestinationAddr, sizeof (m_DestinationAddr))
                != nPacketLen)
        {
            printf("Send() failed! %s\n", strerror(errno));
            return -1;
        }

        return 0;
    }

private:
    int m_nSocket;
    char* m_pBuffer;
    IpHeader *m_pIpHeader;
    UdpHeader *m_pUdpHeader;
    PseudoUdpHeader *m_pPseudoHeader;

    const char* m_pSourceIp;
    const char* m_pDestinationIp;
    uint16_t m_nSourcePort;
    uint16_t m_nDestinationPort;

    bool m_bInited;
    struct sockaddr_in m_DestinationAddr;
    size_t m_nDestionationAddrLen;
    size_t m_nBufferLen;
    size_t m_nIpHeaderLen;
    size_t m_nUdpHeaderLen;
    size_t m_nPseudoHeaderLen;
    uint32_t m_uSourceIp;
    uint32_t m_uDestinationIp;

private:

    uint16_t CalcChecksum(void *data, size_t len)
    {
        uint16_t *p = (uint16_t *) data;
        size_t left = len;
        uint32_t sum = 0;
        while ( left > 1 )
        {
            sum += *p++;
            left -= sizeof (uint16_t);
        }
        if (left == 1)
        {
            sum += *(uint8_t *) p;
        }
        sum = (sum >> 16) + (sum & 0xFFFF);
        sum += (sum >> 16);
        return ~sum;
    }
};
