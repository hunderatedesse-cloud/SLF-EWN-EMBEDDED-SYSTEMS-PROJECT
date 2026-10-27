/* =====================================================================
 * SLF-EWN  --  Tower flash log
 * Device : Winbond W25Q64BV, 64M-bit (8M-byte) SPI NOR flash
 * Bus    : Master SPI Mode 0 (CPOL=0, CPHA=0), one of the two modes the
 *          datasheet allows (Mode 0 or Mode 3); shared with receiver_radio.c
 *          which also runs Mode 1, so this driver re-asserts Mode 0 before
 *          every transaction of its own.
 *
 * NOR-flash rule this file exists to respect: a byte can only be written
 * from 1 to 0. Turning a 0 back into a 1 requires erasing the whole 4KB
 * sector it lives in first (datasheet "Flexible Architecture with 4KB
 * sectors"). That means a counter that gets rewritten in place at a fixed
 * "header" address -- which is how EEPROM-style storage would do this --
 * cannot work here. Instead:
 *   - No header is kept. record_count is recovered at boot by finding the
 *     first still-erased (0xFF) record slot, since every written record's
 *     first byte is always 0x00 (see receiver_storage_append).
 *   - The sector a new record would start is erased just before it is
 *     entered, so appends always land on freshly-erased flash.
 *   - When the chip is full, append automatically Chip-Erases and starts
 *     again from address 0. That way writes walk the whole 8 MB array
 *     instead of hammering one sector (100k erase/write cycles per
 *     sector; filling the chip once is ~524k records / ~6 days at 1 Hz).
 *   - receiver_storage_clear() issues a real Chip Erase instead of
 *     pretending to "reset a counter".
 * ===================================================================== */

#define F_CPU 16000000UL

#include "receiver_storage.h"

#include "../Common/slf_protocol.h"

#include <avr/io.h>
#include <string.h>

#define FLASH_CS_PIN PB1

/* W25Q64BV: 64M-bit / 8M-byte (datasheet General Description / Features). */
#define FLASH_SIZE 0x800000UL
#define FLASH_SECTOR_SIZE 4096UL /* uniform sector erase granularity */
#define FLASH_MAX_RECORDS (FLASH_SIZE / SLF_RECORD_SIZE)

/* ---- Instruction Set Table 1 / Table 2 opcodes actually used here ---- */
#define CMD_WRITE_ENABLE 0x06u
#define CMD_READ_STATUS_1 0x05u
#define CMD_PAGE_PROGRAM 0x02u
#define CMD_SECTOR_ERASE_4K 0x20u
#define CMD_CHIP_ERASE 0xC7u
#define CMD_READ_DATA 0x03u
#define CMD_JEDEC_ID 0x9Fu

#define STATUS_BUSY_BIT (1u << 0) /* S0 in Status Register-1 */

/* JEDEC ID (9Fh): Manufacturer (EFh), Memory Type/Capacity (4017h) for
 * the W25Q64BV, per the Manufacturer and Device Identification table. */
#define JEDEC_MANUFACTURER_WINBOND 0xEFu
#define JEDEC_MEMORY_TYPE_25Q 0x40u
#define JEDEC_CAPACITY_64MBIT 0x17u

static uint32_t record_count;
static uint8_t flash_present;

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

/* Datasheet: SPI Mode 0 or Mode 3 supported; this driver uses Mode 0
 * (CPOL=0, CPHA=0), shared with the rest of the board's SPI bus. */
static void spi_set_mode0(void) {
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
    SPSR = 0;
}

static void select_flash(void) { PORTB &= ~(1 << FLASH_CS_PIN); }
static void deselect_flash(void) { PORTB |= (1 << FLASH_CS_PIN); }

static void send_address(uint32_t address) {
    spi_transfer((uint8_t)(address >> 16));
    spi_transfer((uint8_t)(address >> 8));
    spi_transfer((uint8_t)address);
}

static void flash_read(uint32_t address, uint8_t *data, uint16_t length) {
    spi_set_mode0();
    select_flash();
    spi_transfer(CMD_READ_DATA);
    send_address(address);
    for (uint16_t i = 0; i < length; i++)
        data[i] = spi_transfer(0xFF);
    deselect_flash();
}

/* Polls the real BUSY flag (Status Register-1, S0) instead of guessing a
 * fixed wait: each iteration is itself a full SPI transaction, so no
 * separate timer or delay is needed to pace the polling. */
static void flash_wait_ready(void) {
    uint8_t status;
    do {
        select_flash();
        spi_transfer(CMD_READ_STATUS_1);
        status = spi_transfer(0xFF);
        deselect_flash();
    } while (status & STATUS_BUSY_BIT);
}

static void write_enable(void) {
    spi_set_mode0();
    select_flash();
    spi_transfer(CMD_WRITE_ENABLE);
    deselect_flash();
}

/* Page Program (02h): up to 256 bytes, never crosses a page boundary here
 * because SLF_RECORD_SIZE (16) divides the 256-byte page size evenly and
 * every record address is a multiple of 16. */
static void flash_page_program(uint32_t address, const uint8_t *data,
                                uint8_t length) {
    write_enable();
    select_flash();
    spi_transfer(CMD_PAGE_PROGRAM);
    send_address(address);
    for (uint8_t i = 0; i < length; i++)
        spi_transfer(data[i]);
    deselect_flash();
    flash_wait_ready();
}

static void flash_erase_sector(uint32_t address) {
    write_enable();
    select_flash();
    spi_transfer(CMD_SECTOR_ERASE_4K);
    send_address(address);
    deselect_flash();
    flash_wait_ready();
}

static void flash_chip_erase(void) {
    write_enable();
    select_flash();
    spi_transfer(CMD_CHIP_ERASE);
    deselect_flash();
    flash_wait_ready(); /* tCE: typ 15s, max 30s -- a real, long BUSY wait */
}

static uint8_t flash_read_jedec_ok(void) {
    uint8_t id[3];
    spi_set_mode0();
    select_flash();
    spi_transfer(CMD_JEDEC_ID);
    id[0] = spi_transfer(0xFF);
    id[1] = spi_transfer(0xFF);
    id[2] = spi_transfer(0xFF);
    deselect_flash();
    return id[0] == JEDEC_MANUFACTURER_WINBOND &&
           id[1] == JEDEC_MEMORY_TYPE_25Q && id[2] == JEDEC_CAPACITY_64MBIT;
}

/* A written record's byte 0 is always 0x00 (the top byte of record_count,
 * which never reaches 2^24 for this part's capacity); an unwritten slot
 * reads back as 0xFF (flash erased state). Binary search over that single
 * byte recovers record_count without needing any persisted header. */
static uint32_t scan_record_count(void) {
    uint32_t lo = 0, hi = FLASH_MAX_RECORDS;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        uint8_t marker;
        flash_read(mid * SLF_RECORD_SIZE, &marker, 1);
        if (marker == 0xFFu)
            hi = mid; /* erased: count is here or earlier */
        else
            lo = mid + 1; /* written: count is later */
    }
    return lo;
}

void receiver_storage_init(void) {
    DDRB |= (1 << FLASH_CS_PIN);
    deselect_flash();

    flash_present = flash_read_jedec_ok();
    record_count = flash_present ? scan_record_count() : 0;
}

uint32_t receiver_storage_count(void) { return record_count; }
uint32_t receiver_storage_capacity(void) { return FLASH_MAX_RECORDS; }

void receiver_storage_clear(void) {
    if (!flash_present)
        return;
    flash_chip_erase();
    record_count = 0;
}

void receiver_storage_append(const uint8_t *frame) {
    if (!flash_present)
        return;

    /* Full chip: erase everything and start a new log cycle so wear is
     * spread across all sectors instead of stopping or rewriting one
     * address forever (datasheet: >100,000 erase/write cycles). */
    if (record_count >= FLASH_MAX_RECORDS) {
        flash_chip_erase();
        record_count = 0;
    }

    uint32_t address = record_count * SLF_RECORD_SIZE;

    /* Erase the next sector just before entering it, so every append
     * always lands on freshly-erased flash. */
    if (address % FLASH_SECTOR_SIZE == 0)
        flash_erase_sector(address);

    uint8_t record[SLF_RECORD_SIZE];
    record[0] = (uint8_t)(record_count >> 24); /* always 0x00, see above */
    record[1] = (uint8_t)(record_count >> 16);
    record[2] = (uint8_t)(record_count >> 8);
    record[3] = (uint8_t)record_count;
    memcpy(&record[4], frame, SLF_FRAME_LEN);

    flash_page_program(address, record, sizeof(record));
    record_count++;
}

void receiver_storage_read(uint32_t record_id, uint8_t *record) {
    if (!flash_present || record_id >= record_count)
        return;
    flash_read(record_id * SLF_RECORD_SIZE, record, SLF_RECORD_SIZE);
}