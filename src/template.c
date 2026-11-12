
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

// globaalit IMU:n arvot
float ax, ay, az, gx, gy, gz, t;

// Välilyöntilaskuri
int space_counter = 0;
char symbol;

// oletustila
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
            }
            else {
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
void sensor_task(void *pvParameters)
{
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

    while (1)
    {
        // Luetaan IMU:n arvot
        if (ICM42670_read_sensor_data(&ax, &ay, &az,
                                      &gx, &gy, &gz, &t) == 0) {

            

        } else {
            printf("Failed to read imu data\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// merkkien tulostus
void print_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        switch (state)
        {
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

    /*btn_fxn(){
        if(nappi = 1)
        //tekee jtoain

        jos nappi= 2
        /tekee jotain
    }*/

    /*
    TaskHandle_t myExampleTask = NULL;
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(example_task,       // (en) Task function
                "example",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &myExampleTask);    // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Example Task creation failed\n");
        return 0;
    } */


    TaskHandle_t sensorTask, printTask = NULL;

    BaseType_t result = xTaskCreate(
        sensor_task,
        "SensorTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &sensorTask
    );

    if(result != pdPASS) {
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
    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
    
}

