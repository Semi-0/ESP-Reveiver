#include "pin_controller.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

std::set<int> PinController::configured_pins;

void PinController::configure_pin_if_needed(int pin, gpio_mode_t mode) {
    if (configured_pins.find(pin) == configured_pins.end()) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = mode;
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        
        configured_pins.insert(pin);
        ESP_LOGI(APP_TAG, "Configured pin %d as %s", pin, mode == GPIO_MODE_OUTPUT ? "OUTPUT" : "INPUT");
        
        // Small delay after first configuration to ensure pin is ready
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void PinController::digital_write(int pin, bool high) {
    configure_pin_if_needed(pin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)pin, high ? 1 : 0);
    ESP_LOGI(APP_TAG, "Digital write: pin %d = %s", pin, high ? "HIGH" : "LOW");
}

void PinController::analog_write(int pin, int value) {
    configure_pin_if_needed(pin, GPIO_MODE_OUTPUT);
    
    // Use LEDC for PWM (analog write)
    setup_pwm_timer(LEDC_TIMER_0, 5000);
    setup_pwm_channel(LEDC_CHANNEL_0, LEDC_TIMER_0, (gpio_num_t)pin, (uint32_t)value);
    
    ESP_LOGI(APP_TAG, "Analog write: pin %d = %d", pin, value);
}

void PinController::motor_control(int speed_pin, int in1_pin, int in2_pin, int speed_value) {
    configure_pin_if_needed(speed_pin, GPIO_MODE_OUTPUT);
    configure_pin_if_needed(in1_pin, GPIO_MODE_OUTPUT);
    configure_pin_if_needed(in2_pin, GPIO_MODE_OUTPUT);
    
    // Set direction
    digital_write(in1_pin, true);
    digital_write(in2_pin, false);
    
    // Set speed using PWM
    setup_pwm_timer(LEDC_TIMER_1, 5000);
    setup_pwm_channel(LEDC_CHANNEL_1, LEDC_TIMER_1, (gpio_num_t)speed_pin, (uint32_t)speed_value);
    
    ESP_LOGI(APP_TAG, "Motor control: speed=%d, in1=%d, in2=%d, speed_val=%d", 
             speed_pin, in1_pin, in2_pin, speed_value);
}

size_t PinController::get_configured_pins_count() {
    return configured_pins.size();
}

void PinController::setup_pwm_timer(ledc_timer_t timer_num, uint32_t freq_hz) {
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
    ledc_timer.timer_num = timer_num;
    ledc_timer.freq_hz = freq_hz;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&ledc_timer);
}

void PinController::setup_pwm_channel(ledc_channel_t channel, ledc_timer_t timer_num, 
                                     gpio_num_t gpio_num, uint32_t duty) {
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.gpio_num = gpio_num;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel = channel;
    ledc_channel.timer_sel = timer_num;
    ledc_channel.duty = duty;
    ledc_channel.hpoint = 0;
    ledc_channel_config(&ledc_channel);
}
