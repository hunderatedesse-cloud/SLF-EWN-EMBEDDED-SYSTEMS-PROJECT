#include "slf_protocol.h"

uint8_t slf_crc8(const uint8_t *data, uint8_t length) {
    uint8_t crc = 0x00;

    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u)
                                 : (uint8_t)(crc << 1);
        }
    }

    return crc;
}