/* =====================================================================
 * SLF-EWN  --  Proteus "Fake Accelerometer" (LIS3DH emulator) Child Sheet
 * Target: ATmega328P, 8 MHz internal RC, 3.6 V
 * ===================================================================== */

#define F_CPU 8000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define REG_WHOAMI 0x0Fu
#define REG_OUT_X 0x29u
#define REG_OUT_Y 0x2Bu
#define REG_OUT_Z 0x2Du
#define WHOAMI_VALUE 0x33u
#define READ_BIT 0x80u

#define CS_PIN PB2
#define MOSI_PIN PB3
#define MISO_PIN PB4
#define SCK_PIN PB5
#define INT1_PIN PD2

#define MOTION_THRESHOLD_COUNTS 32

/* ---------------- UART (debug) ------------------------------------------- */

static void uart_init(void) {
    uint16_t ubrr = (F_CPU / (16UL * 9600UL)) - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = c;
}

static void uart_puts(const char *s) {
    while (*s)
        uart_putc(*s++);
}

static void uart_put_hex8(uint8_t v) {
    const char hex[] = "0123456789ABCDEF";
    uart_putc('0');
    uart_putc('x');
    uart_putc(hex[(v >> 4) & 0xF]);
    uart_putc(hex[v & 0xF]);
}

/* ---------------- ADC ---------------------------------------------------- */

static void adc_init(void) {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

static uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    return ADC;
}

static uint8_t adc_to_reg8(uint16_t adc10) { return (uint8_t)(adc10 >> 2); }

static volatile uint8_t last_x = 0x80, last_y = 0x80, last_z = 0x80;

static void sample_axes(void) {
    last_x = adc_to_reg8(adc_read(0));
    last_y = adc_to_reg8(adc_read(1));
    last_z = adc_to_reg8(adc_read(2));
}

static uint8_t motion_exceeds_threshold(void) {
    int16_t dx = (int16_t)last_x - 0x80;
    int16_t dy = (int16_t)last_y - 0x80;
    int16_t dz = (int16_t)last_z - 0x80;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    if (dz < 0)
        dz = -dz;
    return (dx > MOTION_THRESHOLD_COUNTS) || (dy > MOTION_THRESHOLD_COUNTS) ||
           (dz > MOTION_THRESHOLD_COUNTS);
}

/* ---------------- SPI slave ---------------------------------------------- */

static volatile uint8_t pending_reg = 0;
static volatile uint8_t have_pending_reg = 0;

static uint8_t lookup_register(uint8_t reg) {
    switch (reg) {
    case REG_WHOAMI:
        return WHOAMI_VALUE;
    case REG_OUT_X:
        return last_x;
    case REG_OUT_Y:
        return last_y;
    case REG_OUT_Z:
        return last_z;
    default:
        return 0x00;
    }
}

ISR(SPI_STC_vect) {
    uint8_t received = SPDR;
    if (!have_pending_reg) {
        pending_reg = received & 0x7F;
        have_pending_reg = 1;
        SPDR = lookup_register(pending_reg);
    } else {
        have_pending_reg = 0;
        SPDR = 0x00;
    }
}

ISR(PCINT0_vect) {
    if (PINB & (1 << CS_PIN)) {
        DDRB &= ~(1 << MISO_PIN); /* high-Z when deselected */
    } else {
        DDRB |= (1 << MISO_PIN); /* drive MISO when selected */
    }
    have_pending_reg = 0;
}

static void spi_slave_init(void) {
    DDRB &=
        ~((1 << CS_PIN) | (1 << MOSI_PIN) | (1 << SCK_PIN) | (1 << MISO_PIN));
    SPCR = (1 << SPE) | (1 << SPIE);
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT2); /* PB2 = CS */
}

/* ---------------- main --------------------------------------------------- */

int main(void) {
    DDRD |= (1 << INT1_PIN);
    PORTD &= ~(1 << INT1_PIN);

    uart_init();
    adc_init();
    spi_slave_init();
    sei();

    uart_puts("=== FAKE ACCELEROMETER READY ===\r\n");

    for (;;) {
        sample_axes();

        if (motion_exceeds_threshold()) {
            PORTD |= (1 << INT1_PIN);
        } else {
            PORTD &= ~(1 << INT1_PIN);
        }

        uart_puts("[ACCEL] X:");
        uart_put_hex8(last_x);
        uart_puts(" Y:");
        uart_put_hex8(last_y);
        uart_puts(" Z:");
        uart_put_hex8(last_z);
        uart_puts(motion_exceeds_threshold() ? " MOTION\r\n" : "\r\n");

        _delay_ms(150);
    }
}
