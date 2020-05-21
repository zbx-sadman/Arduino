#ifndef	_SOCKET_H_
#define	_SOCKET_H_

//#include "utility/w5500.h"

uint16_t getSnTX_FSR(uint8_t s);
uint16_t getSnRX_RSR(uint8_t s);
void write_data(uint8_t s, uint16_t offset, const uint8_t *data, uint16_t len);
void read_data(uint8_t s, uint16_t src, uint8_t *dst, uint16_t len);


#endif
/* _SOCKET_H_ */
