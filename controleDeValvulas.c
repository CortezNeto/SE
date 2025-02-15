//inclusão de bibliotecas utilizadas
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "ws2818b.pio.h"
#include "math.h"
#include "blink.pio.h"


//definição de constantes
#define LED_COUNT 25
#define LED_PIN 7
#define ALERTA 21
const float volumeTotalReservatorio = 20.0;

//definição de pinos para UART
#define UART_TX_PIN 4
#define UART_RX_PIN 5

//definição de pinos para protocolo I2C
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

//definição de constante: ID e taxa de transferência para UART
#define UART_ID uart1
#define BAUD_RATE 115200

#define WIFI_SSID "Vanvan"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "12345678" // Substitua pela senha da sua rede Wi-Fi

float volumeAtualReservatorio = 0.0; //variável para controle do nível atual do reservatório

//declaração de funções
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npInit(uint pin);
void npWrite();
void npClear();

// Buffer para resposta HTTP
char http_response[1024];

// Função para criar a resposta HTTP
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n" //corpo do documento .html para página web
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "  <meta charset=\"UTF-8\">"
             "  <title>Controle do LED e Botões</title>"
             "</head>"
             "<body>"
             "  <h1>Controle do LED e Botões</h1>"
             "  <p><a href=\"/led/f\">Filtração</a></p>"
             "  <p><a href=\"/led/d\">Descarga de Fundo</a></p>"
             "  <p><a href=\"/led/r\">Retrolavagem</a></p>"
             "  <p><a href=\"/led/off\">Desligar LED</a></p>"
             "  <p><a href=\"/update\">Atualizar Estado</a></p>"
             "  <h2>Estado dos Botões:</h2>"
             "  <p>Botão 1: %s</p>"
             "  <p>Botão 2: %s</p>"
             "</body>"
             "</html>\r\n",
             button1_message, button2_message);
}

int estado = -1; //variável para controlar em qual estado está a operação do sistema

//Aqui, inicia-se a configuração da matriz de LED
//Struct para definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};

typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

//Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT]; //como mostrado nas constantes, LED_COUNT tem valor 25 porque há 25 leds na matriz. É criado um vetor de tamanho 25 e atributos R, G e B

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

void npInit(uint pin) {

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); //procura máquina livre
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Limpa buffer de pixels, evitando 'garbage'
  for (uint i = 0; i < LED_COUNT; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

//a variável index é o número correspondente do led na matriz de LED.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r; //a variável r corresponde ao pixel vermelho e é atribuída ao atributo R do vetor no referido índice
  leds[index].G = g; //a variável g corresponde ao pixel verde e é atribuída ao atributo G do vetor no referido índice
  leds[index].B = b; //a variável b corresponde ao pixel azul e é atribuída ao atributo B do vetor no referido índice
}

//essa função define os atributos R, G e B dos leds como 0, desligando-os 
void npClear() {
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  //Após cada chamada da função npSetLED() é necessário chamar a função npWrite() para que o estado atual da matriz seja atualizado
  for (uint i = 0; i < LED_COUNT; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(300);
}

// Função de callback para processar requisições HTTP, funciona como as interrupções para economizar hardware estudadas ao longo do curso
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)p->payload;
    
    if (strstr(request, "GET /led/f")) { //se a requisição for /led/f, é o estado de filtração
        npClear(); //limpa a matriz de leds
        npSetLED(0,0,0,40); //liga o led 0 no tom azul
        npSetLED(2,0,0,40); //liga o led 2 no tom azul
        npSetLED(4,0,0,40); //liga o led 4 no tom azul
        estado = 1; //estado recebe 1
        sleep_ms(100); //sleep de 0.1 segundo
        npWrite(); //atualiza o estado da matriz de led para exibir os tons azuis e os demais apagados.
    } else if (strstr(request, "GET /led/d")) { //se a requisição for /led/d, é o estado de descarga de fundo
        npClear(); //limpa a matriz de leds
        npSetLED(1,0,0,40); //liga o led 1 no tom azul
        npSetLED(3,0,0,40); //liga o led 3 no tom azul
        estado = 2; //estado recebe 2
        sleep_ms(100); //sleep de 0.1 segundo
        npWrite(); //atualiza o estado da matriz de led para exibir os tons azuis e os demais apagados
    } else if (strstr(request, "GET /led/r")) { //se a requisição for /led/r, é o estado de retrolavagem
        npClear(); //limpa a matriz de leds
        npSetLED(5,0,0,40); //liga o led 5 no tom azul
        npSetLED(6,0,0,40); //liga o led 6 no tom azul
        estado = 3; //estado recebe 3
        sleep_ms(100); //sleep de 0.1 segundo
        npWrite(); //atualiza o estado da matriz de led para exibir os tons azuis e os demais apagados
    } else if (strstr(request, "GET /led/off")) { //se a requisição for /led/off, as válvulas são desativadas
        npClear(); //limpa a matriz de leds
        npSetLED(0, 40, 0, 0); //liga o led 0 no tom vermelho
        npSetLED(1, 40, 0, 0); //liga o led 1 no tom vermelho
        npSetLED(2, 40, 0, 0); //liga o led 2 no tom vermelho
        npSetLED(3, 40, 0, 0); //liga o led 3 no tom vermelho
        npSetLED(4, 40, 0, 0); //liga o led 4 no tom vermelho
        npSetLED(5, 40, 0, 0); //liga o led 5 no tom vermelho
        npSetLED(6, 40, 0, 0); //liga o led 6 no tom vermelho
        estado = 0; //estado recebe 0
        npWrite(); //atualiza a matriz de LED
    }

    // Atualiza o conteúdo da página com base no estado dos botões
    create_http_response();

    // Envia a resposta HTTP
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}

//Aqui se inicia a configuração do buzzer
void pwm_init_buzzer(uint pin, int FREQ_ALERTA) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (FREQ_ALERTA * 28672)); //Divisor de clock
    pwm_init(slice_num, &config, true); //inicia ao slice associado

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
    sleep_ms(1); // Pausa de 100ms
}

//sirene simulada por frequência em valores senoidais
void tocarSirene() {
    float seno = 0; //variável para obter o valor do seno em dado momento
    int freq = 0; //frequencia 

    for(int i = 0; i < 180; i++) { //loop variando de 0 a 180 para simular os valores de seno
        seno = sin(i*3.1416/180); //conversão de graus para radianos
        freq = 1000 + ((int)(seno*1000)); //soma 1000 com o valor obtido do seno (no máximo 1, no mínimo 0) multiplicado por 1000, ou seja, varia de 1000 a 2000

        pwm_init_buzzer(ALERTA, freq); //alerta é a porta do buzzer e frequência é o valor da frequência da nota naquele momento
        beep(ALERTA, 5); //emitir por 5 ms

    }
}

int main() {
    stdio_init_all();  // Inicializa a saída padrão
    
    //inicialização do protocolo I2C
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    //inicialização do buzzer como porta de saída
    gpio_init(ALERTA);
    gpio_set_dir(ALERTA, GPIO_OUT);

    //inicialização do display OLED
    ssd1306_init();

    //inicialização da matriz de LEDs
    npInit(LED_PIN);
    npClear();    

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    restart:

    sleep_ms(2000); //delay de 2 segundos
    printf("Iniciando servidor HTTP\n"); //comunicação serial para controle

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n"); //comunicação serial para controle
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n"); //comunicação serial para controle

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n"); //comunicação serial para controle
        return 1;
    }else {
        printf("Connected.\n");
        //O endereço de IP é passado de forma humanamente legível
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);

        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]); //comunicação serial para controle
        
        //variáveis auxíliares para exibir o endereço de IP
        char primeiraParte[20];
        char segundaParte[2];
        char terceiraParte[2];
        char quartaParte[2];
        char* ponto = ".";

        itoa(ip_address[0], primeiraParte, 10); //captura os 3 primeiros dígitos do endereço de IP da Pi Pico W e transforma em string
        itoa(ip_address[1], segundaParte, 10); //captura os 3 primeiros dígitos do endereço de IP da Pi Pico W após o primeiro "." e transforma em string
        itoa(ip_address[2], terceiraParte, 10); //captura os 3 primeiros dígitos do endereço de IP da Pi Pico W após o segundo "." e transforma em string
        itoa(ip_address[3], quartaParte, 10); //captura os 3 últimos dígitos do endereço de IP da Pi Pico W e transforma em string

        
        strcat(primeiraParte, "."); //concatena a primeira seção do IP com um ponto
        strcat(primeiraParte, segundaParte); //concatena os 3 primeiros dígitos do IP com a segunda seção do endereço
        strcat(primeiraParte, "."); //concatena mais um ponto ao endereço
        strcat(primeiraParte, terceiraParte); //concatena a terceira seção do endereço de IP
        strcat(primeiraParte, "."); //concatena mais um ponto
        strcat(primeiraParte, quartaParte); //concatena os 3 últimos dígitos do IP       

        //variáveis de posição para escrita no display OLED
        int y = 0; 
        int x = 0;

        ssd1306_draw_string(ssd, x, y, "Endereco de IP:"); //texto mostrado na primeira linha do display OLED
        y+=16; //soma 16 para exibir a próxima linha abaixo da anterior
        ssd1306_draw_string(ssd, x, y, primeiraParte); //exibe o endereço de IP
        render_on_display(ssd, &frame_area); //renderiza o display
        y+=8; //incrementa a variável para que outro texto venha ser mostrado abaixo    
    }   

    printf("Wi-Fi conectado!\n"); //print para controle na porta serial    

    // Inicia o servidor HTTP
    start_http_server();

    //todos os leds da matriz que serão utilizados no sistema são definidos como VERMELHO
    npSetLED(0, 40, 0, 0);
    npSetLED(1, 40, 0, 0);
    npSetLED(2, 40, 0, 0);
    npSetLED(3, 40, 0, 0);
    npSetLED(4, 40, 0, 0);
    npSetLED(5, 40, 0, 0);
    npSetLED(6, 40, 0, 0);

    int volumePercentual = 0; //variável para controle do nível do reservatório
    char volumePercentualStr[5]; //vetor para exibir o nível do reservatório no display

    npWrite(); //atualiza a matriz de led
    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
        
        if(estado == 1) { //se for o estado de filtração
            volumeAtualReservatorio += 1.0; //incrementa 1 ao nível atual
            volumePercentual = ((volumeAtualReservatorio/volumeTotalReservatorio)*100); //amostragem percentual
            itoa(volumePercentual, volumePercentualStr, 10); //converte de inteiro para string

            printf("%d\n", volumePercentual); //impressão serial do volume percentual do reservatório

            //limpa o display
            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            render_on_display(ssd, &frame_area);

            //exibe o nível atual do reservatório
            ssd1306_draw_string(ssd, 0, 0, "Nivel do");
            ssd1306_draw_string(ssd, 0, 16, "Reservatorio");
            ssd1306_draw_string(ssd, 0, 32, volumePercentualStr);
            render_on_display(ssd, &frame_area);

            sleep_ms(500); //delay de 0.5 segundo

            if (volumePercentual > 75.0) { //se o volume percentual ultrapassar os 75%
                printf("Entoru volume \n"); //controle de fluxo serial

                //limpa o display
                uint8_t ssd[ssd1306_buffer_length];
                memset(ssd, 0, ssd1306_buffer_length);
                render_on_display(ssd, &frame_area);
                
                //leds são definidos como vermelho
                npSetLED(0, 40, 0, 0);
                npSetLED(1, 40, 0, 0);
                npSetLED(2, 40, 0, 0);
                npSetLED(3, 40, 0, 0);
                npSetLED(4, 40, 0, 0);
                npSetLED(5, 40, 0, 0);
                npSetLED(6, 40, 0, 0);

                while(estado == 1) {
                    printf("Entoru volume loop\n"); //controle de fluxo na serial
                    
                    //exibe mensagem de alerta no display
                    ssd1306_draw_string(ssd, 0, 0, "Alerta");
                    ssd1306_draw_string(ssd, 0, 16, "Nivel alto");
                    render_on_display(ssd, &frame_area);
                    sleep_ms(300); //delay de 0.3 segundo

                    npWrite(); //atualiza a matriz de LED
                    tocarSirene(); //emite alerta sonoro
                }
            }
        } else if(estado == 2) { //se o estado for o de descarga de fluxo

            //limpa o display
            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            render_on_display(ssd, &frame_area);

            //exibe a mensagem de realização da descarga
            ssd1306_draw_string(ssd, 0, 0, "Realizando");
            ssd1306_draw_string(ssd, 0, 16, "Descarga");
            ssd1306_draw_string(ssd, 0, 32, volumePercentualStr);
            render_on_display(ssd, &frame_area);            

            //temporizador de 10 segundos definido
            int temporizador = 10;
            uint32_t interval = 1000; //intervalo de 1 segundo definido
            absolute_time_t next_wake_time = delayed_by_us(get_absolute_time(), interval * 1000); //controle do tempo futuro a cada 1 segundo

            while (temporizador >= 0) { //enquanto o temporizador for maior ou igual a 0
                if (time_reached(next_wake_time)) { //se o tempo futuro foi alcançado
                    char temporizadorStr[3]; //variável auxiliar para exibir mensagem no display

                    //limpa o display
                    uint8_t ssd[ssd1306_buffer_length];
                    memset(ssd, 0, ssd1306_buffer_length);
                    render_on_display(ssd, &frame_area);

                    //exibe a mensagem de realização da descarga e uma contagem regressiva
                    ssd1306_draw_string(ssd, 0, 0, "Descarga");
                    ssd1306_draw_string(ssd, 0, 16, "Faltam");
                    itoa(temporizador, temporizadorStr, 10); //converte o tempo para string
                    ssd1306_draw_string(ssd, 0, 32, temporizadorStr); //exibe o tempo que falta
                    ssd1306_draw_string(ssd, 0, 48, "segundos");
                    render_on_display(ssd, &frame_area);

                    next_wake_time = delayed_by_us(next_wake_time, interval * 1000); //o próximo tempo a ser atingido será o atual + 1 segundo
                    temporizador -= 1; //a contagem regressiva é decrementada
                }
                sleep_ms(1); //delay de 1 ms
            }
            estado = 0; //estado recebe 0

            sleep_ms(500); //delay de 0.5 segundo

        } else if(estado == 3) { //se o estado for o de retrolavagem
            
            //limpa o display
            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            render_on_display(ssd, &frame_area);
            
            //exibe a mensagem de realização da descarga
            ssd1306_draw_string(ssd, 0, 0, "Realizando");
            ssd1306_draw_string(ssd, 0, 16, "Retrolavagem");
            ssd1306_draw_string(ssd, 0, 32, volumePercentualStr);
            render_on_display(ssd, &frame_area);
            
            //temporizador de 10 segundos definido
            int temporizador = 10;
            uint32_t interval = 1000; //intervalo de 1 segundo definido
            absolute_time_t next_wake_time = delayed_by_us(get_absolute_time(), interval * 1000); //controle do tempo futuro a cada 1 segundo

            while (temporizador >= 0) { //enquanto o temporizador for maior ou igual a 0
                if (time_reached(next_wake_time)) { //se o tempo futuro foi alcançado
                    char temporizadorStr[3]; //variável auxiliar para exibir mensagem no display

                    //limpa o display
                    uint8_t ssd[ssd1306_buffer_length];
                    memset(ssd, 0, ssd1306_buffer_length);
                    render_on_display(ssd, &frame_area);

                    //exibe a mensagem de realização da retrolavagem e uma contagem regressiva
                    ssd1306_draw_string(ssd, 0, 0, "Retrolavagem");
                    ssd1306_draw_string(ssd, 0, 16, "Faltam");
                    itoa(temporizador, temporizadorStr, 10); //converte o tempo para string
                    ssd1306_draw_string(ssd, 0, 32, temporizadorStr); //exibe o tempo que falta
                    ssd1306_draw_string(ssd, 0, 48, "segundos");
                    render_on_display(ssd, &frame_area);

                    next_wake_time = delayed_by_us(next_wake_time, interval * 1000); //o próximo tempo a ser atingido será o atual + 1 segundo
                    temporizador -= 1; //a contagem regressiva é decrementada
                }
                sleep_ms(1); //delay de 1 milissegundo 
            }
            estado = 0; //estado recebe 0              

            sleep_ms(500); //delay de 0.5 segundo

        } else if(estado == 0) { //se o estado for igual a 0, o sistema está em 'repouso'
            npClear(); //limpa a matriz de leds

            //todos os leds da operação são definidos como VERMELHO
            npSetLED(0, 40, 0, 0);
            npSetLED(1, 40, 0, 0);
            npSetLED(2, 40, 0, 0);
            npSetLED(3, 40, 0, 0);
            npSetLED(4, 40, 0, 0);
            npSetLED(5, 40, 0, 0);
            npSetLED(6, 40, 0, 0);
            npWrite(); //atualiza a matriz

            //limpa o display            
            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            render_on_display(ssd, &frame_area);

            //exibe mensagem mostrando o nível atual do reservatório
            ssd1306_draw_string(ssd, 0, 0, "Estado Inicial");
            ssd1306_draw_string(ssd, 0, 16, "Nivel do");
            ssd1306_draw_string(ssd, 0, 32, "Reservatorio");
            ssd1306_draw_string(ssd, 0, 48, volumePercentualStr);

            render_on_display(ssd, &frame_area);
            estado = -1; //estado recebe -1
        }


        sleep_ms(100);      //delay de 0.1 segundo
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}
