/*******************************************************************
 *
 * error_bits.h - Mapping from camper firmware error bits to names
 *
 ******************************************************************/
#ifndef ERROR_BITS_H
#define ERROR_BITS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint16_t    bit;
        const char* name;
    } error_bit_t;

    extern const error_bit_t ERROR_BITS[];
    extern const size_t      ERROR_BITS_COUNT;

#ifdef __cplusplus
}
#endif

#endif /* ERROR_BITS_H */
