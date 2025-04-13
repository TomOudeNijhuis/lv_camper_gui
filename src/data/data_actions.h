/*******************************************************************
 *
 * data_actions.h - Functions for controlling camper devices
 *
 ******************************************************************/
#ifndef DATA_ACTIONS_H
#define DATA_ACTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Internal function to set camper action
     */
    int set_camper_action_internal(const char* entity_name, const char* status);

#ifdef __cplusplus
}
#endif

#endif /* DATA_ACTIONS_H */
