/*******************************************************************
 *
 * error_bits.c - Camper firmware error bitmask table
 *
 ******************************************************************/
#include "error_bits.h"

const error_bit_t ERROR_BITS[] = {
    {0x0001, "HOUSEHOLD_SWITCH_FAILED"},
    {0x0002, "VOLTAGE_HOUSEHOLD_LOW"},
    {0x0004, "VOLTAGE_STARTER_LOW"},
    {0x0008, "VOLTAGE_MAINS_LOW"},
    {0x0010, "WATER_LOW"},
    {0x0020, "WASTE_HIGH"},
    {0x0040, "WASTE_FULL"},
    {0x0080, "ADC_STUCK"},
    {0x0100, "PROTOCOL_CRC"},
    {0x0200, "PROTOCOL_OVERRUN"},
    {0x0400, "BROWN_OUT"},
};

const size_t ERROR_BITS_COUNT = sizeof(ERROR_BITS) / sizeof(ERROR_BITS[0]);
