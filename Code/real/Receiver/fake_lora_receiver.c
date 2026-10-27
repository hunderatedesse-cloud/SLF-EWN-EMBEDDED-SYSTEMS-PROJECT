/*
 * fake_lora_rx.c  –  pure binary receiver version
 * Target: ATmega328P @ 8 MHz
 */

#define F_CPU 8000000UL
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define DIO0_PORT PORTD
#define DIO0_PIN PD3

#define NSS_PIN PB2
#define MOSI_PIN PB3
#define MISO_PIN PB4
#define SCK_PIN PB5

#define FRAME_LEN 12

static uint8_t frame[FRAME_LEN];
static volatile uint8_t idx = 0;
static volatile uint8_t sending = 0;

/* ---------------- CRC ----------------------------------- */
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

/* ---------------- UART (ANT input) ---------------------- */
static void uart_init(uint16_t ubrr) {
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); /* RX = ANT, TX = debug */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_tx(uint8_t b) {
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = b;
}

static void uart_print(const char *s) {
    while (*s)
        uart_tx((uint8_t)*s++);
}

static void uart_print_hex(uint8_t b) {
    const char hex[] = "0123456789ABCDEF";
    uart_tx(hex[b >> 4]);
    uart_tx(hex[b & 0x0F]);
}

static uint8_t uart_available(void) { return (UCSR0A & (1 << RXC0)) != 0; }

static uint8_t uart_rx(void) {
    while (!(UCSR0A & (1 << RXC0)))
        ;
    return UDR0;
}

/* ---------------- SPI slave ----------------------------- */
static void spi_slave_init(void) {
    DDRB &= ~((1 << NSS_PIN) | (1 << MOSI_PIN) | (1 << SCK_PIN));
    DDRB |= (1 << MISO_PIN);
    SPCR = (1 << SPE) | (1 << CPHA); /* Mode 1 – matches Master */
    SPDR = 0x00;
}

/* ---------------- main ---------------------------------- */
int main(void) {
    DDRD |= (1 << DIO0_PIN);
    DIO0_PORT &= ~(1 << DIO0_PIN);

    uart_init(51); /* 9600 @ 8 MHz – MUST match TX */
    spi_slave_init();

    uart_print("\r\n=== FAKE LORA RX READY (pure binary) ===\r\n");

    uint8_t rx_cnt = 0;

    while (1) {
        /* Collect exactly 12 bytes from ANT */
        if (uart_available() && !sending) {
            frame[rx_cnt++] = uart_rx();

            if (rx_cnt >= FRAME_LEN) {
                rx_cnt = 0;

                if (crc8(frame, 11) == frame[11]) {
                    /* Good frame → deliver to Master */
                    uart_print(">>> OK node=");
                    uart_print_hex(frame[1]);
                    uart_print(" seq=");
                    uart_print_hex(frame[10] & 0x7F);
                    if (frame[10] & 0x80)
                        uart_print(" EMERGENCY");
                    uart_print("\r\n");

                    idx = 0;
                    SPDR = frame[0];
                    DIO0_PORT |= (1 << DIO0_PIN);
                    sending = 1;
                } else {
                    uart_print("[CRC FAIL]\r\n");
                }
            }
        }

        /* SPI handshake with Master */
        if (SPSR & (1 << SPIF)) {
            (void)SPDR;
            if (sending) {
                idx++;
                if (idx < FRAME_LEN) {
                    SPDR = frame[idx];
                } else {
                    DIO0_PORT &= ~(1 << DIO0_PIN);
                    sending = 0;
                    SPDR = 0x00;
                    uart_print(">>> delivered\r\n");
                }
            }
        }
    }
}
