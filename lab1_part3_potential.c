/*
 * Lab 1, Part 3 - Seven-Segment Display & Keypad
 *
 * ECE-315 WINTER 2026 - COMPUTER INTERFACING
 * Created on: February 5, 2021
 * Modified on: July 26, 2023
 * Modified on: January 20, 2025
 * Modified on: February 11, 2026
 * Author(s):  Shyama Gandhi, Antonio Andara Lara
 *
 * Author(s): Riley Whitford (whitfor1), Komaldeep Taggar (ktaggar)
 *
 * Summary:
 * 1) Declare & initialize the 7-seg display (SSD).
 * 2) Use xDelay to alternate between two digits fast enough to prevent flicker.
 * 3) Output pressed keypad digits on both SSD digits: current_key on right, previous_key on left.
 * 4) Print status changes and experiment with xDelay to find minimum flicker-free frequency.
 *
 * Deliverables:
 * - Demonstrate correct display of current and previous keys with no flicker.
 * - Print to the SDK terminal every time that theh variable `status` changes.
 */


// Include FreeRTOS Libraries
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

// Include xilinx Libraries
#include <xparameters.h>
#include <xgpio.h>
#include <xscugic.h>
#include <xil_exception.h>
#include <xil_printf.h>
#include <sleep.h>
#include <xil_cache.h>

// Other miscellaneous libraries
#include "pmodkypd.h"
#include "rgb_led.h"


// Device ID declarations
#define KYPD_DEVICE_ID   	XPAR_GPIO_KYPD_BASEADDR
/*************************** Enter your code here ****************************/
// TODO: Define the seven-segment display (SSD) base address.

#define SSD_DEVICE_ID       XPAR_GPIO_SSD_BASEADDR

// Defining the RBG Leds and the push buttons base addresses.

#define LEDS_DEVICE_ID      XPAR_GPIO_LEDS_BASEADDR

#define PUSH_DEVICE_ID      XPAR_GPIO_INPUTS_BASEADDR

/*****************************************************************************/

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"

// Declaring the devices
PmodKYPD 	KYPDInst;

/*************************** Enter your code here ****************************/
// TODO: Declare the seven-segment display peripheral here.

XGpio       SSDInst;

// Declaring the RGB Leds and push buttons now.

XGpio       rgbLedInst;

XGpio       pushInst;

// Declaring Queues

QueueHandle_t xKey2DisplayQueue == NULL;
QueueHandle_t xBtn2RGBQueue == NULL;

// Struct for keypad values

typedef struct {
    u8 current;
    u8 previous;
} padData_t;

typedef struct {
	uint8_t	xOn
}	xOn_t;

/*****************************************************************************/

// Function prototypes
void InitializeKeypad();
static void vKeypadTask(void *pvParameters);
static void vRgbTask(void *pvParameters);   // Added vRbgTask function here
static void vButtonsTask(void *pvParameters); // Added vButtonsTask function here
static void vDisplayTask(void *pvParameters); // Added vDisplayTask function here
u32 SSD_decode(u8 key_value, u8 cathode);


int main(void)
{
	int status;

	// Initialize keypad
	InitializeKeypad();

/*************************** Enter your code here ****************************/
	// TODO: Initialize SSD and set the GPIO direction to output.

    // SSD intialized first.
    status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for SSD failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&SSDInst, 1, 0x00);

    // RGB LED intialized second.
    status = XGpio_Initialize(&rgbLedInst, LEDS_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for RGB LED failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&rgbLedInst, 1, 0x00);

    // Push buttons intialized third.
    status = XGpio_Initialize(&pushInst, PUSH_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for Push Button failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&pushInst, 1, 0xFF);

/*****************************************************************************/

	xil_printf("Initialization Complete, System Ready!\n");

	xTaskCreate(vKeypadTask,					/* The function that implements the task. */
				"keypad task", 				/* Text name for the task, provided to assist debugging only. */
				configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
				NULL, 						/* The task parameter is not used, so set to NULL. */
				tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
				NULL);

    // Creating the vRgbTask to start the RGB Leds.
    xTaskCreate(vRgbTask,
                "rgb task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                NULL);

    // Creating the Queues
    xKey2DisplayQueue = xQueueCreate(1, sizeof(padData_t));
        if (xKey2DisplayQueue == NULL) {
            xil_printf("Failed to create Key2Display queue\r\n");
            return XST_FAILURE;
        }

    xBtn2RGBQueue = xQueueCreate(1, sizeof(xOn_t));
        if (xBtn2RGBQueue == NULL) {
            xil_printf("Failed to create Btn2RGB queue");
            return XST_FAILURE;
        }    

    xTaskCreate(vDisplayTask,
                "display task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                NULL);

	xTaskCreat(vButtonsTask,
				"button task"
				configMINIMAL_STACK_SIZE,
				NULL,
				tskIDLE_PRIORITY,
				NULL);

	vTaskStartScheduler();
	while(1);
	return 0;
}

// RGB Led task being defined here; vRgbTask
// xDelay: 10 is when flickering stops, will be used for PWM period value
static void vRgbTask(void *pvParameters)
{
    const uint8_t color = RGB_CYAN;
	const TickType_t xPeriod = 10;
	xOn_t xOn = 5;
	xOn_t rxOn;
	TickType_t xOff;

    while (1)
	{
		if (xQueueReceive(xBtn2RGBQueue, &rxOn, 0) == pdTRUE)
		{
			xOn = rxOn;
			xil_printf("Brightness: %d%%\r\n", (xOn * 100) / xPeriod);
		}
		
        xOff = xPeriod - xOn;

        // LED is ON here
        XGpio_DiscreteWrite(&rgbLedInst, RGB_CHANNEL, color);
        vTaskDelay(xOn);

        // LED is OFF here
        XGpio_DiscreteWrite(&rgbLedInst, RGB_CHANNEL, 0);
        vTaskDelay(xOff);
        }
}


static void vDisplayTask(void *pvParameters) {
    padData_t rxData;
    u32 ssd_value = 0;
    const TickType_t xDelay = pdMS_TO_TICKS(12);

    while(1) 
    {
        if (xQueueReceive(xKey2DisplayQueue, &rxData, portMAX_DELAY) == pdTRUE)
        {
            ssd_value = SSD_decode(rxData.current, 1);
            XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
            vTaskDelay(xOn);

            ssd_value = SSD_decode(rxData.previous, 0);
            XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
            vTaskDelay(xOff);
        }
    }
}

static void vButtonsTask(void *pvParameters) {
	xOn_t xOn = 5;
	const TickType_t xBtn  = pdMS_TO_TICKS(150);   // Delay for button presses, ensures no overshooting

	while (1)
	{
         // Button input
        int readPush = XGpio_DiscreteRead(&pushInst, 1);
    
        // Increase brightness (was previously increase delay)
        if (readPush == 8 && xOn <= 9)) {
            xOn++;
        // Decrease brightness (was previously decrease delay)
        } else if (readPush == 1 && xOn > 1) {
            xOn--;

		xQueueOverwrite(xBtn2RGBQueue, &xOn);
			vTaskDelay(xBtn);
    }
}

static void vKeypadTask( void *pvParameters )
{
	padData_t txData;
	u16 keystate;
	XStatus status, previous_status = KYPD_NO_KEY;
	u8 new_key, current_key = 'x', previous_key = 'x';

/*****************************************************************************/

    xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");
	while (1){
		// Capture state of the keypad
		keystate = KYPD_getKeyStates(&KYPDInst);

		// Determine which single key is pressed, if any
		// if a key is pressed, store the value of the new key in new_key
		status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);
		// Print key detect if a new key is pressed or if status has changed
		if (status == KYPD_SINGLE_KEY && previous_status == KYPD_NO_KEY){
			xil_printf("Key Pressed: %c\r\n", (char) new_key);
/*************************** Enter your code here ****************************/
			// TODO: update value of previous_key and current_key

            previous_key = current_key;
            current_key = new_key;

            txData.current = current_key;
            txData.previous = previous_key;

            xQueueOverwrite(xKey2DisplayQueue, &txData);

/*****************************************************************************/
		} else if (status == KYPD_MULTI_KEY && status != previous_status){
			xil_printf("Error: Multiple keys pressed\r\n");
		}

/*************************** Enter your code here ****************************/
		// TODO: display the value of `status` each time it changes

        if (status != previous_status) {
            xil_printf("Status changed: %d\r\n", status);
        }

/*****************************************************************************/

		previous_status = status;

/*****************************************************************************/
    }
}


void InitializeKeypad()
{
	KYPD_begin(&KYPDInst, KYPD_DEVICE_ID);
	KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}

// This function is hard coded to translate key value codes to their binary representation
u32 SSD_decode(u8 key_value, u8 cathode)
{
    u32 result;

	// key_value represents the code of the pressed key
	switch(key_value){ // Handles the coding of the 7-seg display
		case 48: result = 0b00111111; break; // 0
        case 49: result = 0b00110000; break; // 1
        case 50: result = 0b01011011; break; // 2
        case 51: result = 0b01111001; break; // 3
        case 52: result = 0b01110100; break; // 4
        case 53: result = 0b01101101; break; // 5
        case 54: result = 0b01101111; break; // 6
        case 55: result = 0b00111000; break; // 7
        case 56: result = 0b01111111; break; // 8
        case 57: result = 0b01111100; break; // 9
        case 65: result = 0b01111110; break; // A
        case 66: result = 0b01100111; break; // B
        case 67: result = 0b00001111; break; // C
        case 68: result = 0b01110011; break; // D
        case 69: result = 0b01001111; break; // E
        case 70: result = 0b01001110; break; // F
        default: result = 0b00000000; break; // default case - all seven segments are OFF
    }

	// cathode handles which display is active (left or right)
	// by setting the MSB to 1 or 0
    if(cathode==0){
            return result;
    } else {
            return result | 0b10000000;
	}
}
