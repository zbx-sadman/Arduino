#pragma once
#include "../Ethernet/Ethernet.h"
#include "../Ethernet/w5100.h"

#define TIMEOUT_MS                          (1000UL)
#define WRONG_SOCKET_NO                     (0xFF)

typedef uint8_t icmpStatus_t;

#pragma pack(push,1)
    typedef struct {
        uint8_t      type;
        uint8_t      code;
        uint16_t     checksum;
        uint16_t     id;
        uint16_t     seq;
    } ICMPPrefix_t;

    typedef struct {
        uint32_t     sourceIp;
        uint16_t     icmpPayloadSize;
    } IPPacketInfo_t;

    typedef struct {
        IPPacketInfo_t  info;
        ICMPPrefix_t    icmp;
        uint8_t*        icmpPayload;
        uint16_t        ttl;
    } IPPacket_t;
#pragma pack(pop)

class ICMP {
  private:

#pragma pack(push,1)
    typedef enum : uint8_t {psHandleIpPacketInfo, psHandleIcmpData, psDone, psErrorBadCrc} processingStage_t;
#pragma pack(pop)

    SOCKET            socketNo = WRONG_SOCKET_NO;
    uint16_t          systemId;
    uint16_t          payloadBufferSize;
    uint16_t          recieveBufferAddr;
    uint32_t          checksum;

    IPPacket_t        packet;
    processingStage_t processingStage;

    // ICMPRAW proto lifecycle control
    uint8_t socketBegin(const SOCKET);
    void    socketClose(const SOCKET);
   
    // On-fly checksum calculate routines
    void initChecksum();
    void addChecksum(const uint8_t*, const uint16_t);
    void addChecksum(const uint8_t&, const uint8_t&);
    uint16_t endChecksum();

    //     
    icmpStatus_t receivePacketProcessing(); 
    //
    inline uint16_t makeUint16(const uint8_t& _highByte, const uint8_t& _lowByte) { return ((uint16_t)_highByte << 8) | _lowByte; } 

  public:
    ICMP(const SOCKET, const uint16_t, uint8_t*, const uint16_t);
    ~ICMP();

    // Send ICMP packet
    icmpStatus_t sendPacket(const uint8_t _packetType, const IPAddress& _destinationIpAddress, const uint16_t _packetSeqNo, const uint8_t _ttl);

    // Starts async reading the ICMP packet
    icmpStatus_t receivingStart(); // 
    // Returns status of reading processing the ICMP packet 
    icmpStatus_t receivingStatus(); // async finish
    // Blocking read the ICMP packet from opened socket: async start + wait for finish or timeout
    icmpStatus_t receivePacket(const uint32_t _timeout = TIMEOUT_MS); 
    // Give an external process access to the ICMP packet content
    inline IPPacket_t incomingPacket()     { return packet; }

    // ICMP packet types
    static const uint8_t TYPE_ECHO_REPLY            = 0x00;  // Type 0
    static const uint8_t TYPE_ECHO_PING             = 0x08;  // Type 8
    static const uint8_t TYPE_TIME_EXCEEDED         = 0x0B;  // Type 13

    // ICMP processing status codes
    static const uint8_t STATUS_NONE                = 0x00;
    static const uint8_t STATUS_SUCCESS             = 0x01;
    static const uint8_t STATUS_SEND_TIMEOUT        = 0x02; // Timed out sending the request
    static const uint8_t STATUS_RECIEVE_PROCESSING  = 0x03; //
    static const uint8_t STATUS_RECIEVE_TIMEOUT     = 0x04; // Died waiting for a response
    static const uint8_t STATUS_SOCKET_ERROR        = 0x05; // Socket opening error
    static const uint8_t STATUS_BAD_CHECKSUM        = 0x06; // Recieved packet have wrong checksum

};

