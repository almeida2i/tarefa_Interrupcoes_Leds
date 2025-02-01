#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "ws2812.pio.h"
#include "hardware/pwm.h"

#define NUM_PIXELS 25      // Quantidade de pixels da tarefa
#define WS2812_PIN 7       // GPIO onde a cadeia de pixels está conectada
#define LED_RGB_RED 13     // GPIO onde o LED vermelho  do LED RGB está conectado
#define I 41               // Luminosidade do dígito
#define BOTAO_A 5          // GPIO referente ao Botão A
#define BOTAO_B 6          // GPIO referente ao Botão B

/// Função para envia um pixel para o PIO
static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

// Função para juntar os pixels em uma única word
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

// Mapas para construir dígitos decimais na matriz de LEDs
uint8_t digitos[10][25] = {
    {0,I,I,I,0,
     0,I,0,I,0,
     0,I,0,I,0,
     0,I,0,I,0,
     0,I,I,I,0},
    {0,0,0,I,0,
     0,0,I,I,0,
     0,0,0,I,0,
     0,0,0,I,0,
     0,0,0,I,0},
    {0,I,I,I,0,
     0,0,0,I,0,
     0,I,I,I,0,
     0,I,0,0,0,
     0,I,I,I,0},
    {0,I,I,I,0,
     0,0,0,I,0,
     0,I,I,I,0,
     0,0,0,I,0,
     0,I,I,I,0},
    {0,I,0,I,0,
     0,I,0,I,0,
     0,I,I,I,0,
     0,0,0,I,0,
     0,0,0,I,0},
    {0,I,I,I,0,
     0,I,0,0,0,
     0,I,I,I,0,
     0,0,0,I,0,
     0,I,I,I,0},
    {0,I,I,I,0,
     0,I,0,0,0,
     0,I,I,I,0,
     0,I,0,I,0,
     0,I,I,I,0},
    {0,I,I,I,0,
     0,I,0,I,0,
     0,0,0,I,0,
     0,0,0,I,0,
     0,0,0,I,0},
    {0,I,I,I,0,
     0,I,0,I,0,
     0,I,I,I,0,
     0,I,0,I,0,
     0,I,I,I,0},
    {0,I,I,I,0,
     0,I,0,I,0,
     0,I,I,I,0,
     0,0,0,I,0,
     0,I,I,I,0}
};

// Varável para armazenar a contagem de 0 até 9
volatile uint8_t count = 0;

// Função para mostrar a contagem corrente na matriz de LEDs
void count_show(PIO pio, uint sm, uint8_t count)
{
    for(int i=24; i>19; i--)
        put_pixel(pio,sm,urgb_u32(0,0,digitos[count][i]));
    for(int i=15; i<20; i++)
        put_pixel(pio,sm,urgb_u32(0,0,digitos[count][i]));
    for(int i=14; i>9; i--)
        put_pixel(pio,sm,urgb_u32(0,0,digitos[count][i]));
    for(int i=5; i<10; i++)
        put_pixel(pio,sm,urgb_u32(0,0,digitos[count][i]));
    for(int i=4; i>-1; i--)
        put_pixel(pio,sm,urgb_u32(0,0,digitos[count][i]));
}

// Variáveis para debouncing dos Botões A e B
absolute_time_t next_time_1 = 0;
absolute_time_t next_time_2 = 0;

// Função de callback dos botôes
void gpio_irq_callback(uint gpio, uint32_t event_mask){
    if(gpio==BOTAO_A && absolute_time_diff_us(next_time_1,get_absolute_time())>=0){
        next_time_1 = delayed_by_ms(get_absolute_time(),300);
        count++;
        if(count>9)
            count = 0;
    }

    if(gpio==BOTAO_B && absolute_time_diff_us(next_time_2,get_absolute_time())>=0){
        next_time_2 = delayed_by_ms(get_absolute_time(),300);
        count--;
        if(count>9)
            count = 9;
    }
}

int main() {
    // Iicialização da comunicação serial
    stdio_init_all();

    // Inicialização do PIO
    PIO pio; uint sm; uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
    hard_assert(success);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000.f);

    // Inicializa os Botões A e B
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A,GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B,GPIO_IN);
    gpio_pull_up(BOTAO_B);

    // Inicializa o módulo PWM
    gpio_set_function(LED_RGB_RED, GPIO_FUNC_PWM);
    pwm_set_enabled(pwm_gpio_to_slice_num(LED_RGB_RED), 1);

    // Configuração das interrupções nos botões A e B
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true,gpio_irq_callback);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true,gpio_irq_callback);

    sleep_us(1);

    while (1) {
        // Mostra a contagem corrente na matriz de LEDs
        count_show(pio,sm,count);

        // Pisca o LED RGB vermelho
        pwm_set_gpio_level (LED_RGB_RED, 0x0fff);
        sleep_ms(100);
        pwm_set_gpio_level (LED_RGB_RED, 0);
        sleep_ms(100);
    }

    // This will free resources and unload our program
    pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);

    return 0;
}
