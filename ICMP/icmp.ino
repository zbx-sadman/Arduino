#include "src/Ethernet/Ethernet.h"
#include "src/ICMP/ICMP.h"
#include "src/ICMP/ICMPPing.h"

const uint8_t ethShieldCSPin = 10; // CS pin for the Ethernet Shield
const uint32_t actionInterval = 1000;

const uint8_t socketNo = 0; // ICMP activity on socket 0

uint16_t systemId = 0xC0BA; // System ID for ICMP packets
uint8_t payloadSize = 64,   // ICMP packet payload is 64 byte
        ttl = 128;          // ICMP packet will be die after ... route hops

uint8_t mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};

IPAddress targetIp(8, 8, 8, 8);

inline uint32_t getRamFree(void) {
  extern uint16_t __heap_start, *__brkval;
  uint16_t v;
  return (uint32_t) &v - (__brkval == 0 ? (uint32_t) &__heap_start : (uint32_t) __brkval);
}

// Shared routine which make ICMPPing status human-readable
void printPingStatus(icmpPingStatus_t _pingStatus) {
  Serial.print(' ');
  switch (_pingStatus) {
    case ICMPPing::STATUS_SOCKET_ERROR:    Serial.print(F("socket error")); break;
    case ICMPPing::STATUS_SEND_TIMEOUT:    Serial.print(F("can't send ping")); break;
    case ICMPPing::STATUS_SUCCESS:         Serial.print(F("success")); break;
    case ICMPPing::STATUS_TIME_EXCEEDED:   Serial.print(F("time exceed")); break;
    case ICMPPing::STATUS_BAD_RESPONSE:    Serial.print(F("bad response recieved")); break;
    case ICMPPing::STATUS_NO_RESPONSE:     Serial.print(F("no response recieved")); break;
    default:                               Serial.print(F("unlisted status")); break;
  }
}

// Traceroute IP in the "blocking" style
void blockingIpTracert(IPAddress _targetIp) {
  char buffer[123], icon;
  uint8_t packetTtl = 0x00,
          done = false;
  icmpPingStatus_t pingStatus;
  ICMPPing icmpPing(socketNo);

  Serial.print(F("Host ")); Serial.println(_targetIp);
  Serial.println(F("Legend: 'V' - host reached, '>' hop reply recieved, '*' - reply timeout, X - socket error\n"));

  // Increase TTL of the ICMP packet from 0x01 by 0x01 and wait destination host "ICMP echo reply" answer
  while (!done) {
    packetTtl++;
    icmpPing.start(_targetIp, payloadSize, systemId, packetTtl);
    // Wait for finish of the reply packet recieving finish, or any processing error detect
    while (ICMPPing::STATUS_PROCESSEED == (pingStatus = icmpPing.status())) { };
    switch (pingStatus) {
      case ICMPPing::STATUS_SUCCESS:         icon = 'V'; done = true; break;
      case ICMPPing::STATUS_SOCKET_ERROR:    icon = 'X'; done = true; break;
      case ICMPPing::STATUS_HOP_REACHED:     icon = '>'; break;
      case ICMPPing::STATUS_RECIEVE_TIMEOUT: icon = '*'; break;
      default:                               icon = '-'; break;
    }
    // Take reply data and print it
    ICMPReply_t ICMPReply = icmpPing.reply();
    IPAddress sourceIp = IPAddress(ICMPReply.sourceIp);

    snprintf(buffer, sizeof(buffer), "%c [TTL= %2u | Type= %2u | Code= %2u | Size= %3u | Time= %4lu ] %u.%u.%u.%u",
             icon, packetTtl, ICMPReply.type, ICMPReply.code, ICMPReply.payloadSize, ICMPReply.time, sourceIp[0x00], sourceIp[0x01], sourceIp[0x02], sourceIp[0x03]);
    Serial.println(buffer);
  }
}

// Ping IP in the "blocking" style
void blockingIpPing(IPAddress _targetIp) {
  ICMPPing icmpPing(socketNo);
  Serial.print(F("Host ")); Serial.print(_targetIp); Serial.print(' ');
  // Just wait for ping() method finish with any reason and prints status + time of the Ping action
  icmpPingStatus_t pingStatus = icmpPing.ping(_targetIp, payloadSize, systemId, ttl);
  printPingStatus(pingStatus);
  Serial.print(F(", time (ms): ")); Serial.println(icmpPing.replyTime());
}

// Ping IP in the "async" style
void asyncIpPing(IPAddress _targetIp) {
  uint8_t pingStarted = false;
  uint32_t prevDotPrintTime = 0x00,
           dotPrintInterval = 1;
  icmpPingStatus_t pingStatus;

  ICMPPing icmpPing(socketNo);
  Serial.print(F("Host ")); Serial.print(_targetIp); Serial.print(' ');

  // Imitate main loop
  while (true) {
    uint32_t nowTime = millis();
    // Print dot every dotPrintInterval
    if (nowTime - prevDotPrintTime > dotPrintInterval) {
      prevDotPrintTime = nowTime;
      Serial.print('.');
    }

    // Starts Ping action if it's not started
    if (!pingStarted) {
      pingStatus = icmpPing.start(_targetIp);
      pingStarted = true;
    } else {
      // Wait for finish with any reason if action is started
      pingStatus = icmpPing.status();
      if (ICMPPing::STATUS_PROCESSEED == pingStatus) {
        continue;
      }
      // Print status and time on processing finish
      printPingStatus(pingStatus);
      Serial.print(F(", time (ms): ")); Serial.println(icmpPing.replyTime());
      //pingStarted = false;
      // Jump out from the loop
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("ICMP demo\n\n"));
  Ethernet.init(ethShieldCSPin);
  Serial.print(F("Obtaining address from DHCP... "));
  if (!Ethernet.begin(mac)) {
    Serial.println(F("error"));
    while (true);
  } else {
    Serial.println(F("ok"));
  }
  Serial.print(F("Network IP:\t")); Serial.println(Ethernet.localIP());
  Serial.print(F("Subnet:\t\t")); Serial.println(Ethernet.subnetMask());
  Serial.print(F("Gateway IP:\t")); Serial.println(Ethernet.gatewayIP());
  Serial.println(F("-----------------------------"));
}

void loop() {
  Serial.print(F("\nUptime (ms): ")); Serial.print(millis());
  Serial.print(F("\nFree RAM (b): ")); Serial.println(getRamFree());

  Serial.print(F("\nIP ping (blocking): "));
  blockingIpPing(targetIp);
  delay(actionInterval);

  Serial.print(F("\nIP ping (async): "));
  asyncIpPing(targetIp);
  delay(actionInterval);

  Serial.print(F("\nIP tracert (blocking): "));
  blockingIpTracert(targetIp);
  delay(actionInterval);
}
