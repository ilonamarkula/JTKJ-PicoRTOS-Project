
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#include <stdbool.h>
#include <math.h>

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048

// Bufferikoko receive_taskille (merkit työasemalta Picolle)
#define INPUT_BUFFER_SIZE 256

// Ohjelman tilat
typedef enum {
    WAITING,
    READ_SENSOR,
    PRINT_SYMBOL,
    PRINT_SPACE,
    PRINT_NEWLINE
} state_t;

// Globaalit IMU:n arvot
float ax, ay,az, gx, gy, gz, t;

// Välilyöntilaskuri
int space_counter = 0;
char symbol;

// Summerin ja LCD-näytön FreeRTOS-jonon kahvat
QueueHandle_t buzzerQueue = NULL;
QueueHandle_t displayQueue = NULL;

TaskHandle_t sensorTask = NULL;

// Oletustila
volatile state_t state = WAITING;

// Picon asennot
typedef enum {
    POSITION_UNKNOWN,
    POSITION_VERTICAL,
    POSITION_HORIZONTAL
} position_t;


static void btn_fxn(uint gpio, uint32_t eventMask) {

    //Tila vaihtuu
    if (gpio == BUTTON1 && (eventMask & GPIO_IRQ_EDGE_FALL)) {
        space_counter = 0; // Reset space counter when button 1 is pressed
        state = READ_SENSOR;
    }

    if (gpio == BUTTON2 && (eventMask & GPIO_IRQ_EDGE_FALL)) {
        space_counter++;
        if (space_counter == 3) {
            state = PRINT_NEWLINE;
            space_counter = 0;
        } else {
            state = PRINT_SPACE;
        }
    }
}

// IMU:n lukeminen ja käsittely
static void sensor_task(void *pvParameters) {
    (void)pvParameters;

    // Alustetaan IMU
    if (init_ICM42670() == 0) {
        printf("ICM-42670P initialized successfully!\n");

        if (ICM42670_start_with_default_values() != 0) {
            printf("Could not initialize accelerometer or gyroscope\n");
            vTaskDelete(NULL);
        }

    } else {
        printf("Failed to initialize ICM-42670P.\n");
        vTaskDelete(NULL);
    }

    while (1) {
        // Luetaan IMU:n arvot
        // Tallennetaan suoraan globaaleihin muuttujiin,
        // virheen sattuessa printf
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0){
        } else {
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Merkkien tulostus
static void print_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        switch (state) {
            // Ei tapahdu mitään
            case WAITING:
                break;

            // Tunnistetaan Picon asento
            case READ_SENSOR:
                vTaskDelay(pdMS_TO_TICKS(200));
                symbol = '\0';
                if (fabsf(az) > 0.5f) {
                    symbol = '-';
                }
                else if (fabsf(az) < 0.5f) {
                    symbol = '.';
                }
                
                state = PRINT_SYMBOL;
                break;

            // Tulostetaan piste tai viiva
            case PRINT_SYMBOL:
                if (symbol != '\0') {
                    printf("%c", symbol);

                    // Lähetetään morsemerkki summerijonoon
                    xQueueSend(buzzerQueue, &symbol, 0);
                }

                state = WAITING;
                break;

            // Tulostetaan välilyönti
            case PRINT_SPACE:

                printf(" ");
                state = WAITING;
                break;

            // Päätetään viesti
            case PRINT_NEWLINE:

                printf("\n");
                state = WAITING;
                break;

            default:
                state = WAITING;
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Merkit työasemalta Picolle
static void receive_task(void *pvParameters){
    (void)pvParameters;
    char line[INPUT_BUFFER_SIZE];
    size_t index = 0;

    while (1) {
        int c = getchar_timeout_us(0); // Kurkistaa sarjaporttiin
        if (c != PICO_ERROR_TIMEOUT) { // Merkki kirjoitettu
            if (c == '\r') continue;

            // Enter painettu, viesti valmis
            if (c == '\n') {
                line[index] = '\0';    
                printf("\n__Työaseman viesti morsena:   %s__\n", line); // Debug-viesti terminaaliin

                // Lähetetään koko viesti LCD jonoon
                xQueueSend(displayQueue, line, 0);

                // Lähetetään viesti merkki kerrallaan summerijonoon
                for (size_t i = 0; i < index; i++) {
                    xQueueSend(buzzerQueue, &line[i], portMAX_DELAY);
                }
                index = 0;
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            // Kirjoitetaan lisää merkkejä jos bufferissa on tilaa
            else if (index < INPUT_BUFFER_SIZE -1 ) {
                line[index++] = (char)c;
            }

            // Bufferi täynnä, printataan ja lähetetään viesti samallain ku ylempänä
            else {
                line[INPUT_BUFFER_SIZE - 1] = '\0';
                printf("\n__Työaseman viesti morsena:   %s__\n", line);
                xQueueSend(displayQueue, line, 0);
                for (size_t i = 0; i < INPUT_BUFFER_SIZE - 1; i++) {
                    xQueueSend(buzzerQueue, &line[i], portMAX_DELAY);
                }
                index = 0; 
                line[index++] = (char)c; 
            }
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Morsemerkkien äänet
static void play_morse_sound(char symbol) {
    if (symbol == '.') {
        buzzer_play_tone(440, 150);     // Lyhyt piippaus
        vTaskDelay(pdMS_TO_TICKS(100)); // Tauko piippausten välissä selkeyttää
    }
    
    else if (symbol == '-') {
        buzzer_play_tone(440, 450);     // Pitkä piippaus
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Lisäsin taukoja että piippauksia on helpompi kuunnella T:iitu
    else if (symbol == ' ') {
        vTaskDelay(pdMS_TO_TICKS(300)); // Tauko kirjainten ja sanojen välissä
    }
}

// Summerin FreeRTOS-taski
static void buzzer_task(void *pvParameters) {
    (void)pvParameters;

    init_buzzer();

    // Vastaanotettava morsemerkki
    char receivedSymbol;

    while (1) {

        // Odottaa merkkiä summerijonosta
        if (xQueueReceive(buzzerQueue, &receivedSymbol, portMAX_DELAY) == pdTRUE) {
            play_morse_sound(receivedSymbol);
        }
    }
}

// LCD näytölle tulostus
static void display_task(void *pvParameters) {
    (void)pvParameters;

    char receivedLine[INPUT_BUFFER_SIZE];

    while (1) {
        // Odottaa viestiä LCD jonosta
        if (xQueueReceive(displayQueue, receivedLine, portMAX_DELAY) == pdTRUE) {
            vTaskSuspend(sensorTask); // Pysäytetään anturin luku päivityksen ajaksi

            clear_display();
            write_text(receivedLine);

            vTaskResume(sensorTask);  // Jatketaan anturin lukua
        }
    }
}

int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(
    }*/ 
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    init_display();
    clear_display();
    write_text("Valmis!"); // Alkuteksti ruudulle
    
    // Luo summerin FreeRTOS jonon
    buzzerQueue = xQueueCreate(64, sizeof(char));

    // Tarkistaa epäonnistuiko jonon luominen
    if (buzzerQueue == NULL) {
        printf("Buzzer queue creation failed\n");
        return 0;
    }

    // Luo LCD-näytön FreeRTOS jonon
    displayQueue = xQueueCreate(3, sizeof(char[INPUT_BUFFER_SIZE]));
    if (displayQueue == NULL) {
        printf("Display queue creation failed\n");
        return 0;
    }

    init_sw1();
    init_sw2();
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL, true);


    // Tästä alaspäin on pelkkää taskien luomista

    // Periaatteessa tätä TaskHandle_t me ei kai tarvita, et vois
    // vaan suoraan laittaa xTaskCreate() vikaan kohtaan NULL
    TaskHandle_t printTask, receiveTask, buzzerTask, displayTask = NULL;

    BaseType_t result = xTaskCreate(
        sensor_task,                // taskifunktio
        "SensorTask",               // taskin nimi
        DEFAULT_STACK_SIZE,         // stack size
        NULL,                       // taskin argumentit
        2,                          // prioriteetti
        &sensorTask                 // TaskHandle (kahva)
    );
    if (result != pdPASS) {
        printf("Sensor Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(
        print_task,
        "PrintTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &printTask
    );
    if (result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(
        receive_task,
        "ReceiveTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &receiveTask
    );
    if (result != pdPASS) {
        printf("Receive Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(
        buzzer_task,
        "BuzzerTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &buzzerTask
    );
    if (result != pdPASS) {
        printf("Buzzer Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(
        display_task,
        "DisplayTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &displayTask
    );
    if (result != pdPASS) {
        printf("Display Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
