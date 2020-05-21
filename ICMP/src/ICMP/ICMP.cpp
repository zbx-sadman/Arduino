// https://wizwiki.net/wiki/lib/exe/fetch.php/products:w5500:w5500_ap_ipraw_v110e.pdf
// However, be aware that the Hardwired Ping Reply Logic is disabled if ICMP is opened as SOCKET n in IPRAW mode,
// The lifecycle of SOCKET in IPRAW mode is composed OPEN, SEND, RECEIVE, and CLOSE. 

#include "ICMP.h"
#include "../Ethernet/socket.h"

#define ICMP_DEBUG 0

const uint32_t socketPrepareTimeout = 10;

// this is equal two functions (mus be aliased)
static inline uint16_t __htons(uint16_t _number) { return ((_number << 0x08) & 0xFF00) | ((_number >> 0x08) & 0x00FF); }
#define __ntohs __htons

ICMP::ICMP(const SOCKET _socketNo, const uint16_t _systemId, uint8_t* _payloadBuffer, const uint16_t _payloadBufferSize)
: systemId(_systemId), payloadBufferSize(_payloadBufferSize)
{
   memset((uint8_t*)&packet, 0x00, sizeof(packet));
   packet.icmpPayload = _payloadBuffer;  // this is pointer to external payload buffer!
   packet.info.icmpPayloadSize = _payloadBufferSize;
   // Try to open socket in IPRAW mode
   socketBegin(_socketNo);
}

ICMP::~ICMP() {
   socketClose(socketNo);
} 

uint8_t ICMP::socketBegin(const SOCKET _socketNo) {

    uint8_t rc = false;

#if (ICMP_DEBUG > 1)
    Serial.println(F("Socket begin... "));
#endif
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    // All socket activity will be canceled
    W5100.execCmdSn(_socketNo, Sock_CLOSE);
    W5100.writeSnIR(_socketNo, 0xFF);

    W5100.writeSnPROTO(_socketNo, IPPROTO::ICMP);
    W5100.writeSnMR(_socketNo, SnMR::IPRAW);
    W5100.writeSnPORT(_socketNo, 0x00);
    W5100.execCmdSn(_socketNo, Sock_OPEN);
    uint32_t start = millis();
    while (!rc && (millis() - start < socketPrepareTimeout)) {
      rc = (SnSR::IPRAW == W5100.readSnSR(_socketNo));  
      yield();
    }
    SPI.endTransaction();
    
    if (rc) {
      // All OK, socket can be used
      socketNo = _socketNo;
    } else {
      // Socket does not inited properly within the set time
      socketClose(_socketNo);  
    }
    return rc;
}

void ICMP::socketClose(const SOCKET _socketNo) {

#if (ICMP_DEBUG > 1)
    Serial.println(F("Socket close"));
#endif
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.execCmdSn(_socketNo, Sock_CLOSE);
    W5100.writeSnIR(_socketNo, 0xFF);
    socketNo = WRONG_SOCKET_NO;
    SPI.endTransaction();
}

void ICMP::initChecksum() {
  checksum = 0x00;
}

void ICMP::addChecksum(const uint8_t* _buffer, const uint16_t _bufferSize) {
  for (uint16_t i = 0x00; _bufferSize > i; i += 0x02) {
      checksum += makeUint16(_buffer[i], _buffer[i + 0x01]);
  }
}

void ICMP::addChecksum(const uint8_t& _highByte, const uint8_t& _lowByte) {
      checksum += makeUint16(_highByte, _lowByte);
}

uint16_t ICMP::endChecksum() {
    checksum  = (checksum >> 0x10) + (checksum & 0xFFFF);
    checksum += (checksum >> 0x10);
    return ~checksum;
}
/*
original wiznet code
uint16_t checksum(uint8_t * data_buf, uint16_t len)

{
  uint16_t sum, tsum, i, j;
  uint32_t lsum;

  j = len >> 1;
  lsum = 0;
  tsum = 0;
  for (i = 0; i < j; i++)
    {
      tsum = data_buf[i * 2];
      tsum = tsum << 8;
      tsum += data_buf[i * 2 + 1];
      lsum += tsum;
    }
   if (len % 2)
    {
      tsum = data_buf[i * 2];
      lsum += (tsum << 8);
    }
    sum = (uint16_t)lsum;
    sum = ~(sum + (lsum >> 16));
  return sum;

}

*/
icmpStatus_t ICMP::sendPacket(const uint8_t _packetType, const IPAddress& _destinationIpAddress, const uint16_t _packetSeqNo, const uint8_t _ttl) {
    if (WRONG_SOCKET_NO == socketNo) { return STATUS_SOCKET_ERROR; } 
    
    icmpStatus_t rc = STATUS_NONE;

    packet.icmp.type     = _packetType;
    packet.icmp.id       = __htons(systemId);
    packet.icmp.seq      = __htons(_packetSeqNo);

    initChecksum();
    addChecksum((uint8_t*)&packet.icmp, sizeof(packet.icmp));
    addChecksum(packet.icmpPayload, payloadBufferSize);
    packet.icmp.checksum = endChecksum();
    packet.icmp.checksum = __htons(packet.icmp.checksum);

#if (ICMP_DEBUG > 1)
    Serial.println(F("\nSending packet"));
#endif

    uint8_t addri [] = {_destinationIpAddress[0x00], _destinationIpAddress[0x01], _destinationIpAddress[0x02], _destinationIpAddress[0x03]};

#if (ICMP_DEBUG > 2)
    Serial.print(F("Send packet prefix: "));
    for (uint16_t i = 0x00; sizeof(packet.icmp) > i; i++) {
        Serial.print(F(" 0x")); Serial.print(((uint8_t*)&packet.icmp)[i], HEX);
    }
    Serial.println();
#endif

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.writeSnDIPR(socketNo, addri);
    W5100.writeSnTTL(socketNo, _ttl);
    // The port isn't used, becuause ICMP is a network-layer protocol. So we
    // write zero. This probably isn't actually necessary.
    W5100.writeSnDPORT(socketNo, 0x00);
    // Write to socket packet header first
    write_data(socketNo, 0x00, (uint8_t*)&packet.icmp, sizeof(packet.icmp));
    // Add external payload 
    write_data(socketNo, sizeof(packet.icmp), packet.icmpPayload, payloadBufferSize);
    // Send data
    W5100.execCmdSn(socketNo, Sock_SEND);
    SPI.endTransaction();

    // No system timeout used
    // SnIR is always have SEND_OK bit if sending will be OK once in current session
    // Socket must be re-opened to proper diagnostic

    while(STATUS_NONE == rc) {
      SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
      uint8_t valueSnIR = W5100.readSnIR(socketNo);
      SPI.endTransaction();
      if (SnIR::TIMEOUT == (valueSnIR & SnIR::TIMEOUT)) { rc = STATUS_SEND_TIMEOUT; }
      if (SnIR::SEND_OK == (valueSnIR & SnIR::SEND_OK)) { rc = STATUS_SUCCESS; }
      yield();
    }

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.writeSnIR(socketNo, (SnIR::SEND_OK | SnIR::TIMEOUT));
    SPI.endTransaction();
    return rc;
}

icmpStatus_t ICMP::receivePacketProcessing() {

  if (WRONG_SOCKET_NO == socketNo) { return STATUS_SOCKET_ERROR; }

  icmpStatus_t rc = STATUS_RECIEVE_PROCESSING;

  SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
  uint16_t bytesAvailable = getSnRX_RSR(socketNo);
  SPI.endTransaction();
   
  switch (processingStage) {
    case psHandleIpPacketInfo: {
      // Not enough data 
      if (bytesAvailable < sizeof(packet.info)) { yield(); break; }
      // Fetch IP packet info data
      SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
      read_data(socketNo, recieveBufferAddr, (uint8_t*)&packet.info, sizeof(packet.info));
      SPI.endTransaction();
      // Next reading begins from 0x00 + size of IP packet info 
      recieveBufferAddr += sizeof(packet.info);

      packet.info.icmpPayloadSize = __ntohs(packet.info.icmpPayloadSize);

#if (ICMP_DEBUG > 2)
      Serial.print(F("Received IP packet header: "));
      for (uint8_t i = 0x00; sizeof(packet.info) > i; i++) {
          Serial.print(" 0x"); Serial.print(((uint8_t*)&packet.info)[i], HEX);
      }
      Serial.println();
#endif
      processingStage = psHandleIcmpData; 
      break;
    } // case rpsHandleIpPacketInfo 

    case psHandleIcmpData: {
      // Need to have in the buffer packet info size + payload size from info, because Sn_RX_RD not corrected after packet info reading
      uint16_t needWaiForBytesNum = sizeof(packet.info) + packet.info.icmpPayloadSize;
      if (bytesAvailable < needWaiForBytesNum) { yield(); break; }

      // Fetch ICMP prefix data
      SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
      read_data(socketNo, recieveBufferAddr, (uint8_t*)&packet.icmp, sizeof(packet.icmp));
      SPI.endTransaction();
      recieveBufferAddr += sizeof(packet.icmp);

#if (ICMP_DEBUG > 2)
      Serial.print(F("Received ICMP packet header: "));
      for (uint8_t i = 0x00; sizeof(packet.icmp) > i; i++) {
          Serial.print(" 0x"); Serial.print(((uint8_t*)&packet.icmp)[i], HEX);
      }
      Serial.println();
#endif
      uint16_t recievedChecksum = __ntohs(packet.icmp.checksum);
      packet.icmp.checksum = 0x00;
      initChecksum();
      addChecksum((uint8_t*)&packet.icmp, sizeof(packet.icmp));

      // icmp payload must not include icmp prefix (header, id, etc.) 
      packet.info.icmpPayloadSize -= sizeof(packet.icmp);

      // Wait for all payload incoming, but fetch for external payload size only, and just flush other data 
      uint16_t fetchPayloadSize = (packet.info.icmpPayloadSize > payloadBufferSize) ? payloadBufferSize : packet.info.icmpPayloadSize;

      // Fetch ICMP payload data
      SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
      read_data(socketNo, recieveBufferAddr, packet.icmpPayload, fetchPayloadSize);
      //SPI.endTransaction();
      recieveBufferAddr += fetchPayloadSize;

      // calc checksum for fetched payload
      addChecksum(packet.icmpPayload, payloadBufferSize);

      // When incoming payload very big - we do not recieve it, and just calc CRC 
      if (packet.info.icmpPayloadSize > payloadBufferSize) {
         //SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
         uint16_t restPayloadSize = packet.info.icmpPayloadSize - payloadBufferSize;
         for (uint16_t i = 0x00; restPayloadSize > i; i += 0x02) {
             uint8_t fetchData[0x02] = {0x00, 0x00};
             read_data(socketNo, recieveBufferAddr, fetchData, sizeof(fetchData));
             addChecksum(fetchData[0x00], fetchData[0x01]);     
             recieveBufferAddr += sizeof(fetchData);
         }
         //SPI.endTransaction();
      }

      //SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
      // Mark all data as readed
      W5100.writeSnRX_RD(socketNo, bytesAvailable);
      W5100.execCmdSn(socketNo, Sock_RECV);
      // It is set as ‘1’ whenever W5100 receives data. And it is also set as ‘1’ if received data remains after execute CMD_RECV command. 
      W5100.writeSnIR(socketNo, SnIR::RECV);
      packet.ttl = W5100.readSnTTL(socketNo);
      SPI.endTransaction();

      packet.icmp.checksum = endChecksum();
      packet.icmp.id       = __htons(packet.icmp.id);
      packet.icmp.seq      = __htons(packet.icmp.seq);
 
      // Verify checksum
      processingStage = (packet.icmp.checksum == recievedChecksum) ? psDone : psErrorBadCrc;
      break;
    } // case rpHandleIcmpPayload
    case psDone:        { rc = STATUS_SUCCESS; break; } 
    case psErrorBadCrc: { rc = STATUS_BAD_CHECKSUM; break; } 
    default: { break; }
  } // switch
 return rc; 
}

icmpStatus_t ICMP::receivingStart() {
  if (WRONG_SOCKET_NO == socketNo) { return STATUS_SOCKET_ERROR; } 

  processingStage = psHandleIpPacketInfo; 
  memset(packet.icmpPayload, 0x00, payloadBufferSize);
  SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
  recieveBufferAddr = W5100.readSnRX_RD(socketNo);
  SPI.endTransaction();

#if (ICMP_DEBUG > 1)
  Serial.println(F("\nRecieving packet"));
#endif

  return STATUS_RECIEVE_PROCESSING;
}

icmpStatus_t ICMP::receivingStatus() {
  if (WRONG_SOCKET_NO == socketNo) { return STATUS_SOCKET_ERROR; }
  return receivePacketProcessing();
}; 

icmpStatus_t ICMP::receivePacket(const uint32_t _timeout) {
  if (WRONG_SOCKET_NO == socketNo) { return STATUS_SOCKET_ERROR; } 

  uint32_t start = millis();

  icmpStatus_t rc = receivingStart();
  while (STATUS_RECIEVE_PROCESSING == rc) {
     rc = receivePacketProcessing();
     if (millis() - start > _timeout) { rc = STATUS_RECIEVE_TIMEOUT; }
  }
  return rc;
}
