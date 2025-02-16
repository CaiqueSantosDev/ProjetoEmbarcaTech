#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define WIFI_SSID "Internet" // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "Regeddit" // Substitua pela senha da sua rede Wi-Fi
#define PORT 8080           // Porta para comunicação TCP

// Variável global para armazenar o número de dedos
int dedos_levantados = 6;

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin, int buzzer_frequency) {
    gpio_set_function(pin, GPIO_FUNC_PWM); // Configurar pino como saída PWM
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obter o slice do PWM

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (buzzer_frequency * 4096)); // Ajuste do clock
    pwm_init(slice_num, &config, true);

    pwm_set_gpio_level(pin, 0); // Começa desligado
}

// Liga o buzzer continuamente
void buzzer_on(uint pin) {
    pwm_set_gpio_level(pin, 2048); // Define duty cycle de 50%
}

// Desliga o buzzer
void buzzer_off(uint pin) {
    pwm_set_gpio_level(pin, 0); // Define duty cycle como 0%
}

void verificar_dedos(int pino_led_verde, int pino_led_vermelho, int buzzer, int valor_dedo_levantado) {
    if (valor_dedo_levantado == 0) {
        buzzer_on(buzzer); // Mantém o buzzer ligado sem intervalo
        gpio_put(pino_led_vermelho, 1); // Acende o LED
    } else {
        buzzer_off(buzzer); // Desliga o buzzer
        gpio_put(pino_led_vermelho, 0); // Apaga o LED
    }
    if (valor_dedo_levantado == 5) {
        gpio_put(pino_led_verde, 1); // Acende o LED
    } else {
        gpio_put(pino_led_verde, 0); // Apaga o LED
    }
}

// Função de callback para processar requisições TCP
static err_t tcp_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = (char *)p->payload;

    // Tenta converter a requisição para um inteiro (número de dedos)
    if (sscanf(request, "%d", &dedos_levantados) == 1) {
        printf("Número de dedos recebidos: %d\n", dedos_levantados);
    }

    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o tcp_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_callback); // Associa o callback TCP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_tcp_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    if (tcp_bind(pcb, IP_ADDR_ANY, PORT) != ERR_OK) { // Usa a porta definida
        printf("Erro ao ligar o servidor na porta %d\n", PORT);
        return;
    }

    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);

    printf("Servidor TCP rodando na porta %d...\n", PORT);
}

int main() {
    const int pino_led_verde = 11;
    const int pino_led_vermelho = 13;
    const int buzzer = 21;
    const int buzzer_frequeny = 2000;

    gpio_init(pino_led_verde);
    gpio_set_dir(pino_led_verde, GPIO_OUT);
    gpio_put(pino_led_verde, 0); // 🔹 Define o LED como apagado no início

    gpio_init(pino_led_vermelho);
    gpio_set_dir(pino_led_vermelho, GPIO_OUT);
    gpio_put(pino_led_vermelho, 0); // 🔹 Define o LED como apagado no início

    stdio_init_all();
    sleep_ms(2000); // Aguarda inicialização

    printf("Iniciando servidor TCP\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi após 30 segundos.\n");
        return 1;
    } else {
        printf("Conectado.\n");
        uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    printf("Wi-Fi conectado!\n");

    start_tcp_server(); // Inicia o servidor TCP
    pwm_init_buzzer(buzzer, buzzer_frequeny);

    while (true) {
        cyw43_arch_poll(); // Mantém o Wi-Fi ativo
        verificar_dedos(pino_led_verde, pino_led_vermelho, buzzer, dedos_levantados); // Passa o valor para a função
        sleep_ms(1000);
    }

    cyw43_arch_deinit();
    return 0; // Adicione o return 0 para indicar sucesso
}