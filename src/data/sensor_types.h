/*******************************************************************
 *
 * sensor_types.h - Type definitions for camper sensors
 *
 ******************************************************************/
#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double battery_voltage;
    double battery_charging_current;
    char charge_state[10];
    double solar_power;
    double yield_today;
} smart_solar_t;

typedef struct {
    double voltage;
    double current;
    int remaining_mins;
    double soc;
    double consumed_ah;
} smart_shunt_t;

typedef struct {
    double temperature;
    double humidity;
    double battery;
} climate_sensor_t;

typedef struct {
    float household_voltage;
    float starter_voltage;
    float mains_voltage;
    bool household_state;
    int water_state; // Percentage 0-100
    int waste_state; // Percentage 0-100
    bool pump_state;
} camper_sensor_t;

/**
 * Structure for entity history data
 */
typedef struct {
    bool is_numeric;
    char entity_name[32];
    char unit[16];
    int count;
    char **timestamps;
    float *min;
    float *max;
    float *mean;
} entity_history_t;

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TYPES_H */
