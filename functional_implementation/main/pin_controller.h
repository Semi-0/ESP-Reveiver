#ifndef PIN_CONTROLLER_H
#define PIN_CONTROLLER_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include <set>

class PinController {
public:
    static void configure_pin_if_needed(int pin, gpio_mode_t mode);
    static void digital_write(int pin, bool high);
    static void analog_write(int pin, int value);
    static void motor_control(int speed_pin, int in1_pin, int in2_pin, int speed_value);
    static size_t get_configured_pins_count();

private:
    static std::set<int> configured_pins;
    static void setup_pwm_timer(ledc_timer_t timer_num, uint32_t freq_hz);
    static void setup_pwm_channel(ledc_channel_t channel, ledc_timer_t timer_num, 
                                 gpio_num_t gpio_num, uint32_t duty);
};

#endif // PIN_CONTROLLER_H
