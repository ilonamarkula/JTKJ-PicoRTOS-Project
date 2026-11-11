
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048 

//Add here necessary states
enum state { IDLE=1 };
enum state programState = IDLE;

// Flag to indicate button press
volatile bool button1_pressed = false;
volatile bool button2_pressed = false;


// vakiot eri asennoille
typedef enum {
    POSITION_UNKNOWN,
    POSITION_VERTICAL,
    POSITION_HORIZONTAL,
    WAITING,
    READ_SENSOR,
    SEND_MESSAGE
} position_t;

// oletustila
volatile state_t state = WAITING;

static void btn_fxn(uint gpio, uint32_t eventMask) {
    if(gpio == BUTTON1 && (eventMask & GPIO_IRQ_EDGE_FALL)) {
        button1_pressed = true;
    }
    else if (gpio == BUTTON2 && (eventMask & GPIO_IRQ_EDGE_FALL)) {
        button2_pressed = true;
    }

position_t recognizePosition(void) {
    float ax, ay, az, gx, gy, gz, t;

    if (ICM42670_read_sensor_data(
            &ax, &ay, &az, &gx, &gy, &gz, &t) != 0) {
        return POSITION_UNKNOWN;
    }

    if (fabsf(az) < 0.4f) {
        return POSITION_VERTICAL;
    }

    if (fabsf(az) > 0.7f) {
        return POSITION_HORIZONTAL;
    }

    return POSITION_UNKNOWN;
}



/*

    Taski()
     taskin sisällä mitataan arvoja
     ja kun nappia painetaan niin kutsutaan nappi keskeytyskäsittelijää
     nappi tulostaa joko pisteen tai viivan



*/


void read_IMU(void *pvParameters) {
    (void)pvParameters;

    while (1) {


        }
    vTaskDelay(10);
    }
}

void button2_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        sw1_pressed = gpio_get(BUTTON2);
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
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    btn_fxn(){
        if(nappi = 1)
        //tekee jtoain

        jos nappi= 2
        /tekee jotain
    }
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
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}

