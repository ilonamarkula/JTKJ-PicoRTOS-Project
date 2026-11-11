
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

//Add here necessary states
enum state { IDLE=1 };
enum state programState = IDLE;

// Flag to indicate button press
volatile bool button1_pressed = false;
volatile bool button2_pressed = false;
volatile bool button2_released = false;
volatile TickType_t button2_press_time = 0;
volatile TickType_t button2_release_time = 0;

// Ohjelman tilat
typedef enum {
    WAITING,
    READ_SENSOR,
    SEND_MESSAGE
} state_t;

// oletustila
volatile state_t state = WAITING;

// Picon asennot
typedef enum {
    POSITION_UNKNOWN,
    POSITION_VERTICAL,
    POSITION_HORIZONTAL
} position_t;


static void btn_fxn(uint gpio, uint32_t eventMask) {
    if (gpio == BUTTON1 && (eventMask & GPIO_IRQ_EDGE_FALL)) {
        button1_pressed = true;
    }

    if (gpio == BUTTON2) {
        if (eventMask & GPIO_IRQ_EDGE_FALL) {
            button2_press_time = xTaskGetTickCountFromISR();
        }
        else if (eventMask & GPIO_IRQ_EDGE_RISE) {
            button2_release_time = xTaskGetTickCountFromISR();
            button2_released = true;
        }
    }
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

    float ax, ay, az, gx, gy, gz, t;

    // Alustetaan IMU
    if (init_ICM42670() == 0) {
        printf("ICM-42670P initialized successfully!\n");

        if (ICM42670_start_with_default_values() != 0) {
            printf("Could not initialize accelerometer or gyroscope\n");
        }

    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }

    while (1)
    {
        // Luetaan IMU:n arvot
        if (ICM42670_read_sensor_data(&ax, &ay, &az,
                                      &gx, &gy, &gz, &t) == 0) {

            // Nappi 1: piste tai viiva
            if (button1_pressed) {
                button1_pressed = false;

                if (fabsf(az) > 0.7f) {
                    printf(".");
                }
                else if (fabsf(ax) > 0.7f || fabsf(ay) > 0.7f) {
                    printf("-");
                }
            }

            // Nappi 2: lyhyt tai pitkä painallus
            if (button2_released) {
                button2_released = false;

                TickType_t duration =
                    button2_release_time - button2_press_time;

                if (duration >= pdMS_TO_TICKS(2000)) {
                    printf("  \n");
                }
                else {
                    printf(" ");
                }
            }

        } else {
            printf("Failed to read imu data\n");
        }

        vTaskDelay(10);
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
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, btn_fxn);

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


    TaskHandle_t sensorTask = NULL;

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
    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
    
}

