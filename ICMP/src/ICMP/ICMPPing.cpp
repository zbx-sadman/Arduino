// https://wizwiki.net/wiki/lib/exe/fetch.php/products:w5500:w5500_ap_ipraw_v110e.pdf
// However, be aware that the Hardwired Ping Reply Logic is disabled if ICMP is opened as SOCKET n in IPRAW mode,
// The lifecycle of SOCKET in IPRAW mode is composed OPEN, SEND, RECEIVE, and CLOSE. 

#include "ICMPPing.h"


ICMPPing::ICMPPing(const SOCKET _socketNo)
: socketNo(_socketNo), packetSeqNo(0x00)
{}

ICMPPing::~ICMPPing() {
 resourceFree();
}

void ICMPPing::resourceFree() {
  if (icmpPayload) { 
     delete[] icmpPayload; 
     icmpPayload = nullptr;
  }

  if (icmpInstance) { 
     delete icmpInstance; 
     icmpInstance = nullptr;
  }
}

icmpPingStatus_t ICMPPing::start(const IPAddress& _destinationIpAddress, const uint16_t _payloadSize, const uint16_t _systemId, const uint8_t _ttl, const uint32_t _timeout) {
  icmpStatus_t icmpStatus;
  icmpPingCurrentStatus = STATUS_NO_MEMORY_ENOUGH;

  icmpPayloadSize      = _payloadSize;
  systemId             = _systemId;
  pingTimeout          = _timeout;
  destinationIpAddress = (uint32_t)_destinationIpAddress;
  ICMPReply.type = ICMPReply.code = ICMPReply.payloadSize = ICMPReply.sourceIp = ICMPReply.time = 0x00;
                                          
  // Resources must be freed on restart to avoid ICMP instance lifecycle broke and/or if _payloadSize was changed
  resourceFree();

  // Prepare icmpPayload
  icmpPayload = new uint8_t[icmpPayloadSize];
  if (nullptr == icmpPayload) { goto finish; }
  // Fill payload by first byte of systemId
  memset(icmpPayload, (uint8_t) systemId, icmpPayloadSize);

  // Prepare ICMP instance
  icmpInstance = new ICMP(socketNo, systemId, icmpPayload, icmpPayloadSize);
  if (nullptr == icmpInstance) { goto finish; }

  pingStartTime = millis();
  icmpPingCurrentStatus = STATUS_SEND_TIMEOUT; 
  icmpStatus = icmpInstance->sendPacket(ICMP::TYPE_ECHO_PING, _destinationIpAddress, packetSeqNo, _ttl);
  //Serial.print(F("** 1) icmpStatus: ")); Serial.println(icmpStatus);
  if (ICMP::STATUS_SOCKET_ERROR == icmpStatus) { socketNo = STATUS_SOCKET_ERROR; }
  // STATUS_SUCCESS or STATUS_SEND_TIMEOUT
  if (ICMP::STATUS_SUCCESS != icmpStatus) { goto finish; }
  //Serial.print(F("** 2) icmpStatus: ")); Serial.println(icmpStatus);
  icmpStatus = icmpInstance->receivingStart();
  //Serial.print(F("** 3) icmpStatus: ")); Serial.println(icmpStatus);
  if (ICMP::STATUS_RECIEVE_PROCESSING != icmpStatus) { goto finish; } 
  icmpPingCurrentStatus = STATUS_PROCESSEED;
  //Serial.print(F("** 4) icmpStatus: ")); Serial.println(icmpStatus);

finish:
  return icmpPingCurrentStatus;
}                          

icmpPingStatus_t ICMPPing::status() {

  if (STATUS_PROCESSEED != icmpPingCurrentStatus) { goto finish; }

  ICMPReply.time = millis() - pingStartTime;

  if (ICMPReply.time >= pingTimeout) { icmpPingCurrentStatus = STATUS_RECIEVE_TIMEOUT; }
  if (ICMP::STATUS_SUCCESS == icmpInstance->receivingStatus()) {
    IPPacket_t inPacket = icmpInstance->incomingPacket();
    ICMPReply.type = inPacket.icmp.type;
    ICMPReply.code = inPacket.icmp.code;
    ICMPReply.sourceIp = inPacket.info.sourceIp;
    ICMPReply.payloadSize = inPacket.info.icmpPayloadSize;
    icmpPingCurrentStatus = STATUS_SUCCESS;
    // Reply is not recieved if it's contain wrong data.
    //  ...or may be it is "bad response" state?
    switch (ICMPReply.type) {
       case ICMP::TYPE_ECHO_REPLY: {
         if (destinationIpAddress != ICMPReply.sourceIp || systemId != inPacket.icmp.id || packetSeqNo != inPacket.icmp.seq) { 
            icmpPingCurrentStatus = STATUS_RECIEVE_TIMEOUT; 
         } else if (icmpPayloadSize != ICMPReply.payloadSize) {
            icmpPingCurrentStatus = STATUS_BAD_RESPONSE; 
         }
         break;
       } // case ICMP::TYPE_ECHO_REPLY

       case ICMP::TYPE_TIME_EXCEEDED: {
         icmpPingCurrentStatus = STATUS_HOP_REACHED; 
         break;
       } // case ICMP::TYPE_TIME_EXCEEDED
    } // switch (ICMPReply.type)   
  } // if (ICMP::STATUS_SUCCESS == icmpInstance->receivingStatus())
  // Resources must be freed if ping not processed anymore
  if (STATUS_PROCESSEED != icmpPingCurrentStatus) {
   resourceFree();  
   packetSeqNo++;
  }

finish:
  return icmpPingCurrentStatus;
}

icmpPingStatus_t ICMPPing::ping(const IPAddress& _destinationIpAddress, const uint16_t _payloadSize, const uint16_t _systemId, const uint8_t _ttl, const uint32_t _timeout) {
  icmpPingStatus_t icmpPingStatus = start(_destinationIpAddress, _payloadSize, _systemId, _ttl, _timeout);
  // Serial.print(F("1) icmpPingStatus: ")); Serial.println(icmpPingStatus);
  while (STATUS_PROCESSEED == (icmpPingStatus = ICMPPing::status())) { 
  // Serial.print(F("2) icmpPingStatus: ")); Serial.println(icmpPingStatus);
  };
  // Serial.print(F("3) icmpPingStatus: ")); Serial.println(icmpPingStatus);
  return icmpPingStatus;
} 
