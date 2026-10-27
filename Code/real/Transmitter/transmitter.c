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

#include "../Common/slf_protocol.h"
#include "transmitter_accelerometer.h"
#include "transmitter_lora.h"

/* Timer0 CTC, /64: 8 MHz / 64 = 125 kHz → 125 counts = 1 ms.
 * Used only for short rail settle waits; claimed for the duration of the
 * wait then stopped so other modules can reuse Timer0. No util/delay. */
#define NODE_TIMER_OCR_1MS 124u

static void node_wait_ms(uint16_t ms) {
    TCCR0A = (1 << WGM01);
    OCR0A = NODE_TIMER_OCR_1MS;
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

/* ---------------- Configuration -------------------------------------- */

#define DEVICE_ID 0x0104u

#define PRODUCTION_INTERVAL_S (15UL * 60UL)
#define WDT_TICK_S 8UL
/*
 * Multi-node anti-collision: stagger wake by (Device ID mod 30) seconds so
 * nodes do not systematically share the same WDT boundary.
 *   T_i = 15 min + (id mod 30) s
 * WDT period is 8 s, so stagger is quantised to whole ticks (floor).
 */
#define TX_STAGGER_S ((uint32_t)(DEVICE_ID) % 30UL)
#define WAKE_INTERVAL_WDT_TICKS                                                           \
    ((PRODUCTION_INTERVAL_S + TX_STAGGER_S) / WDT_TICK_S)

/* Proteus only: force ~8 s wakes. Must be undefined / removed for field builds (F1). */
#define SIM_FAST_WAKE 1
#ifdef SIM_FAST_WAKE
#undef WAKE_INTERVAL_WDT_TICKS
#define WAKE_INTERVAL_WDT_TICKS SIM_FAST_WAKE
#endif

/* ---------------- Pins (see EasyEDA Hub TX netlist) ------------------ */
/*
 * Always-on 3.6 V: MCU, ADXL345, supercap across cell.
 * VCC_SW (PD4 low = on): moisture + Ra-02 only.
 * PD2 INT0 = ADXL345 INT1 activity · PD3 INT1 = Ra-02 DIO0 TxDone
 */
#define NSS_LORA_BIT PB1
#define CS_ACCEL_BIT PB2
#define GATE_BIT PD4

static inline void sensor_rail_on(void) { PORTD &= ~(1 << GATE_BIT); }
static inline void sensor_rail_off(void) { PORTD |= (1 << GATE_BIT); }

/* ---------------- Globals -------------------------------------------- */

static volatile uint16_t wdt_tick_count = 0;
static volatile uint8_t motion_wake_flag = 0;
static uint8_t seq_counter = 0; /* sequence number */

ISR(WDT_vect) { wdt_tick_count++; }
ISR(INT0_vect) { motion_wake_flag = 1; }

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
    PORTB |= (1 << NSS_LORA_BIT) | (1 << CS_ACCEL_BIT);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0); /* Mode 0, /16 */
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
    /* Accel is always-on; raise ODR for a clean sample, clear any ACTIVITY INT */
    transmitter_accelerometer_enter_measure();
    transmitter_accelerometer_clear_interrupt();

    sensor_rail_on();
    node_wait_ms(5); /* rail settle before SPI / ADC (moisture + LoRa) */

    int16_t tilt_x = 0, tilt_y = 0, tilt_z = 0;
    uint8_t accel_ok = transmitter_accelerometer_read(&tilt_x, &tilt_y, &tilt_z);

    uint16_t raw10 = adc_read(1);
    uint16_t moisture = (uint16_t)(((uint32_t)raw10 * 10000UL) / 1023UL);

    sensor_rail_off();

    /* Status = emergency bit + 7-bit sequence number */
    uint8_t status = (seq_counter & 0x7F);
    if (emergency)
        status |= SLF_STATUS_EMERGENCY;
    if (!accel_ok)
        status |= SLF_STATUS_ACCEL_ERROR;
    seq_counter++;

    uint8_t packet[SLF_FRAME_LEN];
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
    packet[11] = slf_crc8(packet, 11);

    sensor_rail_on();
    node_wait_ms(2); /* rail settle before LoRa TX */
    uint8_t tx_ok = transmitter_lora_send(packet, sizeof(packet));
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
    uart_puts(tx_ok ? " TX-OK\r\n" : " TX-TIMEOUT\r\n");

    /* Back to low-power activity mode before the next hibernation window */
    transmitter_accelerometer_enter_low_power();
    transmitter_accelerometer_clear_interrupt();
}

/* ---------------- Sleep ---------------------------------------------- */

static void enter_hibernation(void) {
    /* ADXL stays powered and in low-power MEASURE+ACTIVITY; INT0 can wake us */
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
    transmitter_accelerometer_init();
    transmitter_lora_init();

    EICRA |= (1 << ISC01) | (1 << ISC00); /* INT0 rising */
    EIMSK |= (1 << INT0);
    EICRA |= (1 << ISC11) | (1 << ISC10); /* INT1 rising */
    EIMSK |= (1 << INT1);

    wdt_init_interrupt_8s();
    sei();

    uart_puts("=== MASTER NODE BOOT ===\r\n");
    uart_puts("ID=");
    uart_put_u16(DEVICE_ID);
    uart_puts(" STAGGER_S=");
    uart_put_u16((uint16_t)TX_STAGGER_S);
    uart_puts(" WDT_TICKS=");
    uart_put_u16((uint16_t)WAKE_INTERVAL_WDT_TICKS);
    uart_puts("\r\n");

    for (;;) {
        wdt_tick_count = 0;
        motion_wake_flag = 0;

        while (wdt_tick_count < WAKE_INTERVAL_WDT_TICKS && !motion_wake_flag) {
            enter_hibernation();
        }

        run_measurement_cycle(motion_wake_flag);
    }
}
