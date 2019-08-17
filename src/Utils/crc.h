/*
 * CRC.h
 *
 *  Created on: 08 May 2018
 *      Author: janus
 */

#ifndef SRC_CABOODLE_CRC_H_
#define SRC_CABOODLE_CRC_H_
#include <stdint.h>

#define WIDTH 		16

typedef uint16_t crc;

class CRC_8
{
	static uint8_t crc_update(uint8_t data, uint8_t crc);

public:
	static uint8_t crc(uint8_t * data_ptr, uint32_t len);
};

class CRC_16
{
	crc crcTable[256];
	crc mInitial;

	void crcInit(void);

public:
	CRC_16(uint16_t polynomial, uint16_t initial);
	virtual ~CRC_16();

	static uint16_t calculateKSES(uint8_t *buff, int len);
	uint16_t calculate(uint8_t *buff, int len);
};


#endif /* SRC_CRC_H_ */
