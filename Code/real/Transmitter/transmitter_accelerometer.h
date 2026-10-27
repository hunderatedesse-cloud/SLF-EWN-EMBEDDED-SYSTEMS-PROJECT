#ifndef TRANSMITTER_ACCELEROMETER_H
#define TRANSMITTER_ACCELEROMETER_H

#include <stdint.h>

/*
 * Device: Analog Devices ADXL345 (DEVID 0xE5).
 *
 * Power policy (matches PCB always-on rail for ADXL):
 *   - Always powered from 3.6 V (not VCC_SW) so motion INT works in MCU sleep.
 *   - Between samples: low-power activity mode (low ODR + ACTIVITY → INT1).
 *   - During sample: full MEASURE at higher ODR, then return to low-power.
 *
 * INT1 → MCU PD2 / INT0 (rising edge in transmitter.c).
 *
 * init()              detect DEVID, ±2 g, configure activity INT, enter low-power
 * enter_low_power()   low ODR + LOW_POWER + MEASURE + activity INT armed
 * enter_measure()     higher ODR for a clean axis read
 * read()              signed 16-bit axes; 1 = DEVID OK
 * clear_interrupt()   read INT_SOURCE so INT1 can deassert
 */
void transmitter_accelerometer_init(void);
void transmitter_accelerometer_enter_low_power(void);
void transmitter_accelerometer_enter_measure(void);
void transmitter_accelerometer_clear_interrupt(void);
uint8_t transmitter_accelerometer_read(int16_t *x, int16_t *y, int16_t *z);

#endif
