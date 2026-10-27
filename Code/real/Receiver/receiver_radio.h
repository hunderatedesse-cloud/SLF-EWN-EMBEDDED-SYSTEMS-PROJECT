#ifndef RECEIVER_RADIO_H
#define RECEIVER_RADIO_H

#include <stdint.h>

/*
 * AI-Thinker Ra-02 (SX1278) receiver.
 * Continuous RX @ 433 MHz / SF7 / BW125 (must match the node).
 * DIO0/RxDone fills buffer; available() / copy_frame() deliver it.
 */
void receiver_radio_init(void);
uint8_t receiver_radio_available(void);
void receiver_radio_copy_frame(uint8_t *frame);
void receiver_radio_wait_idle(void);

#endif
