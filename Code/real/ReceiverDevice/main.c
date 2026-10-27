/*
 * master_dashboard.c
 * Production receiver firmware for Qt dashboard
 *
 * Protocol: binary, big-endian, 115200 8N1
 * Record = 16 bytes (4-byte RID + 12-byte LoRa frame)
 */
#define F_CPU 16000000UL
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
/* ---------- Pins ---------- */
#define RADIO_CS_PORT PORTB
#define RADIO_CS_PIN PB2
#define FLASH_CS_PORT PORTB
#define FLASH_CS_PIN PB1
#define DIO0_PIN PD2
#define FRAME_LEN 12
#define RECORD_SIZE 16
#define EE_HEADER_ADDR 0x000000UL
#define EE_DATA_START 0x000010UL
#define EE_MAX_RECORDS ((131072UL - EE_DATA_START) / RECORD_SIZE)
/* Protocol */
#define MAGIC 0xAA
#define CMD_STATUS 0x53
#define CMD_FULL 0x44
#define CMD_NEW 0x4E
#define CMD_CLEAR 0x43
#define CMD_ERROR 0xFF
/* ---------- Globals ---------- */
static volatile uint8_t packet_ready = 0;
static uint8_t rx[FRAME_LEN];
static uint32_t write_count = 0;
/* ---------- CRC ---------- */
static uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}
/* ---------- UART ---------- */
static void uart_init(uint16_t ubrr) {
    UBRR0H = (ubrr >> 8);
    UBRR0L = ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
static void uart_tx(uint8_t b) {
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = b;
}
static void uart_write(const uint8_t *buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++)
        uart_tx(buf[i]);
}
static uint8_t uart_available(void) { return (UCSR0A & (1 << RXC0)) != 0; }
static uint8_t uart_rx(void) {
    while (!(UCSR0A & (1 << RXC0)))
        ;
    return UDR0;
}
/* Read exactly n bytes with timeout (returns 0 on timeout) */
static uint8_t uart_read_exact(uint8_t *buf, uint8_t n, uint16_t timeout_ms) {
    for (uint8_t i = 0; i < n; i++) {
        uint16_t t = timeout_ms * 100;
        while (!uart_available() && t--)
            _delay_us(10);
        if (!uart_available())
            return 0;
        buf[i] = uart_rx();
    }
    return 1;
}
/* ---------- SPI ---------- */
static void spi_set_mode0(void) {
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
    SPSR = 0;
}
static void spi_set_mode1(void) {
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0) | (1 << CPHA);
    SPSR = 0;
}
static uint8_t spi_transfer(uint8_t out) {
    SPDR = out;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}
/* ---------- Flash ---------- */
static void flash_select(void) {
    FLASH_CS_PORT &= ~(1 << FLASH_CS_PIN);
    _delay_us(20);
}
static void flash_deselect(void) {
    FLASH_CS_PORT |= (1 << FLASH_CS_PIN);
    _delay_us(20);
}
static void flash_wait_ready(void) {
    uint8_t st;
    do {
        flash_select();
        spi_transfer(0x05);
        st = spi_transfer(0xFF);
        flash_deselect();
    } while (st & 0x01);
}
static void flash_wren(void) {
    flash_select();
    spi_transfer(0x06);
    flash_deselect();
    _delay_us(50);
}
static void flash_write_bytes(uint32_t addr, const uint8_t *data, uint8_t len) {
    spi_set_mode0();
    flash_wren();
    flash_select();
    spi_transfer(0x02);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint8_t i = 0; i < len; i++)
        spi_transfer(data[i]);
    flash_deselect();
    flash_wait_ready();
}
static void flash_read_bytes(uint32_t addr, uint8_t *buf, uint8_t len) {
    spi_set_mode0();
    flash_select();
    spi_transfer(0x03);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint8_t i = 0; i < len; i++)
        buf[i] = spi_transfer(0xFF);
    flash_deselect();
}
/* ---------- EEPROM ---------- */
static void ee_load_count(void) {
    uint8_t tmp[4];
    flash_read_bytes(EE_HEADER_ADDR, tmp, 4);
    write_count = ((uint32_t)tmp[0] << 24) | ((uint32_t)tmp[1] << 16) |
                  ((uint32_t)tmp[2] << 8) | (uint32_t)tmp[3];
    if (write_count > EE_MAX_RECORDS)
        write_count = 0;
}
static void ee_save_count(void) {
    uint8_t tmp[4] = {(write_count >> 24) & 0xFF, (write_count >> 16) & 0xFF,
                      (write_count >> 8) & 0xFF, write_count & 0xFF};
    flash_write_bytes(EE_HEADER_ADDR, tmp, 4);
}
static void ee_append_frame(const uint8_t *frame) {
    if (write_count >= EE_MAX_RECORDS)
        return;
    uint8_t rec[RECORD_SIZE];
    rec[0] = (write_count >> 24) & 0xFF;
    rec[1] = (write_count >> 16) & 0xFF;
    rec[2] = (write_count >> 8) & 0xFF;
    rec[3] = write_count & 0xFF;
    memcpy(&rec[4], frame, FRAME_LEN);
    uint32_t addr = EE_DATA_START + (write_count * RECORD_SIZE);
    flash_write_bytes(addr, rec, RECORD_SIZE);
    write_count++;
    ee_save_count();
}
/* ---------- Protocol helpers ---------- */
static void send_header(uint8_t cmd, uint32_t length) {
    uint8_t hdr[6] = {MAGIC,
                      cmd,
                      (length >> 24) & 0xFF,
                      (length >> 16) & 0xFF,
                      (length >> 8) & 0xFF,
                      length & 0xFF};
    uart_write(hdr, 6);
}
static void send_status(void) {
    uint32_t free = EE_MAX_RECORDS - write_count;
    uint8_t payload[8] = {
        (write_count >> 24) & 0xFF, (write_count >> 16) & 0xFF,
        (write_count >> 8) & 0xFF,  write_count & 0xFF,
        (free >> 24) & 0xFF,        (free >> 16) & 0xFF,
        (free >> 8) & 0xFF,         free & 0xFF};
    send_header(CMD_STATUS, 8);
    uart_write(payload, 8);
}
static void send_dump(uint32_t after_id) {
    uint32_t start = (after_id == 0xFFFFFFFFUL) ? 0 : (after_id + 1);
    if (start > write_count)
        start = write_count;
    uint32_t count = write_count - start;
    uint32_t length = count * RECORD_SIZE;
    send_header((after_id == 0xFFFFFFFFUL) ? CMD_FULL : CMD_NEW, length);
    uint8_t buf[RECORD_SIZE];
    for (uint32_t i = start; i < write_count; i++) {
        uint32_t addr = EE_DATA_START + (i * RECORD_SIZE);
        flash_read_bytes(addr, buf, RECORD_SIZE);
        uart_write(buf, RECORD_SIZE);
    }
}
static void send_error(uint8_t code) {
    send_header(CMD_ERROR, 1);
    uart_tx(code);
}
/* ---------- LoRa ISR ---------- */
ISR(INT0_vect) {
    if (packet_ready)
        return;
    spi_set_mode1();
    RADIO_CS_PORT &= ~(1 << RADIO_CS_PIN);
    _delay_us(200);
    for (uint8_t i = 0; i < FRAME_LEN; i++) {
        rx[i] = spi_transfer(0xFF);
        _delay_us(150);
    }
    RADIO_CS_PORT |= (1 << RADIO_CS_PIN);
    packet_ready = 1;
}
/* ---------- Main ---------- */
int main(void) {
    /* GPIO */
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << RADIO_CS_PIN) | (1 << FLASH_CS_PIN);
    DDRB &= ~(1 << PB4);
    RADIO_CS_PORT |= (1 << RADIO_CS_PIN);
    FLASH_CS_PORT |= (1 << FLASH_CS_PIN);
    DDRD &= ~(1 << DIO0_PIN);
    /* INT0 rising edge */
    EICRA = (1 << ISC01) | (1 << ISC00);
    EIMSK = (1 << INT0);
    uart_init(8); /* 115200 @ 16 MHz */
    spi_set_mode0();
    sei();
    ee_load_count();
    while (1) {
        /* ---- Handle dashboard commands ---- */
        if (uart_available()) {
            uint8_t cmd = uart_rx();
            if (cmd == CMD_STATUS) {
                send_status();
            } else if (cmd == CMD_FULL) {
                send_dump(0xFFFFFFFFUL);
            } else if (cmd == CMD_NEW) {
                uint8_t idbuf[4];
                if (uart_read_exact(idbuf, 4, 100)) {
                    uint32_t last = ((uint32_t)idbuf[0] << 24) |
                                    ((uint32_t)idbuf[1] << 16) |
                                    ((uint32_t)idbuf[2] << 8) |
                                    (uint32_t)idbuf[3];
                    send_dump(last);
                } else {
                    send_error(0x02); /* timeout */
                }
            } else if (cmd == CMD_CLEAR) {
                write_count = 0;
                ee_save_count();
                send_header(CMD_CLEAR, 0);
            } else {
                send_error(0x01); /* unknown */
            }
        }
        /* ---- Handle LoRa packet ---- */
        if (packet_ready) {
            if (crc8(rx, 11) == rx[11]) {
                ee_append_frame(rx);
            }
            packet_ready = 0;
            while (PIND & (1 << DIO0_PIN))
                ;
        }
    }
}
