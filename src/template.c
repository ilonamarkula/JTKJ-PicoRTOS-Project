
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

// Ohjelman tilat
typedef enum {
    WAITING,
    READ_SENSOR,
    PRINT_SYMBOL,
    PRINT_SPACE,
    PRINT_NEWLINE
} state_t;

// Globaalit IMU:n arvot
float ax, ay, az, gx, gy, gz, t;

// Välilyöntilaskuri
int space_counter = 0;
char symbol;

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


        // nappi1 -> menee tilaan: piste tai viiva -> menee tilaan: tulostus -> menee tilaan: odotus
        //nappi 2 tulostaa välin.

        // teillä on välilyöntilaskuri ja kun laskuri on == 3
        // niin tulostaa rivin vaihdon 
}

/*

    Taski()
     taskin sisällä mitataan arvoja
     ja kun nappia painetaan niin kutsutaan nappi keskeytyskäsittelijää
     nappi tulostaa joko pisteen tai viivan

*/

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
        if (ICM42670_read_sensor_data(&ax, &ay, &az,
                                      &gx, &gy, &gz, &t) != 0) {
            printf("Failed to read imu data\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
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

                if (fabsf(az) > 0.7f) {
                    symbol = '.';
                }
                else if (fabsf(ax) > 0.7f || fabsf(ay) > 0.7f) {
                    symbol = '-';
                }
                else {
                    symbol = '\0';
                }

                state = PRINT_SYMBOL;
                break;

            // Tulostetaan piste tai viiva
            case PRINT_SYMBOL:

                if (symbol != '\0') {
                    printf("%c", symbol);
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

// Summerin soiminen
static void buzzer_task(void *pvParameters) {
    (void)pvParameters;

    init_buzzer();

    while (1) {

        /*
        hat_example:

        if (sw2_pressed) {
            buzzer_play_tone(440, 500);
        }
        vTaskDelay(10);



        hello_hat example:

        buzzer_play_tone (440, 500);
        vTaskDelay(pdMS_TO_TICKS(6000));

        */
    }
}

// LCD näytölle tulostus
static void display_task(void *arg) {
    (void)arg;

    init_display();


    /*
    hello_hat example:

    static int counter = 0; 

    while (1) {
        clear_display();
        char buf[5]; //Store a number of maximum 5 figures 
        sprintf(buf,"%d",counter++);
        write_text(buf);
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
    */
}

/* tehkää taski mitatkaa sen sisällä IMUn arvoja

tehkää napinkeskeytyskäsittelijä

nappi tulostaa arvoja

*/

int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    init_sw1();
    init_sw2();
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL, true);


    // Tästä alaspäin on pelkkää taskien luomista


    // Periaatteessa tätä TaskHandle_t me ei kai tarvita, et vois
    // vaan suoraan laittaa xTaskCreate() vikaan kohtaan NULL
    // (kysykää AI en osaa selittää T:Iitu)
    TaskHandle_t sensorTask, printTask, buzzerTask, displayTask = NULL;

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
