/* Production receiver firmware for the Qt dashboard. */
#define F_CPU 16000000UL

#include "receiver_radio.h"
#include "receiver_storage.h"
#include "../Common/slf_protocol.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define MAGIC 0xAAu
#define CMD_STATUS 0x53u
#define CMD_FULL 0x44u
#define CMD_NEW 0x4Eu
#define CMD_CLEAR 0x43u
#define CMD_ERROR 0xFFu

static void uart_init(uint16_t ubrr) {
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_tx(uint8_t byte) {
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = byte;
}

static void uart_write(const uint8_t *data, uint16_t length) {
    for (uint16_t i = 0; i < length; i++)
        uart_tx(data[i]);
}

static uint8_t uart_available(void) { return (UCSR0A & (1 << RXC0)) != 0; }

static uint8_t uart_rx(void) {
    while (!(UCSR0A & (1 << RXC0)))
        ;
    return UDR0;
}

static uint8_t uart_read_exact(uint8_t *data, uint8_t length,
                               uint16_t timeout_ms) {
    for (uint8_t i = 0; i < length; i++) {
        uint16_t ticks = timeout_ms * 100;
        while (!uart_available() && ticks--)
            _delay_us(10);
        if (!uart_available())
            return 0;
        data[i] = uart_rx();
    }
    return 1;
}

static void send_header(uint8_t command, uint32_t length) {
    uint8_t header[6] = {MAGIC, command, (uint8_t)(length >> 24),
                         (uint8_t)(length >> 16), (uint8_t)(length >> 8),
                         (uint8_t)length};
    uart_write(header, sizeof(header));
}

static void send_status(void) {
    uint32_t count = receiver_storage_count();
    uint32_t free_records = receiver_storage_capacity() - count;
    uint8_t payload[8] = {(uint8_t)(count >> 24), (uint8_t)(count >> 16),
                          (uint8_t)(count >> 8), (uint8_t)count,
                          (uint8_t)(free_records >> 24),
                          (uint8_t)(free_records >> 16),
                          (uint8_t)(free_records >> 8),
                          (uint8_t)free_records};
    send_header(CMD_STATUS, sizeof(payload));
    uart_write(payload, sizeof(payload));
}

static void send_dump(uint32_t after_id) {
    uint32_t count = receiver_storage_count();
    uint32_t first = (after_id == 0xFFFFFFFFUL) ? 0 : after_id + 1;
    if (first > count)
        first = count;

    send_header(after_id == 0xFFFFFFFFUL ? CMD_FULL : CMD_NEW,
                (count - first) * SLF_RECORD_SIZE);
    uint8_t record[SLF_RECORD_SIZE];
    for (uint32_t id = first; id < count; id++) {
        receiver_storage_read(id, record);
        uart_write(record, sizeof(record));
    }
}

static void send_error(uint8_t code) {
    send_header(CMD_ERROR, 1);
    uart_tx(code);
}

int main(void) {
    receiver_radio_init();
    receiver_storage_init();
    EICRA = (1 << ISC01) | (1 << ISC00);
    EIMSK = (1 << INT0);
    uart_init(8);
    sei();

    for (;;) {
        if (uart_available()) {
            uint8_t command = uart_rx();
            if (command == CMD_STATUS) {
                send_status();
            } else if (command == CMD_FULL) {
                send_dump(0xFFFFFFFFUL);
            } else if (command == CMD_NEW) {
                uint8_t id_data[4];
                if (uart_read_exact(id_data, sizeof(id_data), 100)) {
                    uint32_t last_id = ((uint32_t)id_data[0] << 24) |
                                       ((uint32_t)id_data[1] << 16) |
                                       ((uint32_t)id_data[2] << 8) | id_data[3];
                    send_dump(last_id);
                } else {
                    send_error(0x02);
                }
            } else if (command == CMD_CLEAR) {
                receiver_storage_clear();
                send_header(CMD_CLEAR, 0);
            } else {
                send_error(0x01);
            }
        }

        if (receiver_radio_available()) {
            uint8_t frame[SLF_FRAME_LEN];
            receiver_radio_copy_frame(frame);
            if (slf_crc8(frame, SLF_FRAME_LEN - 1) == frame[SLF_FRAME_LEN - 1])
                receiver_storage_append(frame);
            receiver_radio_wait_idle();
        }
    }
}