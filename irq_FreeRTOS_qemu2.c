/*==================================================================================================

  irq_FreeRTOS_qemu.c

  Used hardware: STM32F4-Discovery evaluation board (QEMU)

  Example of using the FreeRTOS Real-Time Operating System on the STM32F4-Discovery board.

  ==================================================================================================
*/

#include "diag/Trace.h"
#include "stm32f4_discovery.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#define BUFFER_SIZE 4


SemaphoreHandle_t xSemFree = NULL;
SemaphoreHandle_t xSemFull = NULL;
TaskHandle_t xProducer = NULL;
uint8_t buffer[BUFFER_SIZE];


void EXTI0_IRQHandler(void){
	EXTI->PR = EXTI_IMR_IM0; // clear interrupt flag

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(xProducer, &xHigherPriorityTaskWoken);

	//portEND_SWITCHING_ISR(xHigherPriorityTaskWoken); // causes UsageFault in QEMU ARM!
	if(xHigherPriorityTaskWoken != pdFALSE)
	  portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
} // EXTI0_IRQHandler


void xProdTask(void *pvParam){
  uint8_t index_i = 0, data_i = 0xF0;

  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // data production
    register uint8_t aux = data_i & 0x80;
    data_i <<= 1;
    data_i += aux >> 7;

    if(xSemaphoreTake(xSemFree, 0) == pdTRUE){; // is there any free space?
      buffer[index_i] = data_i; // put data
      xSemaphoreGive(xSemFull); // data available

      index_i++; // update input index
      if(index_i >= BUFFER_SIZE)
	    index_i = 0;
    } // if
    else
	  trace_puts("Buffer full!");
  } // while
} // xProdTask


void xConsTask(void *pvParam){
  const TickType_t xPeriod = 500 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint8_t index_o = 0, data_o;

  while(1){
	if(xSemaphoreTake(xSemFull, 0) == pdTRUE){ // is there any data available?
	  data_o = buffer[index_o]; // get data
	  xSemaphoreGive(xSemFree); // free space

	  index_o++; // update output index
	  if(index_o >= BUFFER_SIZE)
		index_o = 0;

	  // data consumption
	  if (data_o & 0x01) BSP_LED_On(LED3);
	  else BSP_LED_Off(LED3);

	  if (data_o & 0x02) BSP_LED_On(LED4);
	  else BSP_LED_Off(LED4);

	  if (data_o & 0x04) BSP_LED_On(LED6);
	  else BSP_LED_Off(LED6);

	  if (data_o & 0x08) BSP_LED_On(LED5);
	  else BSP_LED_Off(LED5);
	} // if
    else
      trace_puts("Buffer empty!");
	vTaskDelayUntil(&xLastWakeTime, xPeriod);
  } // while
} // xConsTask


void main(void){
  __HAL_RCC_GPIOD_CLK_ENABLE();
  BSP_LED_Init(LED3);
  BSP_LED_Init(LED4);
  BSP_LED_Init(LED6);
  BSP_LED_Init(LED5);

  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);

  xTaskCreate(xProdTask, "Producer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &xProducer);
  xTaskCreate(xConsTask, "Consumer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  xSemFree = xSemaphoreCreateCounting(BUFFER_SIZE, BUFFER_SIZE);
  xSemFull = xSemaphoreCreateCounting(BUFFER_SIZE, 0);

  vTaskStartScheduler();
  while(1);
} // main
