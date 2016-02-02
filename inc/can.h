#ifndef __CAN_H__
#define __CAN_H__

#include "FreeRTOS.h"
#include "task.h"

#ifdef TARGET_F407
#include "stm32f4xx_can.h"

#define CANx                       CAN1
#define CAN_CLK                    RCC_APB1Periph_CAN1
#define CAN_RX_PIN                 GPIO_Pin_0
#define CAN_TX_PIN                 GPIO_Pin_1
#define CAN_GPIO_PORT              GPIOD
#define CAN_GPIO_CLK               RCC_AHB1Periph_GPIOD
#define CAN_AF_PORT                GPIO_AF_CAN1
#define CAN_RX_SOURCE              GPIO_PinSource0
#define CAN_TX_SOURCE              GPIO_PinSource1 
#define CAN_IRQ                    CAN1_RX0_IRQn
#endif /* TARGET_F407 */

#ifdef TARGET_F091
#include "stm32f0xx_can.h"

#define CANx			CAN
#define CAN_CLK			RCC_APB1Periph_CAN
#define CAN_RX_PIN		GPIO_Pin_11
#define CAN_TX_PIN		GPIO_Pin_12
#define CAN_GPIO_PORT		GPIOA
#define CAN_GPIO_CLK		RCC_AHBPeriph_GPIOA
#define CAN_AF_PORT		GPIO_AF_4
#define CAN_RX_SOURCE		GPIO_PinSource11
#define CAN_TX_SOURCE		GPIO_PinSource12
#define CAN_IRQ			CEC_CAN_IRQn
#endif /* TARGET_F091 */

extern CanRxMsg RxMessage;

extern TaskHandle_t can_listen_handle;

void can_init();
void task_can_listen(void *vpars);
#endif
