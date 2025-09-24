#ifndef BACN_GPIO_H
#define BACN_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#define PWR_MODULE 4
#define ANTENNA_SEL 25
#define STATUS 18
#define RF1 1
#define RF2 0

uint8_t status_LTE(void);

uint8_t power_ON_LTE(void);
uint8_t power_OFF_LTE(void);
uint8_t reset_LTE(void);

uint8_t switch_ANTENNA(bool RF);

uint8_t real_time(void);

#endif // BACN_GPIO_H