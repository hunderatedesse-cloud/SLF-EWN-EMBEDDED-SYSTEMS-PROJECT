/* =====================================================================
 * SLF-EWN  --  Buried Sensor Node - MASTER FIRMWARE (updated)
 * Target: ATmega328P, 8 MHz internal RC, 3.6 V
 * ===================================================================== */

#define F_CPU 8000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>

/* ---------------- Configuration -------------------------------------- */

#define DEVICE_ID 0x0104u

#define PRODUCTION_INTERVAL_S (15UL * 60UL)
#define WDT_TICK_S 8UL
#define WAKE_INTERVAL_WDT_TICKS (PRODUCTION_INTERVAL_S / WDT_TICK_S)

#define SIM_FAST_WAKE 1
#ifdef SIM_FAST_WAKE
#undef WAKE_INTERVAL_WDT_TICKS
#define WAKE_INTERVAL_WDT_TICKS SIM_FAST_WAKE
#endif

#define STATUS_EMERGENCY_BIT 0x80u
#define LORA_CMD_FIFO_WRITE 0x8Du

#define ACC_REG_WHOAMI 0x0Fu
#define ACC_REG_OUT_X 0x29u
#define ACC_REG_OUT_Y 0x2Bu
#define ACC_REG_OUT_Z 0x2Du
#define ACC_WHOAMI_VALUE 0x33u
#define ACC_READ_BIT 0x80u

/* ---------------- Pins ----------------------------------------------- */

#define NSS_LORA_BIT PB1
#define CS_ACCEL_BIT PB2
#define GATE_BIT PD4

static inline void lora_select(void) { PORTB &= ~(1 << NSS_LORA_BIT); }
static inline void lora_deselect(void) { PORTB |= (1 << NSS_LORA_BIT); }
static inline void acc_select(void) { PORTB &= ~(1 << CS_ACCEL_BIT); }
static inline void acc_deselect(void) { PORTB |= (1 << CS_ACCEL_BIT); }
static inline void sensor_rail_on(void) { PORTD &= ~(1 << GATE_BIT); }
static inline void sensor_rail_off(void) { PORTD |= (1 << GATE_BIT); }

/* ---------------- Globals -------------------------------------------- */

static volatile uint16_t wdt_tick_count = 0;
static volatile uint8_t motion_wake_flag = 0;
static volatile uint8_t lora_txdone_flag = 0;
static uint8_t seq_counter = 0; /* sequence number */

ISR(WDT_vect) { wdt_tick_count++; }
ISR(INT0_vect) { motion_wake_flag = 1; }
ISR(INT1_vect) { lora_txdone_flag = 1; }

/* ---------------- UART ----------------------------------------------- */

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

static void uart_put_u16(uint16_t v) {
    char buf[6];
    uint8_t i = 0;
    if (v == 0) {
        uart_putc('0');
        return;
    }
    while (v > 0 && i < sizeof(buf)) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i > 0)
        uart_putc(buf[--i]);
}

static void uart_put_i16(int16_t v) {
    if (v < 0) {
        uart_putc('-');
        uart_put_u16((uint16_t)(-(int32_t)v));
    } else {
        uart_put_u16((uint16_t)v);
    }
}

/* ---------------- SPI ------------------------------------------------ */

static void spi_master_init(void) {
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << NSS_LORA_BIT) | (1 << CS_ACCEL_BIT);
    DDRB &= ~(1 << PB4);
    lora_deselect();
    acc_deselect();
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0); /* Mode 0, /16 */
}

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

/* ---------------- ADC ------------------------------------------------ */

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

/* ---------------- CRC ------------------------------------------------ */

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

/* ---------------- Accelerometer driver ------------------------------- */

static uint8_t accel_read_reg(uint8_t reg) {
    uint8_t value;
    acc_select();
    _delay_us(10);
    spi_transfer(reg | ACC_READ_BIT);
    _delay_us(10);
    value = spi_transfer(0x00);
    acc_deselect();
    return value;
}

static uint8_t accel_read_tilt(int16_t *tilt_x, int16_t *tilt_y,
                               int16_t *tilt_z) {
    if (accel_read_reg(ACC_REG_WHOAMI) != ACC_WHOAMI_VALUE)
        return 0;

    uint8_t rx = accel_read_reg(ACC_REG_OUT_X);
    uint8_t ry = accel_read_reg(ACC_REG_OUT_Y);
    uint8_t rz = accel_read_reg(ACC_REG_OUT_Z);

    *tilt_x = ((int16_t)rx - 128) * (2000 / 128);
    *tilt_y = ((int16_t)ry - 128) * (2000 / 128);
    *tilt_z = ((int16_t)rz - 128) * (2000 / 128);
    return 1;
}

/* ---------------- LoRa driver ---------------------------------------- */

static void lora_transmit(const uint8_t *packet, uint8_t len) {
    lora_txdone_flag = 0;

    lora_select();
    spi_transfer(LORA_CMD_FIFO_WRITE);
    for (uint8_t i = 0; i < len; i++) {
        spi_transfer(packet[i]);
        _delay_us(250);
    }
    lora_deselect();

    uint16_t timeout_ms = 1000;
    while (!lora_txdone_flag && timeout_ms > 0) {
        _delay_ms(1);
        timeout_ms--;
    }
}

/* ---------------- Watchdog ------------------------------------------- */

static void wdt_init_interrupt_8s(void) {
    cli();
    wdt_reset();
    MCUSR &= ~(1 << WDRF);
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
    sei();
}

/* ---------------- Measurement cycle ---------------------------------- */

static void run_measurement_cycle(uint8_t emergency) {
    sensor_rail_on();
    _delay_ms(5);

    int16_t tilt_x = 0, tilt_y = 0, tilt_z = 0;
    uint8_t accel_ok = accel_read_tilt(&tilt_x, &tilt_y, &tilt_z);

    uint16_t raw10 = adc_read(1);
    uint16_t moisture = (uint16_t)(((uint32_t)raw10 * 10000UL) / 1023UL);

    sensor_rail_off();

    /* Status = emergency bit + 7-bit sequence number */
    uint8_t status = (seq_counter & 0x7F);
    if (emergency)
        status |= STATUS_EMERGENCY_BIT;
    if (!accel_ok)
        status |= 0x01;
    seq_counter++;

    uint8_t packet[12];
    packet[0] = (uint8_t)(DEVICE_ID >> 8);
    packet[1] = (uint8_t)(DEVICE_ID & 0xFF);
    packet[2] = (uint8_t)(moisture >> 8);
    packet[3] = (uint8_t)(moisture & 0xFF);
    packet[4] = (uint8_t)((uint16_t)tilt_x >> 8);
    packet[5] = (uint8_t)((uint16_t)tilt_x & 0xFF);
    packet[6] = (uint8_t)((uint16_t)tilt_y >> 8);
    packet[7] = (uint8_t)((uint16_t)tilt_y & 0xFF);
    packet[8] = (uint8_t)((uint16_t)tilt_z >> 8);
    packet[9] = (uint8_t)((uint16_t)tilt_z & 0xFF);
    packet[10] = status;
    packet[11] = crc8_smbus(packet, 11);

    sensor_rail_on();
    _delay_ms(2);
    lora_transmit(packet, sizeof(packet));
    sensor_rail_off();

    /* Debug */
    uart_puts("[NODE] ID=");
    uart_put_u16(DEVICE_ID);
    uart_puts(" SEQ=");
    uart_put_u16(status & 0x7F);
    uart_puts(" X=");
    uart_put_i16(tilt_x);
    uart_puts(" Y=");
    uart_put_i16(tilt_y);
    uart_puts(" Z=");
    uart_put_i16(tilt_z);
    uart_puts(" Moist=");
    uart_put_u16(moisture / 100);
    uart_puts("%");
    if (emergency)
        uart_puts(" EMERGENCY");
    uart_puts(lora_txdone_flag ? " TX-OK\r\n" : " TX-TIMEOUT\r\n");
}

/* ---------------- Sleep ---------------------------------------------- */

static void enter_hibernation(void) {
    ADCSRA &= ~(1 << ADEN);
    SPCR &= ~(1 << SPE);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();
    sleep_cpu();
    sleep_disable();
    ADCSRA |= (1 << ADEN);
    SPCR |= (1 << SPE);
}

/* ---------------- main ----------------------------------------------- */

int main(void) {
    DDRD |= (1 << GATE_BIT);
    sensor_rail_off();

    DDRD &= ~((1 << PD2) | (1 << PD3));
    PORTD &= ~((1 << PD2) | (1 << PD3));

    uart_init();
    spi_master_init();
    adc_init();

    EICRA |= (1 << ISC01) | (1 << ISC00); /* INT0 rising */
    EIMSK |= (1 << INT0);
    EICRA |= (1 << ISC11) | (1 << ISC10); /* INT1 rising */
    EIMSK |= (1 << INT1);

    wdt_init_interrupt_8s();
    sei();

    uart_puts("=== MASTER NODE BOOT ===\r\n");

    for (;;) {
        wdt_tick_count = 0;
        motion_wake_flag = 0;

        while (wdt_tick_count < WAKE_INTERVAL_WDT_TICKS && !motion_wake_flag) {
            enter_hibernation();
        }

        run_measurement_cycle(motion_wake_flag);
    }
}
