#include "RTE_Components.h"
#include CMSIS_device_header
#define UMBRAL_OSCURO 1300 // Definir un umbral para determinar si hay suficiente luz ambiente

volatile uint16_t lecturas[2]; // Array para almacenar las lecturas del ADC [0] = potenciometro, [1] = fotoresistor
volatile uint16_t indice = 0; // que canal de la secuencia acaba de llegar


void gpio_init(void);
void adc_init(void);
void tim3_init(void);
void mostrar_nivel(uint16_t nivel);
void ADC1_2_IRQHandler(void);

int main() {
    gpio_init();
    adc_init();
    tim3_init();
    for (;;) {
        if(lecturas[1] < UMBRAL_OSCURO){ // Si la lectura del fotoresistor es menor que el umbral, significa que hay poca luz
            TIM3 -> CCR3 = lecturas[0]; // Ajustar el duty cycle del PWM según la lectura del potenciometro
        }else{
            TIM3 -> CCR3 = 0; // Apagar el LED si hay suficiente luz
        }
        mostrar_nivel(lecturas[0]); // Mostrar el nivel de intensidad con los LEDs según la lectura del potenciometro
    }
}

void gpio_init(void){
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN; // Habilitar reloj GPIOA
    RCC -> AHB2ENR |= RCC_AHB2ENR_GPIOBEN; // Habilitar reloj GPIOB

    GPIOB -> MODER &= ~(3<<0); // Limpiar bits de modo para PB0
    GPIOB -> MODER |= (2<<0); // Configurar PB0 como salida alternativa (AF)
    GPIOB -> AFR[0] &= ~(0xF<<0); // Limpiar bits de AF para PB0
    GPIOB -> AFR[0] |= (2<<0); // Configurar PB0 como AF2 (TIM3_CH3)

    GPIOB -> MODER &= ~(3<<8); // Limpiar bits de modo para PB4
    GPIOB -> MODER |= (1<<8); // Configurar PB4 como salida

    GPIOB -> MODER &= ~(3<<10); // Limpiar bits de modo para PB5
    GPIOB -> MODER |= (1<<10); // Configurar PB5 como salida

    GPIOA -> MODER &= ~(3<<24); // Limpiar bits de modo para PA12
    GPIOA -> MODER |= (1<<24); // Configurar PA12 como salida

    GPIOA -> MODER &= ~(3<<22); // Limpiar bits de modo para PA11
    GPIOA -> MODER |= (1<<22); // Configurar PA11 como salida

    GPIOA -> MODER &= ~(3<<0); // Limpiar bits de modo para PA0
    GPIOA -> MODER |= (3<<0); // Configurar PA0 como entrada analógica

    GPIOA -> MODER &= ~(3<<2); // Limpiar bits de modo para PA1
    GPIOA -> MODER |= (3<<2); // Configurar PA1 como entrada analógica
}

void tim3_init(void){
    RCC -> APB1ENR1 |= RCC_APB1ENR1_TIM3EN; // Habilitar reloj TIM3
    TIM3 -> PSC = 3; // Prescaler 16Mhz / 4 = 4Mhz
    TIM3 -> ARR = 4095; // 4 MHz / 4096 = 976.5625 Hz esta es el periodo de la señal PWM
    TIM3 -> CCR3 = 2048; // 50% duty cycle

    TIM3 -> CCMR2 &= ~((3<<0)|(7<<4)|(1<<16)); // Limpiar CC3S (bits 1:0) y OC3M (bits 6:4 y bit 16) para CH3
    TIM3 -> CCMR2 |= (6<<4); // Configurar CH3 en modo PWM1 OC3M=0110 PWM modo 1
    TIM3 -> CCMR2 |= (1<<3); // Habilitar preload para CH3

    TIM3 -> CCER &= ~(1<<9); // CC3P=0, salida activa en alto
    TIM3 -> CCER |= (1<<8); // Habilitar salida para CH3

    TIM3 -> CR1 &= ~((1<<4)|(3<<5)); // DIR=0, contador ascendente, CMS = 00, edge-aligned mode
    TIM3 -> CR1 |= (1<<7); // ARPE: precarga habilitada de ARR

    TIM3 -> EGR |= (1<<0); // UG: cargar el valor de ARR y PSC y CCR3
    TIM3 -> CR1 |= (1<<0); // CEN: arrancar el contador

}

void adc_init(void){
    RCC -> AHB2ENR |= RCC_AHB2ENR_ADC12EN; // Habilitar reloj ADC1/ADC2
    ADC12_COMMON -> CCR &= ~(3<<16); // Limpiar bits de CKMODE para seleccionar el reloj del ADC
    ADC12_COMMON -> CCR |= (1<<16); // CKMODE=01, HCLK/1 = 16 MHz

    ADC1 -> CR &= ~(1<<29); // DEEPPWD=0 salir del modo de bajo consumo
    ADC1 -> CR |= (1<<28); // ADVREGEN=1 encender regulador de voltaje del ADC
    for(volatile int i=0; i<10000; i++); // Esperar >= 20us para que el regulador se estabilice

    ADC1 -> CR &= ~(1<<30); // ADCALDIF=0, calibrar en modo single-ended
    ADC1 -> CR |= (1U<<31); // ADCAL=1, iniciar calibración
    while (ADC1 -> CR & (1U<<31)); // Esperar a que la calibración termine (ADCAL vuelve a 0)

    ADC1 -> SMPR1 |= (7<<3) | (7<<6); // Tiempo de muestreo maximo (640.5 ADC cycles) para canales 1 y 2

    ADC1 -> SQR1 &= ~((0xF<<0)|(0x1F<<6)|(0x1F<<12)); // Limpiar bits de L, SQ1 y SQ2
    ADC1 -> SQR1 |= (1<<0); //L=1, 2 conversiones en la secuencia (L+1)
    ADC1 -> SQR1 |= (1<<6); // SQ1=1, primer canal en la secuencia es el canal 1 (PA0)
    ADC1 -> SQR1 |= (2<<12); // SQ2=2, segundo canal en la secuencia es el canal 2 (PA1)

    ADC1 -> CFGR &= ~(3<<3); // RES=00, resolución de 12 bits
    ADC1 -> CFGR |= (1<<13); // CONT=1, modo de conversión continua
    ADC1 -> CFGR |= (1<<12); // OVRMOD=1, sobrescritura de datos, el valor de DR se sobrescribe con cada nueva conversión

    ADC1 -> IER |= (1<<2); // EOCIE=1, habilitar interrupción al terminar la conversión de cada canal
    ADC1 -> IER |= (1<<3); // EOSIE=1, habilitar interrupción al terminar la secuencia de conversiones

    NVIC_EnableIRQ(ADC1_2_IRQn); // Habilitar interrupción del ADC en el NVIC

    ADC1 -> ISR = (1<<0); // Limpiar bandera ADRDY escribiendo 1
    ADC1 -> CR |= (1<<0); // ADEN=1 encender el ADC
    while(!(ADC1 -> ISR & (1<<0))); // Esperar a que ADRDY=1, el ADC está listo

    ADC1 -> CR |= (1<<2); // ADSTART=1, iniciar la conversión
}

void ADC1_2_IRQHandler(void){
    uint32_t estado = ADC1 -> ISR; // Leer el registro de estado del ADC

    if(estado & (1<<2)){ //EOC termino una conversion de un canal
        uint16_t dato = ADC1 -> DR; // Leer el dato convertido tambien limpia la bandera EOC
        if(indice < 2){
            lecturas[indice] = dato; // Guardar el dato en el arreglo de lecturas
            indice++; // Incrementar el índice para la siguiente lectura
        }
    }

    if(estado & (1<<3)){ //EOS termino la secuencia de conversiones
        ADC1 -> ISR = (1<<3); // Limpiar la bandera EOS escribiendo 1
        indice = 0; // Reiniciar el índice para la siguiente secuencia
    }
}

void mostrar_nivel(uint16_t nivel){
    uint16_t leds_encender = (nivel * 5) / 4096; // 5 niveles parejos, 819 cuentas cada uno
    uint32_t encender_a = 0; // pines de gpioa que se deben encender
    uint32_t encender_b = 0; // pines de gpiob que se deben encender

    if(leds_encender >= 1){
        encender_a |= (1<<12); // Encender LED (PA12)
    }
    if(leds_encender >= 2){
        encender_a |= (1<<11); // Encender LED (PA11)
    }
    if(leds_encender >= 3){
        encender_b |= (1<<5); // Encender LED (PB5)
    }
    if(leds_encender >= 4){
        encender_b |= (1<<4); // Encender LED (PB4)
    }

    GPIOA -> BSRR = (((1<<12)|(1<<11)) << 16) | encender_a; // Bits 16-31 apagan PA12/PA11, bit 0-15 encienden los que toquen
    GPIOB -> BSRR = (((1<<4)|(1<<5)) << 16) | encender_b; // Bits 16-31 apagan PB4/PB5, bit 0-15 encienden los que toquen
}
