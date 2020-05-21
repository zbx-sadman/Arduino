#pragma once
#include "ICMP.h"

#define ICMPPING_DEFAULT_TTL           (0x80)
#define ICMPPING_DEFAULT_SYSTEM_ID     (0x41)
#define ICMPPING_DEFAULT_PAYLOAD_SIZE  (0x20)
#define ICMPPING_DEFAULT_TIMEOUT       (1000UL)

typedef uint8_t icmpPingStatus_t;

#pragma pack(push,1)
    typedef struct {
        uint8_t  type;
        uint8_t  code;
        uint16_t payloadSize;
        uint32_t sourceIp;
        uint32_t time;
    } ICMPReply_t;
#pragma pack(pop)


class ICMPPing {
private:

    SOCKET socketNo;
    uint8_t* icmpPayload = nullptr;
    uint16_t icmpPayloadSize;
    uint16_t packetSeqNo;
    uint16_t systemId;
    uint32_t pingTimeout;
    uint32_t pingStartTime;
    uint32_t destinationIpAddress;
    ICMP*    icmpInstance = nullptr;
    icmpPingStatus_t icmpPingCurrentStatus;
    ICMPReply_t ICMPReply;
    void resourceFree();
  
public:
    ICMPPing(const SOCKET);
    ~ICMPPing();

    // Starts Ping process 
    icmpPingStatus_t start(const IPAddress&, const uint16_t = ICMPPING_DEFAULT_PAYLOAD_SIZE, const uint16_t = ICMPPING_DEFAULT_SYSTEM_ID, const uint8_t = ICMPPING_DEFAULT_TTL, const uint32_t = ICMPPING_DEFAULT_TIMEOUT);
    // Returns status of Ping process
    icmpPingStatus_t status();
    // Return process duration. It is represent ping reply time when status() returns STATUS_SUCCESS.
    inline uint32_t  replyTime() { return ICMPReply.time; }
    // Give an external process access to the ICMP reply data
    inline ICMPReply_t  reply() { return ICMPReply; }
     
    // Blocking Ping host by IP 
    icmpPingStatus_t ping(const IPAddress&, const uint16_t = ICMPPING_DEFAULT_PAYLOAD_SIZE, const uint16_t = ICMPPING_DEFAULT_SYSTEM_ID, const uint8_t = ICMPPING_DEFAULT_TTL, const uint32_t = ICMPPING_DEFAULT_TIMEOUT);

    // ICMPPing processing status codes
    static const uint8_t STATUS_NONE             = 0x00; // Just nope
    static const uint8_t STATUS_SUCCESS          = 0x01; // Ping is finished with success
    static const uint8_t STATUS_SEND_TIMEOUT     = 0x02; // Request can't be send
    static const uint8_t STATUS_RECIEVE_TIMEOUT  = 0x03; // Reply not recieved. The same state as STATUS_NO_RESPONSE
    static const uint8_t STATUS_NO_RESPONSE      = 0x03; // -"-"-"-. The same state as STATUS_RECIEVE_TIMEOUT
    static const uint8_t STATUS_SOCKET_ERROR     = 0x04; // General socket error. It can't be used (busy, or wrong number, or other problem detected).
    static const uint8_t STATUS_NO_MEMORY_ENOUGH = 0x05; // No free memory for ICMP payload buffer
    static const uint8_t STATUS_BAD_RESPONSE     = 0x06; // Remote answer is wrong - ICMP header is OK, but payload size is not equal
    static const uint8_t STATUS_PROCESSEED       = 0x07; // Ping in progress
    static const uint8_t STATUS_HOP_REACHED      = 0x08; // Remote answer with ICMP TYPE_TIME_EXCEEDED packet. It is traceroute'd hops answer. The same state as STATUS_TIME_EXCEEDED
    static const uint8_t STATUS_TIME_EXCEEDED    = 0x08; // -"-"-"-. The same state as STATUS_HOP_REACHED
};
