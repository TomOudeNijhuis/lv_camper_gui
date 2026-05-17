/*******************************************************************
 *
 * data_actions.h - Functions for controlling camper devices
 *
 ******************************************************************/
#ifndef DATA_ACTIONS_H
#define DATA_ACTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Internal function to set camper action
     */
    int set_camper_action_internal(const char* entity_name, int state);

    /**
     * Post a mask-style action body: {"mask": "0xXXXX"}.
     * Used for entities like `errors` whose action body is a bitmask.
     */
    int set_camper_mask_action_internal(const char* entity_name, uint16_t mask);

#ifdef __cplusplus
}
#endif

#endif /* DATA_ACTIONS_H */
