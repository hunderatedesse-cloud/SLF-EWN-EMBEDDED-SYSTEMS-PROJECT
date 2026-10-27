/* =====================================================================
 * SLF-EWN  --  Buried Sensor Node accelerometer driver
 * Device : Analog Devices ADXL345, 3-axis SPI accelerometer
 *
 * Always-on 3.6 V rail (not FET-gated) so ACTIVITY interrupt can wake
 * the MCU during the long hibernation window.
 *
 * Modes:
 *   Low-power wait  — MEASURE + LOW_POWER + low ODR + ACTIVITY on INT1
 *   Sample          — MEASURE + higher ODR, read DATAX0..DATAZ1
 *
 * Bus: shared SPI with Ra-02. ADXL345 uses Mode 3; restore Mode 0 after.
 * ===================================================================== */

#define F_CPU 8000000UL

#include "transmitter_accelerometer.h"

#include <avr/io.h>

#define ACCEL_CS_PIN PB2

/* ---- ADXL345 register map (datasheet) -------------------------------- */
#define ADXL345_REG_DEVID 0x00u
#define ADXL345_REG_THRESH_ACT 0x24u
#define ADXL345_REG_ACT_INACT_CTL 0x27u
#define ADXL345_REG_BW_RATE 0x2Cu
#define ADXL345_REG_POWER_CTL 0x2Du
#define ADXL345_REG_INT_ENABLE 0x2Eu
#define ADXL345_REG_INT_MAP 0x2Fu
#define ADXL345_REG_INT_SOURCE 0x30u
#define ADXL345_REG_DATA_FORMAT 0x31u
#define ADXL345_REG_DATAX0 0x32u

#define ADXL345_DEVID_VALUE 0xE5u

/* POWER_CTL */
#define ADXL345_POWER_CTL_MEASURE (1u << 3)
#define ADXL345_POWER_CTL_SLEEP (1u << 2)
#define ADXL345_POWER_CTL_AUTO_SLEEP (1u << 4)

/* BW_RATE: bit4 LOW_POWER, bits3:0 rate code */
#define ADXL345_BW_RATE_LOW_POWER (1u << 4)
#define ADXL345_RATE_100HZ 0x0Au
#define ADXL345_RATE_25HZ 0x08u
#define ADXL345_RATE_12_5HZ 0x07u
#define ADXL345_RATE_6_25HZ 0x06u

#define ADXL345_DATA_FORMAT_FULL_RES_2G 0x08u

/* INT bits (INT_ENABLE / INT_MAP / INT_SOURCE) */
#define ADXL345_INT_ACTIVITY (1u << 4)

/* ACT_INACT_CTL: enable activity on X,Y,Z (dc-coupled) */
#define ADXL345_ACT_EN_XYZ 0x70u

/*
 * THRESH_ACT scale: 62.5 mg/LSB.
 * 0x20 ≈ 2.0 g — sudden jolt / slide class motion.
 * Tune on hardware (open issue F8).
 */
#define ADXL345_THRESH_ACT_DEFAULT 0x20u

#define ADXL345_CMD_READ 0x80u
#define ADXL345_CMD_MULTIBYTE 0x40u

/* Turn-on settling after MEASURE at higher ODR */
#define ADXL345_TURN_ON_MS 12
#define ACCEL_TIMER_OCR_1MS 124u

static uint8_t accel_present = 0;

static void select_accelerometer(void) { PORTB &= ~(1 << ACCEL_CS_PIN); }
static void deselect_accelerometer(void) { PORTB |= (1 << ACCEL_CS_PIN); }

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

static void spi_set_mode3(void) {
    SPCR |= (1 << CPOL) | (1 << CPHA);
}

static void spi_restore_mode0(void) {
    SPCR &= ~((1 << CPOL) | (1 << CPHA));
}

static void accel_wait_ms(uint16_t ms) {
    TCCR0A = (1 << WGM01);
    OCR0A = ACCEL_TIMER_OCR_1MS;
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

static uint8_t read_register(uint8_t reg) {
    uint8_t value;
    spi_set_mode3();
    select_accelerometer();
    spi_transfer(ADXL345_CMD_READ | reg);
    value = spi_transfer(0x00);
    deselect_accelerometer();
    spi_restore_mode0();
    return value;
}

static void write_register(uint8_t reg, uint8_t value) {
    spi_set_mode3();
    select_accelerometer();
    spi_transfer(reg);
    spi_transfer(value);
    deselect_accelerometer();
    spi_restore_mode0();
}

static void read_axes_burst(uint8_t out[6]) {
    spi_set_mode3();
    select_accelerometer();
    spi_transfer(ADXL345_CMD_READ | ADXL345_CMD_MULTIBYTE | ADXL345_REG_DATAX0);
    for (uint8_t i = 0; i < 6; i++)
        out[i] = spi_transfer(0x00);
    deselect_accelerometer();
    spi_restore_mode0();
}

static void configure_activity_interrupt(void) {
    /* Threshold and axes for ACTIVITY */
    write_register(ADXL345_REG_THRESH_ACT, ADXL345_THRESH_ACT_DEFAULT);
    write_register(ADXL345_REG_ACT_INACT_CTL, ADXL345_ACT_EN_XYZ);

    /* ACTIVITY → INT1 (map bit 0 = INT1), enable ACTIVITY */
    write_register(ADXL345_REG_INT_MAP, 0x00u);
    write_register(ADXL345_REG_INT_ENABLE, ADXL345_INT_ACTIVITY);

    /* Clear any pending interrupt status */
    (void)read_register(ADXL345_REG_INT_SOURCE);
}

void transmitter_accelerometer_enter_low_power(void) {
    if (!accel_present)
        return;

    /*
     * Low-power wait mode:
     *   - LOW_POWER bit reduces current vs normal ODR mode
     *   - 12.5 Hz is enough for coarse activity detection
     *   - MEASURE on so ACTIVITY can fire INT1
     *   - Do not use full SLEEP bit alone: activity detection needs measure path
     */
    write_register(ADXL345_REG_BW_RATE,
                   (uint8_t)(ADXL345_BW_RATE_LOW_POWER | ADXL345_RATE_12_5HZ));
    write_register(ADXL345_REG_POWER_CTL, ADXL345_POWER_CTL_MEASURE);
    configure_activity_interrupt();
}

void transmitter_accelerometer_enter_measure(void) {
    if (!accel_present)
        return;

    /* Higher ODR, normal (not LOW_POWER) for a stable sample */
    write_register(ADXL345_REG_BW_RATE, ADXL345_RATE_100HZ);
    write_register(ADXL345_REG_POWER_CTL, ADXL345_POWER_CTL_MEASURE);
    accel_wait_ms(ADXL345_TURN_ON_MS);
}

void transmitter_accelerometer_clear_interrupt(void) {
    if (!accel_present)
        return;
    (void)read_register(ADXL345_REG_INT_SOURCE);
}

void transmitter_accelerometer_init(void) {
    DDRB |= (1 << ACCEL_CS_PIN);
    deselect_accelerometer();

    accel_present = (read_register(ADXL345_REG_DEVID) == ADXL345_DEVID_VALUE);
    if (!accel_present)
        return;

    write_register(ADXL345_REG_DATA_FORMAT, ADXL345_DATA_FORMAT_FULL_RES_2G);

    /* Start in low-power activity mode (hibernate default) */
    transmitter_accelerometer_enter_low_power();
}

uint8_t transmitter_accelerometer_read(int16_t *x, int16_t *y, int16_t *z) {
    if (read_register(ADXL345_REG_DEVID) != ADXL345_DEVID_VALUE) {
        accel_present = 0;
        *x = 0;
        *y = 0;
        *z = 0;
        return 0;
    }
    accel_present = 1;

    uint8_t raw[6];
    read_axes_burst(raw);

    /* Little-endian 16-bit, full-resolution ±2 g (~3.9 mg/LSB). */
    *x = (int16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
    *y = (int16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
    *z = (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));

    return 1;
}
