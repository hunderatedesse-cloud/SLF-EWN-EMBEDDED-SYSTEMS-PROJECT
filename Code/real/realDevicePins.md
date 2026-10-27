# Device pinouts

> **As-built:** ADXL345. LIS3DH is reference only (future option, different footprint). Pin references are datasheet names, not board net labels.

The ADXL345 features a 14-lead configuration. The complete list of pins and their descriptions from the technical data sheet is detailed below:

- Pin 1 (VDD I/O): Digital Interface Supply Voltage
- Pin 2 (GND): Must be connected to ground
- Pin 3 (Reserved): Reserved, must be connected to $V_S$ or left open
- Pin 4 (GND): Must be connected to ground
- Pin 5 (GND): Must be connected to ground
- Pin 6 (VS): Supply Voltage
- Pin 7 (CS): Chip Select
- Pin 8 (INT1): Interrupt 1 Output
- Pin 9 (INT2): Interrupt 2 Output
- Pin 10 (GND): Must be connected to ground
- Pin 11 (Reserved): Reserved, must be connected to GND or left open
- Pin 12 (SDO/ALT ADDRESS): Serial Data Out, Alternate I²C Address Select
- Pin 13 (SDA/SDI/SDIO): Serial Data (I²C), Serial Data In (SPI 4-Wire), Serial Data In/Out (SPI 3-Wire)
- Pin 14 (SCL/SCLK): Serial Communications Clock

The LIS3DH features a 16-lead LGA configuration (pp. 1, 9). The complete list of pins and their functions from the technical data sheet is detailed below (p. 9):

- Pin 1 (Vdd_IO): Power supply for I/O pins (p. 9)
- Pin 2 (NC): Not connected (p. 9)
- Pin 3 (NC): Not connected (p. 9)
- Pin 4 (SCL/SPC): I²C serial clock (SCL) / SPI serial port clock (SPC) (p. 9)
- Pin 5 (GND): 0V supply (p. 9)
- Pin 6 (SDA/SDI/SDO): I²C serial data (SDA) / SPI serial data input (SDI) / 3-wire interface serial data output (SDO) (p. 9)
- Pin 7 (SDO/SA0): SPI serial data output (SDO) / I²C less significant bit of the device address (SA0) (p. 9)
- Pin 8 (CS): SPI enable / I²C/SPI mode selection (1: I²C mode; 0: SPI enabled) (p. 9)
- Pin 9 (INT2): Inertial interrupt 2 (p. 9)
- Pin 10 (RES): Connect to GND (p. 9)
- Pin 11 (INT1): Inertial interrupt 1 (p. 9)
- Pin 12 (GND): 0V supply (p. 9)
- Pin 13 (ADC3): Analog to digital converter input 3 (p. 9)
- Pin 14 (Vdd): Power supply (p. 9)
- Pin 15 (ADC2): Analog to digital converter input 2 (p. 9)
- Pin 16 (ADC1): Analog to digital converter input 1 (p. 9)

The W25Q64BV serial flash memory is available in standard 8-pin configurations (SOIC 208-mil, PDIP 300-mil, WSON 8x6-mm) as well as a 16-pin SOIC 300-mil configuration. Both layouts from the technical data sheet are detailed below:

## 8-Pin Packages (SOIC-8, PDIP-8, WSON-8)

- Pin 1 (/CS): Chip Select Input
- Pin 2 (DO / IO1): Data Output (Data Input Output 1 for Dual/Quad SPI)
- Pin 3 (/WP / IO2): Write Protect Input (Data Input Output 2 for Quad SPI)
- Pin 4 (GND): Ground
- Pin 5 (DI / IO0): Data Input (Data Input Output 0 for Dual/Quad SPI)
- Pin 6 (CLK): Serial Clock Input
- Pin 7 (/HOLD / IO3): Hold Input (Data Input Output 3 for Quad SPI)
- Pin 8 (VCC): Power Supply

---

## 16-Pin Package (SOIC-16)

- Pin 1 (/HOLD / IO3): Hold Input (Data Input Output 3 for Quad SPI)
- Pin 2 (VCC): Power Supply
- Pin 3 (N/C): No Connect
- Pin 4 (N/C): No Connect
- Pin 5 (N/C): No Connect
- Pin 6 (N/C): No Connect
- Pin 7 (/CS): Chip Select Input
- Pin 8 (DO / IO1): Data Output (Data Input Output 1 for Dual/Quad SPI)
- Pin 9 (/WP / IO2): Write Protect Input (Data Input Output 2 for Quad SPI)
- Pin 10 (GND): Ground
- Pin 11 (N/C): No Connect
- Pin 12 (N/C): No Connect
- Pin 13 (N/C): No Connect
- Pin 14 (N/C): No Connect
- Pin 15 (DI / IO0): Data Input (Data Input Output 0 for Dual/Quad SPI)
- Pin 16 (CLK): Serial Clock Input

The Ai-Thinker Ra-01 and Ra-02 LoRa modules are built using the same core layout, but they differ slightly on Pins 1 and 2 to accommodate the different antenna configurations (Ra-01 uses a spring/wire antenna pad, while Ra-02 features an onboard IPEX connector).
Here is the complete pinout configuration for both modules from the technical manual:

## Ra-01 Module Pinout

- Pin 1 (ANT): Antenna connection pad
- Pin 2 (GND): Ground
- Pin 3 (3.3V): 3.3V Power Supply
- Pin 4 (RESET): Device Reset
- Pin 5 (DIO0): Digital I/O 0 (Configurable)
- Pin 6 (DIO1): Digital I/O 1 (Configurable)
- Pin 7 (DIO2): Digital I/O 2 (Configurable)
- Pin 8 (DIO3): Digital I/O 3 (Configurable)
- Pin 9 (GND): Ground
- Pin 10 (DIO4): Digital I/O 4 (Configurable)
- Pin 11 (DIO5): Digital I/O 5 (Configurable)
- Pin 12 (SCK): SPI Clock Input
- Pin 13 (MISO): SPI Data Output
- Pin 14 (MOSI): SPI Data Input
- Pin 15 (NSS): SPI Chip Select Input
- Pin 16 (GND): Ground

---

## Ra-02 Module Pinout

- Pin 1 (GND): Ground
- Pin 2 (GND): Ground
- Pin 3 (3.3V): 3.3V Power Supply
- Pin 4 (RESET): Device Reset
- Pin 5 (DIO0): Digital I/O 0 (Configurable)
- Pin 6 (DIO1): Digital I/O 1 (Configurable)
- Pin 7 (DIO2): Digital I/O 2 (Configurable)
- Pin 8 (DIO3): Digital I/O 3 (Configurable)
- Pin 9 (GND): Ground
- Pin 10 (DIO4): Digital I/O 4 (Configurable)
- Pin 11 (DIO5): Digital I/O 5 (Configurable)
- Pin 12 (SCK): SPI Clock Input
- Pin 13 (MISO): SPI Data Output
- Pin 14 (MOSI): SPI Data Input
- Pin 15 (NSS): SPI Chip Select Input
- Pin 16 (GND): Ground
