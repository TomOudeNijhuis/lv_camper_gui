/*******************************************************************
 *
 * camper_data.h - User interface definitions for the Camper GUI
 *
 ******************************************************************/
#ifndef CAMPER_DATA_H
#define CAMPER_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Data structures for each sensor type
typedef struct {
    float battery_charging_current;
    float battery_voltage;
    int charge_state;
    float solar_power;
    float yield_today;
} smart_solar_t;

typedef struct {
    float voltage;
    float current;
    int remaining_mins;
    float soc;  // State of Charge (%)
    float consumed_ah;
} smart_shunt_t;

typedef struct {
    float battery;
    float temperature;
    float humidity;
} climate_sensor_t;

typedef struct {
    float household_voltage;
    float starter_voltage;
    float mains_voltage;
    bool household_state;
    int water_state;
    int waste_state;
    bool pump_state;
} camper_sensor_t;

// Fetch data from the server
int fetch_camper_data(void);
camper_sensor_t* get_camper_data(void);
int set_camper_action(const int entity_id, const char *status);

// Getter functions to access the data
smart_solar_t* get_smart_solar_data(void);
smart_shunt_t* get_smart_shunt_data(void);
climate_sensor_t* get_inside_climate_data(void);
climate_sensor_t* get_outside_climate_data(void);
camper_sensor_t* get_camper_data(void);


#ifdef __cplusplus
}
#endif

#endif /* CAMPER_DATA_H */