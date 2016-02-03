#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#ifdef TARGET_F407
#include "stm32f4xx_gpio.h"
#include "misc.h"
#endif
#ifdef TARGET_F091
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_misc.h"
#include "stm32f0xx_rcc.h"
#endif
#include "can.h"

CanRxMsg RxMessage;
TaskHandle_t can_listen_handle = NULL;

static void NVIC_Config(void)
{
	NVIC_InitTypeDef  NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = CAN_IRQ;
#ifdef TARGET_F407
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
#endif
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

static void CAN_Config(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	CAN_InitTypeDef        CAN_InitStructure;
	CAN_FilterInitTypeDef  CAN_FilterInitStructure;

	/* CAN GPIOs configuration */

	/* Enable GPIO clock */
#ifdef TARGET_F407
	RCC_AHB1PeriphClockCmd(CAN_GPIO_CLK, ENABLE);
#endif

#ifdef TARGET_F091
	RCC_AHBPeriphClockCmd(CAN_GPIO_CLK, ENABLE);
#endif
	/* Connect CAN pins to AF9 */
	GPIO_PinAFConfig(CAN_GPIO_PORT, CAN_RX_SOURCE, CAN_AF_PORT);
	GPIO_PinAFConfig(CAN_GPIO_PORT, CAN_TX_SOURCE, CAN_AF_PORT); 
	
	/* Configure CAN RX and TX pins */
	GPIO_InitStructure.GPIO_Pin = CAN_RX_PIN | CAN_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_Init(CAN_GPIO_PORT, &GPIO_InitStructure);

	/* CAN configuration */  
	/* Enable CAN clock */
	RCC_APB1PeriphClockCmd(CAN_CLK, ENABLE);
	
	/* CAN register init */
	CAN_DeInit(CANx);

	/* CAN cell init */
	CAN_InitStructure.CAN_TTCM = DISABLE;
	CAN_InitStructure.CAN_ABOM = DISABLE;
	CAN_InitStructure.CAN_AWUM = DISABLE;
	CAN_InitStructure.CAN_NART = DISABLE;
	CAN_InitStructure.CAN_RFLM = DISABLE;
	CAN_InitStructure.CAN_TXFP = DISABLE;
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
#ifdef TARGET_F407
	CAN_InitStructure.CAN_Prescaler = 7;
#endif
#ifdef TARGET_F091
	CAN_InitStructure.CAN_Prescaler = 8;
#endif
	/* 1Mb */
	CAN_InitStructure.CAN_BS1 = CAN_BS1_2tq;
	CAN_InitStructure.CAN_BS2 = CAN_BS2_3tq;
	CAN_Init(CANx, &CAN_InitStructure);

	/* CAN filter init */
	CAN_FilterInitStructure.CAN_FilterNumber = 0;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
	CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = 0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);
	
	/* Enable FIFO 0 message pending Interrupt */
	CAN_ITConfig(CANx, CAN_IT_FMP0, ENABLE);
}

void can_init()
{
	NVIC_Config();
	CAN_Config();
};

void task_can_listen(void *vpars)
{
	uint32_t notify;

	can_listen_handle = xTaskGetCurrentTaskHandle();

	while (1) {
		notify = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
		if (notify == 1) {
			int i;
			printf("\r\nCAN packet received\r\n");
			printf("StdId: %x\r\n", RxMessage.StdId);
			printf("ExtId: %x\r\n", RxMessage.ExtId);
			printf("payload(%d): ", RxMessage.DLC);
			for (i = 0; i < RxMessage.DLC; i++)
				printf(" %02x", RxMessage.Data[i]);
			printf("\r\n");
		}
	}
}


#ifdef TARGET_F407
void CAN1_RX0_IRQHandler(void)
#endif
#ifdef TARGET_F091
void CEC_CAN_IRQHandler(void)
#endif
{
	CAN_Receive(CANx, CAN_FIFO0, &RxMessage);
	if (can_listen_handle != NULL)
		vTaskNotifyGiveFromISR(can_listen_handle, NULL);
}
