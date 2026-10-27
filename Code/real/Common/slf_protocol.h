#ifndef SLF_PROTOCOL_H
#define SLF_PROTOCOL_H

#include <stdint.h>

#define SLF_FRAME_LEN 12u
#define SLF_RECORD_SIZE 16u
#define SLF_LORA_CMD_FIFO_WRITE 0x8Du
#define SLF_STATUS_EMERGENCY 0x80u
#define SLF_STATUS_ACCEL_ERROR 0x01u

uint8_t slf_crc8(const uint8_t *data, uint8_t length);

#endif