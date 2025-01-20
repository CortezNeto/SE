#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define ALERTA 21
int star_wars_notes[] = {
    220, 220, 220, 174, 261, 220, 174, 261, 220,
    329, 329, 329, 349, 261, 207, 174, 261, 220,
    440, 220, 220, 440, 415, 392, 370, 329, 349,
    233, 311, 293, 277, 261, 247, 261

};

int note_duration[] = {
    500, 500, 500, 333, 166, 500, 333, 166, 500,
    500, 500, 500, 333, 166, 500, 333, 166, 500,
    500, 333, 166, 500, 333, 166, 100, 100, 100,
    166, 500, 333, 166, 100, 100, 100
};

void pwm_init_buzzer(uint pin, int FREQ_ALERTA) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (FREQ_ALERTA * 28672)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

void beep(uint pin, float duration_ms) {
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}

int main() {
    gpio_init(ALERTA);
    gpio_set_dir(ALERTA, GPIO_OUT);

    for (int i =0; i <= 33; i++) {
        if(i > 1 && i % 8 == 0) {
            sleep_ms(10);
        }
        pwm_init_buzzer(ALERTA, star_wars_notes[i]);
        beep(ALERTA, note_duration[i]);
    }

    return 0;
}
