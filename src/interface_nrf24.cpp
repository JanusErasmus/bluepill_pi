/*
 * interface_nrf24.cpp
 *
 *  Created on: 14 Jul 2018
 *      Author: janus
 */
#include <stdio.h>
#include <unistd.h>

#include "RPi/spi.h"
#include "nRF24L01.h"
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
	digitalWrite(RPI_V2_GPIO_P1_15,false);
}

void InterfaceNRF24::nrf_ce_h(void)
{
	digitalWrite(RPI_V2_GPIO_P1_15,true);
}

void InterfaceNRF24::init()
{
	if(!__instance)
	{
		mSPI.begin(0);
		__instance = new InterfaceNRF24();
	}
}

InterfaceNRF24::InterfaceNRF24()
{
	mPacketsLost = 0;

	nrf_cb.nRF24_T = nrf_t;
	nrf_cb.nRF24_CSN_L = nrf_cs_l;
	nrf_cb.nRF24_CSN_H = nrf_cs_h;
	nrf_cb.nRF24_CE_L = nrf_ce_l;
	nrf_cb.nRF24_CE_H = nrf_ce_h;


	pinMode(RPI_V2_GPIO_P1_15 ,OUTPUT);

	nRF24_Init(&nrf_cb);

	// Set RF channel
	nRF24_SetRFChannel(76);

	// Set data rate
	nRF24_SetDataRate(nRF24_DR_250kbps);

	// Set CRC scheme
	nRF24_SetCRCScheme(nRF24_CRC_2byte);

	// Set address width, its common for all pipes (RX and TX)
	nRF24_SetAddrWidth(5);

//    // Configure RX PIPES
    uint8_t nRF24_ADDR[] =  {0x00, 0xfc, 0xe1, 0xa8, 0xa8 };//{ 0xa8, 0xa8, 0xe1, 0xfc, 0x00 };
    nRF24_SetAddr(nRF24_PIPETX, nRF24_ADDR);
    nRF24_SetAddr(nRF24_PIPE0, nRF24_ADDR);
	nRF24_SetRXPipe(nRF24_PIPE0	, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes
//    nRF24_ADDR[0] = 0x11;
//	nRF24_SetAddr(nRF24_PIPE1, nRF24_ADDR); // program address for pipe
//	nRF24_SetRXPipe(nRF24_PIPE1	, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

//	nRF24_ADDR[0] = 0x22;
//	nRF24_SetAddr(nRF24_PIPE2, nRF24_ADDR); // program address for pipe
//	nRF24_SetRXPipe(nRF24_PIPE2, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

//	nRF24_ADDR[0] = 0x33;
//	nRF24_SetAddr(nRF24_PIPE3, nRF24_ADDR); // program address for pipe
//	nRF24_SetRXPipe(nRF24_PIPE3, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

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

	uint8_t pipeAddr[8];
	int len = nRF24_GetAddr(nRF24_PIPETX, pipeAddr);
	printf("PIPETX\n");
	diag_dump_buf(pipeAddr, len);
	len = nRF24_GetAddr(nRF24_PIPE0, pipeAddr);
	printf("PIPE0\n");
	diag_dump_buf(pipeAddr, len);
	len = nRF24_GetAddr(nRF24_PIPE1, pipeAddr);
	printf("PIPE1\n");
	diag_dump_buf(pipeAddr, len);
	len = nRF24_GetAddr(nRF24_PIPE2, pipeAddr);
	printf("PIPE2\n");
	diag_dump_buf(pipeAddr, len);
	len = nRF24_GetAddr(nRF24_PIPE3, pipeAddr);
	printf("PIPE3\n");
	diag_dump_buf(pipeAddr, len);
	len = nRF24_GetAddr(nRF24_PIPE4, pipeAddr);
	printf("PIPE4\n");
	diag_dump_buf(pipeAddr, len);

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

//	nrf_ce_l();

    // Configure TX PIPE
    nRF24_SetAddr(nRF24_PIPETX, addr); // program TX address
	nRF24_SetAddr(nRF24_PIPE0, addr); // program address for pipe to receive ACK
//	nRF24_SetRXPipe(nRF24_PIPE0	, nRF24_AA_ON, 10); // Auto-ACK: enabled, payload length: 10 bytes

//	nrf_ce_h();

    printf("ADDR: \n");
    diag_dump_buf(addr, 5);

	// Print a payload
	printf("TX  : %d\n", (int)length);
	diag_dump_buf(payload, length);

	// Transmit a packet
	nRF24_SetOperationalMode(nRF24_MODE_TX);
//	HAL_Delay(100);

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

//	addr[0] = 0x00;
//	nRF24_SetAddr(nRF24_PIPE0, addr); // reset address to receive data on PIPE0

	return tx_length;
}

void InterfaceNRF24::run()
{
//	if(HAL_GPIO_ReadPin(NRF_IRQ_GPIO_Port, NRF_IRQ_Pin))
//		return;
	usleep(100000);

    uint8_t payload_length = 10;
	uint8_t nRF24_payload[32];
	while (nRF24_GetStatus_RXFIFO() != nRF24_STATUS_RXFIFO_EMPTY) {
		// Get a payload from the transceiver
		nRF24_RXResult pipe = nRF24_ReadPayload(nRF24_payload, &payload_length);

		// Clear all pending IRQ flags
		nRF24_ClearIRQFlags();

		// Print a payload contents to UART
		printf("RCV PIPE# %d", (int)pipe);
		printf(" PAYLOAD:> %d", payload_length);
		diag_dump_buf(nRF24_payload, payload_length);
	}

	{
		printf("empty\n");
	}
}

