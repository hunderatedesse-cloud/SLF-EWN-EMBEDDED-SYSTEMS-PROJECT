/* =====================================================================
 * SLF-EWN  --  Tower LoRa receiver
 * Module : AI-Thinker Ra-02 (Semtech SX1278)
 * Bus    : SPI Master Mode 0 (same as flash driver)
 *
 * Full SX1278 register driver. Public API unchanged.
 *
 * RF settings must match the node:
 *   433.0 MHz, SF7, BW 125 kHz, CR 4/5, explicit header, CRC on,
 *   sync word 0x12, preamble 8
 *
 * Continuous RX. DIO0 = RxDone -> INT0 reads FIFO into buffer.
 * ===================================================================== */

#define F_CPU 16000000UL

#include "receiver_radio.h"
#include "../Common/slf_protocol.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#define RADIO_CS_PIN PB2
#define RADIO_DIO0_PIN PD2

#define REG_FIFO              0x00u
#define REG_OP_MODE           0x01u
#define REG_FRF_MSB           0x06u
#define REG_FRF_MID           0x07u
#define REG_FRF_LSB           0x08u
#define REG_LNA               0x0Cu
#define REG_FIFO_ADDR_PTR     0x0Du
#define REG_FIFO_TX_BASE      0x0Eu
#define REG_FIFO_RX_BASE      0x0Fu
#define REG_FIFO_RX_CURRENT   0x10u
#define REG_IRQ_FLAGS         0x12u
#define REG_RX_NB_BYTES       0x13u
#define REG_MODEM_CONFIG_1    0x1Du
#define REG_MODEM_CONFIG_2    0x1Eu
#define REG_PREAMBLE_MSB      0x20u
#define REG_PREAMBLE_LSB      0x21u
#define REG_PAYLOAD_LENGTH    0x22u
#define REG_MODEM_CONFIG_3    0x26u
#define REG_SYNC_WORD         0x39u
#define REG_DIO_MAPPING_1     0x40u
#define REG_VERSION           0x42u

#define MODE_LONG_RANGE       0x80u
#define MODE_SLEEP            0x00u
#define MODE_STDBY            0x01u
#define MODE_RXCONTINUOUS     0x05u

#define IRQ_RX_DONE           0x40u
#define IRQ_PAYLOAD_CRC_ERROR 0x20u

#define FRF_433_MSB           0x6Cu
#define FRF_433_MID           0x53u
#define FRF_433_LSB           0x33u

#define SX127x_VERSION        0x12u
#define RADIO_TIMER_OCR_1MS   249u

static volatile uint8_t packet_ready;
static volatile uint8_t received_frame[SLF_FRAME_LEN];
static uint8_t radio_ok;

static void select_radio(void) { PORTB &= ~(1 << RADIO_CS_PIN); }
static void deselect_radio(void) { PORTB |= (1 << RADIO_CS_PIN); }

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

static void spi_set_mode0(void) {
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
    SPSR = 0;
}

static void radio_wait_ms(uint16_t ms) {
    TCCR0A = (1 << WGM01);
    OCR0A = RADIO_TIMER_OCR_1MS;
    TIFR0 = (1 << OCF0A);
    TCNT0 = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);
    for (uint16_t elapsed = 0; elapsed < ms;) {
        if (TIFR0 & (1 << OCF0A)) {
            TIFR0 = (1 << OCF0A);
            elapsed++;
        }
    }
    TCCR0B = 0;
}

static uint8_t read_reg(uint8_t addr) {
    uint8_t value;
    spi_set_mode0();
    select_radio();
    spi_transfer(addr & 0x7Fu);
    value = spi_transfer(0x00);
    deselect_radio();
    return value;
}

static void write_reg(uint8_t addr, uint8_t value) {
    spi_set_mode0();
    select_radio();
    spi_transfer(addr | 0x80u);
    spi_transfer(value);
    deselect_radio();
}

static void set_mode(uint8_t mode) {
    write_reg(REG_OP_MODE, (uint8_t)(MODE_LONG_RANGE | mode));
}

static void start_rx_continuous(void) {
    write_reg(REG_FIFO_ADDR_PTR, 0x00u);
    write_reg(REG_IRQ_FLAGS, 0xFFu);
    set_mode(MODE_RXCONTINUOUS);
}

ISR(INT0_vect) {
    if (packet_ready || !radio_ok)
        return;

    uint8_t flags = read_reg(REG_IRQ_FLAGS);
    write_reg(REG_IRQ_FLAGS, 0xFFu);

    if (!(flags & IRQ_RX_DONE) || (flags & IRQ_PAYLOAD_CRC_ERROR)) {
        start_rx_continuous();
        return;
    }

    uint8_t nb = read_reg(REG_RX_NB_BYTES);
    uint8_t addr = read_reg(REG_FIFO_RX_CURRENT);
    write_reg(REG_FIFO_ADDR_PTR, addr);

    spi_set_mode0();
    select_radio();
    spi_transfer(REG_FIFO & 0x7Fu);
    for (uint8_t i = 0; i < SLF_FRAME_LEN; i++) {
        uint8_t b = spi_transfer(0x00);
        received_frame[i] = (i < nb) ? b : 0;
    }
    deselect_radio();

    if (nb >= SLF_FRAME_LEN)
        packet_ready = 1;

    start_rx_continuous();
}

void receiver_radio_init(void) {
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << RADIO_CS_PIN);
    DDRB &= ~(1 << PB4);
    deselect_radio();
    DDRD &= ~(1 << RADIO_DIO0_PIN);
    packet_ready = 0;
    radio_ok = 0;

    write_reg(REG_OP_MODE, MODE_SLEEP);
    radio_wait_ms(10);
    write_reg(REG_OP_MODE, MODE_LONG_RANGE | MODE_SLEEP);
    radio_wait_ms(10);

    if (read_reg(REG_VERSION) != SX127x_VERSION)
        return;
    radio_ok = 1;

    set_mode(MODE_STDBY);

    write_reg(REG_FRF_MSB, FRF_433_MSB);
    write_reg(REG_FRF_MID, FRF_433_MID);
    write_reg(REG_FRF_LSB, FRF_433_LSB);

    write_reg(REG_LNA, 0x23u);
    write_reg(REG_FIFO_TX_BASE, 0x00u);
    write_reg(REG_FIFO_RX_BASE, 0x00u);

    write_reg(REG_MODEM_CONFIG_1, 0x72u);
    write_reg(REG_MODEM_CONFIG_2, 0x74u);
    write_reg(REG_MODEM_CONFIG_3, 0x04u);

    write_reg(REG_PREAMBLE_MSB, 0x00u);
    write_reg(REG_PREAMBLE_LSB, 0x08u);
    write_reg(REG_SYNC_WORD, 0x12u);
    write_reg(REG_PAYLOAD_LENGTH, SLF_FRAME_LEN);

    write_reg(REG_DIO_MAPPING_1, 0x00u);

    start_rx_continuous();
}

uint8_t receiver_radio_available(void) { return packet_ready; }

void receiver_radio_copy_frame(uint8_t *frame) {
    for (uint8_t i = 0; i < SLF_FRAME_LEN; i++)
        frame[i] = received_frame[i];
    packet_ready = 0;
}

void receiver_radio_wait_idle(void) {
    while (PIND & (1 << RADIO_DIO0_PIN))
        ;
}
