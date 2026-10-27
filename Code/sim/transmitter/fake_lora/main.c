/*
 * fake_lora_tx.c  –  pure binary broadcast version
 * Target: ATmega328P @ 8 MHz
 */

#define F_CPU 8000000UL
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define LORA_CMD_FIFO_WRITE 0x8Du
#define PACKET_LEN 12u

#define NSS_PIN PB2
#define MOSI_PIN PB3
#define MISO_PIN PB4
#define SCK_PIN PB5
#define DIO0_PIN PD3

/* ---------------- UART (ANT output only) ---------------- */
static void uart_init(void) {
    uint16_t ubrr = (F_CPU / (16UL * 9600UL)) - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_putc(uint8_t b) {
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = b;
}

/* Send exactly 12 binary bytes – nothing else */
static void uart_send_raw(const uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        uart_putc(buf[i]);
        _delay_us(300); /* slow & reliable for Proteus */
    }
}

/* ---------------- CRC ----------------------------------- */
static uint8_t crc8_smbus(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ---------------- SPI slave ----------------------------- */
static volatile uint8_t rx_buf[PACKET_LEN + 1];
static volatile uint8_t rx_index = 0;
static volatile uint8_t frame_ready = 0;

ISR(SPI_STC_vect) {
    uint8_t received = SPDR;
    if (rx_index < sizeof(rx_buf))
        rx_buf[rx_index++] = received;
    SPDR = 0x00;
}

ISR(PCINT0_vect) {
    if (PINB & (1 << NSS_PIN)) {
        DDRB &= ~(1 << MISO_PIN);
        if (rx_index >= (PACKET_LEN + 1) && rx_buf[0] == LORA_CMD_FIFO_WRITE)
            frame_ready = 1;
        rx_index = 0;
    } else {
        DDRB |= (1 << MISO_PIN);
        rx_index = 0;
        frame_ready = 0;
    }
}

static void spi_slave_init(void) {
    DDRB &=
        ~((1 << NSS_PIN) | (1 << MOSI_PIN) | (1 << SCK_PIN) | (1 << MISO_PIN));
    SPCR = (1 << SPE) | (1 << SPIE);
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT2);
}

static void dio0_pulse(void) {
    PORTD |= (1 << DIO0_PIN);
    _delay_ms(2);
    PORTD &= ~(1 << DIO0_PIN);
}

/* ---------------- main ---------------------------------- */
int main(void) {
    DDRD |= (1 << DIO0_PIN);
    PORTD &= ~(1 << DIO0_PIN);

    uart_init();
    spi_slave_init();
    sei();

    for (;;) {
        if (frame_ready) {
            frame_ready = 0;

            /* Extract the 12-byte packet (skip the 0x8D command) */
            uint8_t local[PACKET_LEN];
            for (uint8_t i = 0; i < PACKET_LEN; i++)
                local[i] = rx_buf[i + 1];

            /* Optional safety check */
            if (crc8_smbus(local, 11) == local[11]) {
                /* === PURE BINARY BROADCAST ON ANT === */
                uart_send_raw(local, PACKET_LEN);
            }

            dio0_pulse();
        }
    }
}
