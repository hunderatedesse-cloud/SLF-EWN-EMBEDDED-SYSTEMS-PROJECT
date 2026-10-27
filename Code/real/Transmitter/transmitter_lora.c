/* =====================================================================
 * SLF-EWN  --  Buried Sensor Node LoRa transmitter
 * Module : AI-Thinker Ra-02 (Semtech SX1278)
 * Bus    : Shared SPI Master Mode 0 (accel briefly switches to Mode 3)
 *
 * Full SX1278 register driver. Public API unchanged:
 *   transmitter_lora_init()
 *   transmitter_lora_send(frame, length)  -> 1 if TxDone, 0 on timeout
 *
 * RF settings (must match the tower receiver):
 *   433.0 MHz, SF7, BW 125 kHz, CR 4/5, explicit header, CRC on,
 *   sync word 0x12, preamble 8, PA_BOOST ~+14..17 dBm
 * ===================================================================== */

#define F_CPU 8000000UL

#include "transmitter_lora.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#define LORA_NSS_PIN PB1
#define LORA_DIO0_PIN PD3

/* ---- SX127x registers (LoRa page) ------------------------------------ */
#define REG_FIFO              0x00u
#define REG_OP_MODE           0x01u
#define REG_FRF_MSB           0x06u
#define REG_FRF_MID           0x07u
#define REG_FRF_LSB           0x08u
#define REG_PA_CONFIG         0x09u
#define REG_OCP               0x0Bu
#define REG_LNA               0x0Cu
#define REG_FIFO_ADDR_PTR     0x0Du
#define REG_FIFO_TX_BASE      0x0Eu
#define REG_FIFO_RX_BASE      0x0Fu
#define REG_IRQ_FLAGS         0x12u
#define REG_MODEM_CONFIG_1    0x1Du
#define REG_MODEM_CONFIG_2    0x1Eu
#define REG_PREAMBLE_MSB      0x20u
#define REG_PREAMBLE_LSB      0x21u
#define REG_PAYLOAD_LENGTH    0x22u
#define REG_MODEM_CONFIG_3    0x26u
#define REG_SYNC_WORD         0x39u
#define REG_DIO_MAPPING_1     0x40u
#define REG_VERSION           0x42u
#define REG_PA_DAC            0x4Du

#define MODE_LONG_RANGE       0x80u
#define MODE_SLEEP            0x00u
#define MODE_STDBY            0x01u
#define MODE_TX               0x03u

#define IRQ_TX_DONE           0x08u

/* 433.0 MHz: Frf = f * 2^19 / 32e6 */
#define FRF_433_MSB           0x6Cu
#define FRF_433_MID           0x53u
#define FRF_433_LSB           0x33u

#define SX127x_VERSION        0x12u
#define LORA_TIMER_OCR_1MS    124u

static volatile uint8_t transmit_complete;
static uint8_t radio_ok;

static void select_radio(void) { PORTB &= ~(1 << LORA_NSS_PIN); }
static void deselect_radio(void) { PORTB |= (1 << LORA_NSS_PIN); }

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

static void spi_set_mode0(void) {
    SPCR &= ~((1 << CPOL) | (1 << CPHA));
}

static void lora_wait_ms(uint16_t ms) {
    TCCR0A = (1 << WGM01);
    OCR0A = LORA_TIMER_OCR_1MS;
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

static void write_fifo(const uint8_t *data, uint8_t length) {
    spi_set_mode0();
    select_radio();
    spi_transfer(REG_FIFO | 0x80u);
    for (uint8_t i = 0; i < length; i++)
        spi_transfer(data[i]);
    deselect_radio();
}

static void set_mode(uint8_t mode) {
    write_reg(REG_OP_MODE, (uint8_t)(MODE_LONG_RANGE | mode));
}

ISR(INT1_vect) { transmit_complete = 1; }

void transmitter_lora_init(void) {
    DDRB |= (1 << LORA_NSS_PIN);
    DDRD &= ~(1 << LORA_DIO0_PIN);
    deselect_radio();
    transmit_complete = 0;
    radio_ok = 0;

    write_reg(REG_OP_MODE, MODE_SLEEP);
    lora_wait_ms(10);
    write_reg(REG_OP_MODE, MODE_LONG_RANGE | MODE_SLEEP);
    lora_wait_ms(10);

    if (read_reg(REG_VERSION) != SX127x_VERSION)
        return;
    radio_ok = 1;

    set_mode(MODE_STDBY);

    write_reg(REG_FRF_MSB, FRF_433_MSB);
    write_reg(REG_FRF_MID, FRF_433_MID);
    write_reg(REG_FRF_LSB, FRF_433_LSB);

    write_reg(REG_PA_CONFIG, 0x8Fu);
    write_reg(REG_OCP, 0x2Bu);
    write_reg(REG_PA_DAC, 0x84u);
    write_reg(REG_LNA, 0x23u);

    write_reg(REG_FIFO_TX_BASE, 0x00u);
    write_reg(REG_FIFO_RX_BASE, 0x00u);

    write_reg(REG_MODEM_CONFIG_1, 0x72u);
    write_reg(REG_MODEM_CONFIG_2, 0x74u);
    write_reg(REG_MODEM_CONFIG_3, 0x04u);

    write_reg(REG_PREAMBLE_MSB, 0x00u);
    write_reg(REG_PREAMBLE_LSB, 0x08u);
    write_reg(REG_SYNC_WORD, 0x12u);
    write_reg(REG_PAYLOAD_LENGTH, 12u);

    write_reg(REG_DIO_MAPPING_1, 0x40u);
    write_reg(REG_IRQ_FLAGS, 0xFFu);
    set_mode(MODE_STDBY);
}

uint8_t transmitter_lora_send(const uint8_t *frame, uint8_t length) {
    if (!radio_ok || length == 0 || length > 255)
        return 0;

    transmit_complete = 0;

    set_mode(MODE_STDBY);
    write_reg(REG_PAYLOAD_LENGTH, length);
    write_reg(REG_FIFO_ADDR_PTR, 0x00u);
    write_fifo(frame, length);

    write_reg(REG_IRQ_FLAGS, 0xFFu);
    set_mode(MODE_TX);

    uint16_t timeout_ms = 2000;
    while (!transmit_complete && timeout_ms > 0) {
        if (read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE) {
            transmit_complete = 1;
            break;
        }
        lora_wait_ms(1);
        timeout_ms--;
    }

    write_reg(REG_IRQ_FLAGS, 0xFFu);
    set_mode(MODE_STDBY);
    return transmit_complete;
}
