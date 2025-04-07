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
    int charge_state;
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

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TYPES_H */
