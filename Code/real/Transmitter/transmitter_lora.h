#ifndef TRANSMITTER_LORA_H
#define TRANSMITTER_LORA_H

#include <stdint.h>

/*
 * AI-Thinker Ra-02 (SX1278) transmitter.
 * init() -> LoRa standby @ 433 MHz / SF7 / BW125.
 * send() -> FIFO + TX, returns 1 on TxDone.
 */
void transmitter_lora_init(void);
uint8_t transmitter_lora_send(const uint8_t *frame, uint8_t length);

#endif
