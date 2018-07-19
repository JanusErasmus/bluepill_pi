/*
 * interface_nrf24.h
 *
 *  Created on: 14 Jul 2018
 *      Author: janus
 */

#ifndef SRC_INTERFACE_NRF24_H_
#define SRC_INTERFACE_NRF24_H_

#include "nrf24.h"

class InterfaceNRF24
{
	static InterfaceNRF24 *__instance;
	nRF24cb nrf_cb;
	uint32_t mPacketsLost;
	int mNetAddressLen;
	uint8_t mNetAddress[5];

	bool (*receivedCB)(int pipe, uint8_t *data, int len);

	static uint8_t nrf_t(uint8_t *tx_data, uint8_t *rx_data, int len);
	static void nrf_cs_l(void);
	static void nrf_cs_h(void);
	static void nrf_ce_l(void);
	static void nrf_ce_h(void);
	nRF24_TXResult transmitPacket(uint8_t *pBuf, uint8_t length);

	InterfaceNRF24(uint8_t *net_address, int len);
	virtual ~InterfaceNRF24();

public:
	static void init(uint8_t *net_address, int len);
	static InterfaceNRF24 *get(){ return __instance; }

	void setRXcb(bool(cb(int pipe, uint8_t *data, int len))){ receivedCB = cb; }

	void run();
	int transmit(uint8_t *addr, uint8_t *payload, uint8_t length);
};

#endif /* SRC_INTERFACE_NRF24_H_ */
