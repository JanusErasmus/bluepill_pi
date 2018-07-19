/*
 * interface_nrf24.cpp
 *
 *  Created on: 14 Jul 2018
 *      Author: janus
 */
#include <stdio.h>
#include <unistd.h>

#include "RPi/spi.h"
#include "Utils/utils.h"
#include "interface_nrf24.h"

InterfaceNRF24 *InterfaceNRF24::__instance = 0;

SPI mSPI;

uint8_t InterfaceNRF24::nrf_t(uint8_t *tx_data, uint8_t *rx_data, int len)
{
	try {

		mSPI.transfernb( (char *) tx_data, (char *) rx_data, len);


	} catch (int &error)
	{
		printf("Permission denied: %s\n", strerror(error));
	}

	return len;
}

void InterfaceNRF24::nrf_cs_l(void)
{
	mSPI.beginTransaction(SPISettings(RF24_SPI_SPEED, MSBFIRST, SPI_MODE0));
}

void InterfaceNRF24::nrf_cs_h(void)
{
	mSPI.endTransaction();
}

void InterfaceNRF24::nrf_ce_l(void)
{
	digitalWrite(RPI_V2_GPIO_P1_22,false);
}

void InterfaceNRF24::nrf_ce_h(void)
{
	digitalWrite(RPI_V2_GPIO_P1_22,true);
}

void InterfaceNRF24::init(uint8_t *net_address, int len)
{
	if(!__instance)
	{
		mSPI.begin(0);
		__instance = new InterfaceNRF24(net_address, len);
	}
}

void addrToString(uint8_t *addr, char *string, int len)
{
	for (int k = 0; k < len; ++k)
	{
		sprintf(string, "%02X", addr[k]);
		string += 2;

		if(k < (len - 1))
			sprintf(string++, ":");
	}
}

void printPipe(uint8_t pipe)
{
	char pipeStr[16];
	uint8_t pipeAddr[8];
	int len = nRF24_GetAddr(pipe, pipeAddr);
	addrToString(pipeAddr, pipeStr, len);

	if(pipe < nRF24_PIPETX)
		printf("PIPE%2d %s\n", pipe, pipeStr);
	else
		printf("PIPETX %s\n", pipeStr);
}

void printPipes()
{
	for (int k = 0; k < 7; ++k)
	{
		printPipe(k);
	}
}

InterfaceNRF24::InterfaceNRF24(uint8_t *net_addr, int len)
{
	mPacketsLost = 0;
	mNetAddressLen = len;
	memcpy(mNetAddress, net_addr, len);

	nrf_cb.nRF24_T = nrf_t;
	nrf_cb.nRF24_CSN_L = nrf_cs_l;
	nrf_cb.nRF24_CSN_H = nrf_cs_h;
	nrf_cb.nRF24_CE_L = nrf_ce_l;
	nrf_cb.nRF24_CE_H = nrf_ce_h;


	pinMode(RPI_V2_GPIO_P1_22 ,OUTPUT);
	pinMode(RPI_V2_GPIO_P1_26, INPUT);

	nRF24_Init(&nrf_cb);

	// Set RF channel
	nRF24_SetRFChannel(40);

	// Set data rate
	nRF24_SetDataRate(nRF24_DR_250kbps);

	// Set CRC scheme
	nRF24_SetCRCScheme(nRF24_CRC_2byte);

	// Set address width, its common for all pipes (RX and TX)
	nRF24_SetAddrWidth(len);

	// Configure RX PIPES
	nRF24_SetAddr(nRF24_PIPE0, net_addr);
	nRF24_SetRXPipe(nRF24_PIPE0	, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes
	net_addr[0] = 0x01;
	nRF24_SetAddr(nRF24_PIPE1, net_addr); // program address for pipe
	nRF24_SetRXPipe(nRF24_PIPE1	, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

	net_addr[0] = 0x02;
	nRF24_SetAddr(nRF24_PIPE2, net_addr); // program address for pipe
	nRF24_SetRXPipe(nRF24_PIPE2, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

	net_addr[0] = 0x03;
	nRF24_SetAddr(nRF24_PIPE3, net_addr); // program address for pipe
	nRF24_SetRXPipe(nRF24_PIPE3, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

	net_addr[0] = 0x04;
	nRF24_SetAddr(nRF24_PIPE4, net_addr); // program address for pipe
	nRF24_SetRXPipe(nRF24_PIPE4, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

	net_addr[0] = 0x05;
	nRF24_SetAddr(nRF24_PIPE5, net_addr); // program address for pipe
	nRF24_SetRXPipe(nRF24_PIPE5, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

	// Set TX power for Auto-ACK (maximum, to ensure that transmitter will hear ACK reply)
	nRF24_SetTXPower(nRF24_TXPWR_0dBm);

	// Configure auto retransmit: 10 retransmissions with pause of 2500s in between
	nRF24_SetAutoRetr(nRF24_ARD_4000us, 10);

	// Enable Auto-ACK for pipe#0 (for ACK packets)
	//    // Disable ShockBurst for all pipes
	//    nRF24_DisableAA(0xFF);

	// Set operational mode (PRX == receiver)
	nRF24_SetOperationalMode(nRF24_MODE_RX);

	// Clear any pending IRQ flags
	nRF24_ClearIRQFlags();

	printPipes();

	// Wake the transceiver
	nRF24_SetPowerMode(nRF24_PWR_UP);

	// Put the transceiver to the RX mode
	nrf_ce_h();
}

InterfaceNRF24::~InterfaceNRF24()
{
}

#define nRF24_WAIT_TIMEOUT         (uint32_t)20
// Function to transmit data packet
// input:
//   pBuf - pointer to the buffer with data to transmit
//   length - length of the data buffer in bytes
// return: one of nRF24_TX_xx values
nRF24_TXResult InterfaceNRF24::transmitPacket(uint8_t *pBuf, uint8_t length)
{
	volatile uint32_t wait = nRF24_WAIT_TIMEOUT;
	uint8_t status;

	// Deassert the CE pin (in case if it still high)
	nrf_ce_l();

	// Transfer a data from the specified buffer to the TX FIFO
	nRF24_WritePayload(pBuf, length);
	// Start a transmission by asserting CE pin (must be held at least 10us)
	nrf_ce_h();
	usleep(100000);

	// Poll the transceiver status register until one of the following flags will be set:
	//   TX_DS  - means the packet has been transmitted
	//   MAX_RT - means the maximum number of TX retransmits happened
	// note: this solution is far from perfect, better to use IRQ instead of polling the status
	do {
		status = nRF24_GetStatus();
		if (status & (nRF24_FLAG_TX_DS | nRF24_FLAG_MAX_RT)) {
			break;
		}

		usleep(100);
	} while (wait--);

	if (!wait) {
		// Timeout
		return nRF24_TX_TIMEOUT;
	}

	// Clear pending IRQ flags
    nRF24_ClearIRQFlags();
    nRF24_GetStatus();

	// Check the flags in STATUS register
	printf(" - Status: %02X\n", status);

	if (status & nRF24_FLAG_MAX_RT) {
		// Auto retransmit counter exceeds the programmed maximum limit (FIFO is not removed)
		return nRF24_TX_MAXRT;
	}

	if (status & nRF24_FLAG_TX_DS) {
		// Successful transmission
		return nRF24_TX_SUCCESS;
	}

	// Some banana happens, a payload remains in the TX FIFO, flush it
	nRF24_FlushTX();

	return nRF24_TX_ERROR;
}


int InterfaceNRF24::transmit(uint8_t *addr, uint8_t *payload, uint8_t length)
{
	int  tx_length = 0;

    // Configure TX PIPE
    nRF24_SetAddr(nRF24_PIPETX, addr); // program TX address
	nRF24_SetAddr(nRF24_PIPE0, addr); // program address for pipe to receive ACK

    printf("ADDR: \n");
    diag_dump_buf(addr, 5);

	// Print a payload
	printf("TX  : %d\n", (int)length);
	diag_dump_buf(payload, length);

	// Transmit a packet
	nRF24_SetOperationalMode(nRF24_MODE_TX);

	nRF24_TXResult tx_res = transmitPacket(payload, length);
	uint8_t otx = nRF24_GetRetransmitCounters();
	uint8_t otx_plos_cnt = (otx & nRF24_MASK_PLOS_CNT) >> 4; // packets lost counter
	uint8_t otx_arc_cnt  = (otx & nRF24_MASK_ARC_CNT); // auto retransmissions counter
	switch (tx_res) {
	case nRF24_TX_SUCCESS:
		printf(GREEN(" - OK\n"));
		tx_length = length;
		break;
	case nRF24_TX_TIMEOUT:
		printf(RED("TIMEOUT\n"));
		mPacketsLost++;
		tx_length = -1;
		break;
	case nRF24_TX_MAXRT:
		printf(CYAN("MAX RETRANSMIT\n"));
		mPacketsLost += otx_plos_cnt;
		nRF24_ResetPLOS();
		tx_length = -2;
		break;
	default:
		printf(RED("ERROR\n"));
		tx_length = -3;
		break;
	}
	printf(" - ARC= %d LOST= %d\n", (int)otx_arc_cnt, (int)mPacketsLost);

	// Set operational mode (PRX == receiver)
	nRF24_SetOperationalMode(nRF24_MODE_RX);

	nRF24_SetAddr(nRF24_PIPE0, mNetAddress); // reset address to receive data on PIPE0

	return tx_length;
}

void InterfaceNRF24::run()
{
	if(digitalRead(RPI_V2_GPIO_P1_26))
		return;

    uint8_t payload_length = 10;
	uint8_t nRF24_payload[32];
	while (nRF24_GetStatus_RXFIFO() != nRF24_STATUS_RXFIFO_EMPTY) {
		// Get a payload from the transceiver
		nRF24_RXResult pipe = nRF24_ReadPayload(nRF24_payload, &payload_length);

		// Clear all pending IRQ flags
		nRF24_ClearIRQFlags();

		if(receivedCB)
			receivedCB(pipe, nRF24_payload, payload_length);

		// Print a payload contents
		//printf("RCV PIPE# %d\n", (int)pipe);
		//printf(" PAYLOAD: %d\n", payload_length);
		//diag_dump_buf(nRF24_payload, payload_length);
	}
}

