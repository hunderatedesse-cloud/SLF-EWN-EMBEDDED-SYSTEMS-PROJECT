/* =====================================================================
 * SLF-EWN  --  Proteus "Fake Accelerometer" (ADXL345 emulator)
 * Target: ATmega328P, 8 MHz internal RC
 *
 * SPI slave for transmitter_accelerometer.c (ADXL345).
 * DEVID 0xE5, DATAX0..DATAZ1, SPI Mode 3, multi-byte auto-increment.
 * ===================================================================== */

#define F_CPU 8000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#define REG_DEVID 0x00u
#define REG_BW_RATE 0x2Cu
#define REG_POWER_CTL 0x2Du
#define REG_DATA_FORMAT 0x31u
#define REG_DATAX0 0x32u
#define REG_DATAX1 0x33u
#define REG_DATAY0 0x34u
#define REG_DATAY1 0x35u
#define REG_DATAZ0 0x36u
#define REG_DATAZ1 0x37u

#define DEVID_VALUE 0xE5u
#define POWER_CTL_MEASURE (1u << 3)
#define REG_FILE_SIZE 0x40u

#define CS_PIN PB2
#define MOSI_PIN PB3
#define MISO_PIN PB4
#define SCK_PIN PB5
#define INT1_PIN PD2

#define MOTION_THRESHOLD_COUNTS 32
#define MS_TIMER_OCR 124u

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

static void ms_timer_init(void) {
    TCCR0A = (1 << WGM01);
    OCR0A = MS_TIMER_OCR;
    TIFR0 = (1 << OCF0A);
    TCNT0 = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);
}

static void wait_ms(uint16_t ms) {
    for (uint16_t elapsed = 0; elapsed < ms;) {
        if (TIFR0 & (1 << OCF0A)) {
            TIFR0 = (1 << OCF0A);
            elapsed++;
        }
    }
}

static volatile uint8_t regs[REG_FILE_SIZE];
static int16_t last_x = 0, last_y = 0, last_z = 0;

static int16_t adc_to_signed_axis(uint16_t adc10) {
    return (int16_t)(((int16_t)adc10 - 512) * 4);
}

static void store_axis(uint8_t reg_lsb, int16_t value) {
    regs[reg_lsb] = (uint8_t)(value & 0xFF);
    regs[reg_lsb + 1] = (uint8_t)((uint16_t)value >> 8);
}

static void sample_axes(void) {
    last_x = adc_to_signed_axis(adc_read(0));
    last_y = adc_to_signed_axis(adc_read(1));
    last_z = adc_to_signed_axis(adc_read(2));
    store_axis(REG_DATAX0, last_x);
    store_axis(REG_DATAY0, last_y);
    store_axis(REG_DATAZ0, last_z);
}

static uint8_t motion_exceeds_threshold(void) {
    int16_t dx = last_x < 0 ? -last_x : last_x;
    int16_t dy = last_y < 0 ? -last_y : last_y;
    int16_t dz = last_z < 0 ? -last_z : last_z;
    return (dx > MOTION_THRESHOLD_COUNTS) || (dy > MOTION_THRESHOLD_COUNTS) ||
           (dz > MOTION_THRESHOLD_COUNTS);
}

typedef enum { SPI_STATE_CMD, SPI_STATE_DATA } spi_state_t;
static volatile spi_state_t spi_state = SPI_STATE_CMD;
static volatile uint8_t spi_addr = 0;
static volatile uint8_t spi_is_read = 0;
static volatile uint8_t spi_multibyte = 0;

static uint8_t reg_read_safe(uint8_t addr) {
    return (addr < REG_FILE_SIZE) ? regs[addr] : 0x00;
}

static void reg_write_safe(uint8_t addr, uint8_t value) {
    if (addr < REG_FILE_SIZE && addr != REG_DEVID)
        regs[addr] = value;
}

ISR(SPI_STC_vect) {
    uint8_t received = SPDR;
    if (spi_state == SPI_STATE_CMD) {
        spi_is_read = (received & 0x80u) != 0;
        spi_multibyte = (received & 0x40u) != 0;
        spi_addr = received & 0x3Fu;
        spi_state = SPI_STATE_DATA;
        SPDR = spi_is_read ? reg_read_safe(spi_addr) : 0x00;
    } else {
        if (spi_is_read) {
            if (spi_multibyte)
                spi_addr++;
            SPDR = reg_read_safe(spi_addr);
        } else {
            reg_write_safe(spi_addr, received);
            if (spi_multibyte)
                spi_addr++;
            SPDR = 0x00;
        }
    }
}

ISR(PCINT0_vect) {
    if (PINB & (1 << CS_PIN)) {
        DDRB &= ~(1 << MISO_PIN);
    } else {
        DDRB |= (1 << MISO_PIN);
        spi_state = SPI_STATE_CMD;
    }
}

static void spi_slave_init(void) {
    DDRB &=
        ~((1 << CS_PIN) | (1 << MOSI_PIN) | (1 << SCK_PIN) | (1 << MISO_PIN));
    SPCR = (1 << SPE) | (1 << SPIE) | (1 << CPOL) | (1 << CPHA); /* Mode 3 */
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT2);
}

static void regs_init(void) {
    for (uint8_t i = 0; i < REG_FILE_SIZE; i++)
        regs[i] = 0x00;
    regs[REG_DEVID] = DEVID_VALUE;
    regs[REG_BW_RATE] = 0x0Au;
    regs[REG_POWER_CTL] = 0x00u;
    regs[REG_DATA_FORMAT] = 0x00u;
}

int main(void) {
    DDRD |= (1 << INT1_PIN);
    PORTD &= ~(1 << INT1_PIN);

    uart_init();
    adc_init();
    regs_init();
    spi_slave_init();
    ms_timer_init();
    sei();

    uart_puts("=== FAKE ACCELEROMETER (ADXL345) READY ===\r\n");

    for (;;) {
        sample_axes();
        if (motion_exceeds_threshold())
            PORTD |= (1 << INT1_PIN);
        else
            PORTD &= ~(1 << INT1_PIN);

        uart_puts("[ACCEL] DEVID:");
        uart_put_hex8(regs[REG_DEVID]);
        uart_puts(regs[REG_POWER_CTL] & POWER_CTL_MEASURE ? " MEAS" : " STBY");
        uart_puts(" X:");
        uart_put_hex8((uint8_t)last_x);
        uart_puts(" Y:");
        uart_put_hex8((uint8_t)last_y);
        uart_puts(" Z:");
        uart_put_hex8((uint8_t)last_z);
        uart_puts(motion_exceeds_threshold() ? " MOTION\r\n" : "\r\n");
        wait_ms(150);
    }
}
