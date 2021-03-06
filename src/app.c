#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#ifdef TARGET_F407
#include "usbd_cdc_core.h"
#include "usbd_usr.h"
#include "usb_conf.h"
#include "usbd_desc.h"
#include "stm32f4xx_gpio.h"
#include "f4d_leds.h"
#endif

#ifdef TARGET_F091
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_rcc.h"
#include "stm32f0xx_usart.h"
#endif

#include "can.h"
#include "can_msg.h"

#include "delay.h"
#include "version.h"

#define TXRXDELTAMAX	256

static volatile uint32_t ping_pending[TXRXDELTAMAX];
static volatile int ping_rx, ping_tx, ping_pending_count, ping_trace;
static volatile int txrxdelta = TXRXDELTAMAX;
static volatile int timeout = TXRXDELTAMAX / 10 + 1;

#define TXRXDELTA	(txrxdelta)
#define PING_TIMEOUT	(timeout)

#ifdef TARGET_F407
#define PS1 "uCAN-F407" __VERSION "> "
#define LED_GPIO GPIOD
#define LED_PIN GPIO_Pin_15
__ALIGN_BEGIN  USB_OTG_CORE_HANDLE  USB_OTG_dev  __ALIGN_END;
#endif

#ifdef TARGET_F091
#define PS1 "uCAN-F091" __VERSION "> "
#define LED_GPIO GPIOA
#define LED_PIN GPIO_Pin_5
#endif

void task_chat(void* vpars);
void task_blink(void* vpars);
void task_can(void *vpars);

int main(void)
{
	GPIO_InitTypeDef sGPIOinit;
#ifdef TARGET_F091
	USART_InitTypeDef usart_init = {
		.USART_BaudRate = 115200,
		.USART_WordLength = USART_WordLength_8b,
		.USART_Parity = USART_Parity_No,
		.USART_StopBits = USART_StopBits_1,
		.USART_HardwareFlowControl = USART_HardwareFlowControl_None,
		.USART_Mode = USART_Mode_Tx | USART_Mode_Rx
	};
#endif /* TARGET_F091 */

#ifdef TARGET_F407
	/* Required by the FreeRTOS */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	led_init(&f4d_led_blue);
	led_init(&f4d_led_orange);
	led_init(&f4d_led_green);
#endif /* TARGET_F407 */

#ifdef TARGET_F091
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	sGPIOinit.GPIO_Pin = LED_PIN;
	sGPIOinit.GPIO_Mode = GPIO_Mode_OUT;
	sGPIOinit.GPIO_Speed = GPIO_Speed_Level_2;
	sGPIOinit.GPIO_OType = GPIO_OType_PP;
#endif /* TARGET_F091 */
	GPIO_Init(LED_GPIO, &sGPIOinit);

#ifdef TARGET_F407
	USBD_Init(&USB_OTG_dev,
#ifdef USE_USB_OTG_HS
		USB_OTG_HS_CORE_ID,
#else
		USB_OTG_FS_CORE_ID,
#endif
		&USR_desc,
		&USBD_CDC_cb,
		&USR_cb);
#endif /* TARGET_F407 */
#ifdef TARGET_F091
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);

	sGPIOinit.GPIO_Mode = GPIO_Mode_AF;
	sGPIOinit.GPIO_Speed = GPIO_Speed_Level_2;
	sGPIOinit.GPIO_OType = GPIO_OType_PP;
	sGPIOinit.GPIO_PuPd = GPIO_PuPd_NOPULL;
	sGPIOinit.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOA, &sGPIOinit);

	sGPIOinit.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &sGPIOinit);

	USART_Init(USART2, &usart_init);

	USART_Cmd(USART2, ENABLE);
#endif /* TARGET_F091 */

	/* disable stdio buffering */
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	printf("\r\n\nuCAN" __VERSION "\r\n\n");

	can_init();

	xTaskCreate(task_blink, "blink", 100, NULL,
		    tskIDLE_PRIORITY + 1, NULL);

	xTaskCreate(task_can, "task_can", 1000, NULL,
		    tskIDLE_PRIORITY + 3, NULL);

	xTaskCreate(task_chat, "task_chat", 1000, NULL,
		    tskIDLE_PRIORITY + 1, NULL);
	vTaskStartScheduler();
}

static inline void ping_add_pending(uint32_t x)
{
	int i = 0;

	while (i < TXRXDELTA && ping_pending[i])
		i++;
	if (i < TXRXDELTA) {
		ping_pending[i] = x;
		if (ping_trace)
			printf("s %08x\r\n", x);
	}
}

static inline int ping_remove_pending(uint32_t x)
{
	int i = 0;
	uint32_t max = 0;

	while (i < TXRXDELTA && ping_pending[i] != x) {
		if (ping_pending[i] && max < ping_pending[i])
			max = ping_pending[i];
		i++;
	}
	if (i < TXRXDELTA) {
		ping_pending[i] = 0;
		if (ping_trace)
			printf("a %08x\r\n", x);
		return 1;
	}
	if (ping_trace)
		printf("orphaned: %08x max: %08x\r\n", x, max);
	return 0;
}

static void can_ping(int id, int count)
{
	TickType_t tim, timp;
	struct can_msg msg;
	int pending, max = 0, i, ping_tout = 0;

	msg.type = CAN_MSG_PING;
	msg.sender = can_id;

	tim = xTaskGetTickCount();

	ping_tx = 0;
	ping_rx = 0;
	ping_pending_count = 0;
	memset(ping_pending, 0, sizeof(ping_pending));
	for (i = 0; i < count; i++) {
		timp = xTaskGetTickCount();
		while (1) {
			taskDISABLE_INTERRUPTS();
			pending = ping_pending_count;
			taskENABLE_INTERRUPTS();
			if (pending < TXRXDELTA)
				break;
			if (xTaskGetTickCount() - timp > PING_TIMEOUT) {
				taskDISABLE_INTERRUPTS();
				memset(ping_pending, 0, sizeof(ping_pending));
				ping_pending_count = 0;
				ping_tout += 1;
				taskENABLE_INTERRUPTS();
				break;
			}
			vTaskDelay(1);
		}
		msg.data = i + 1;
		taskDISABLE_INTERRUPTS();
		ping_add_pending(msg.data);
		ping_pending_count += 1;
		if (ping_pending_count > max)
			max = ping_pending_count;
		taskENABLE_INTERRUPTS();
		can_xmit(id, &msg, sizeof(msg));
		ping_tx += 1;
	}

	timp = xTaskGetTickCount();
	taskDISABLE_INTERRUPTS();
	pending = ping_pending_count;
	taskENABLE_INTERRUPTS();
	while (pending && (xTaskGetTickCount() - timp < PING_TIMEOUT)) {
		vTaskDelay(1);
		taskDISABLE_INTERRUPTS();
		pending = ping_pending_count;
		taskENABLE_INTERRUPTS();
	}

	tim = xTaskGetTickCount() - tim;
	printf("Max pending: %d\r\n", max);
	printf("# timeouts: %d\r\n", ping_tout);
	printf("TX: %d\r\n", ping_tx);
	printf("RX: %d\r\n", ping_rx);
	printf("Time: %d ms\r\n", tim);
}

void task_chat(void *vpars)
{
#define CMD_LEN 255
	int c;
	char cmd[CMD_LEN+1];
	int pos = 0;
	char *tk;

	printf(PS1);

	while (1) {
		fflush(stdout);
		c = getchar();
		putchar(c);
		if (c == '\b') {
			if (pos > 0)
				pos--;
			continue;
		}
		if (c == '\r') {
			int i;

			printf("\r\n");
			/* parse cmd */
			cmd[pos] = 0;
			tk = strtok(cmd, " ");
			if (!tk || strlen(tk) == 0)
				goto cmd_finish;
			if (strcmp(tk, "stop") == 0) {
				for (i = 0; i < CAN_NUM_MB; i++)
					CAN_CancelTransmit(CANx, i);
			} else if (strcmp(tk, "udelay") == 0) {
				TickType_t tim;
				int cnt, del;

				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				del = strtol(tk, NULL, 10);
				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				cnt = strtol(tk, NULL, 10);
				tim = xTaskGetTickCount();
				while (cnt--)
					udelay(del);
				tim = xTaskGetTickCount() - tim;
				printf("time elapsed: %d ms\r\n", tim);
			} else if (strcmp(tk, "sleep") == 0) {
				int t;

				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				t = strtol(tk, NULL, 10);
				vTaskDelay(t);
			} else if (strcmp(tk, "dump") == 0) {
				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				if (strcmp(tk, "on") == 0)
					can_dump_pkt(1);
				else if (strcmp(tk, "off") == 0)
					can_dump_pkt(0);
				else
					goto cmd_error;
			} else if (strcmp(tk, "xstat") == 0) {
				can_dump_tx();
			} else if (strcmp(tk, "stat") == 0) {
				tk = strtok(NULL, " ");
				if (tk != NULL && strcmp(tk, "reset") == 0)
					can_stat_reset();
				can_stat_dump();
			} else if (strcmp(tk, "ping") == 0) {
				int id, count;

				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				id = strtol(tk, NULL, 0x10);

				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				count = strtol(tk, NULL, 10);

				tk = strtok(NULL, " ");
				if (tk == NULL)
					ping_trace = 0;
				else
					ping_trace = 1;

				can_ping(id, count);
			} else if (strcmp(tk, "send") == 0) {
				unsigned int id, len;
				unsigned char data[8];

				/* Transmit Structure preparation */
				tk = strtok(NULL, " ");
				if (tk == NULL)
					goto cmd_error;
				id = strtol(tk, NULL, 0x10);
				printf("addr: %03x\r\n", id);
				i = 0;
				do {
					tk = strtok(NULL, " ");
					if (tk == NULL)
						break;
					data[i] = strtol(tk, NULL, 0x10);
					i++;
				} while (i < 8);
				if (i == 0)
					goto cmd_error;
				printf("payload:");
				len = i;
				for (i = 0; i < len; i++)
					printf(" %02x", data[i]);
				printf("\r\n");

				can_xmit(id, data, len);
			} else if (strcmp(tk, "addr") == 0) {
				unsigned int id;

				tk = strtok(NULL, " ");
				if (tk == NULL) {
					printf("0x%08x\r\n", can_id);
					goto cmd_finish;
				};
				id = strtoul(tk, NULL, 0x10);
				can_filter_setup(id, 0x7ff);
			} else if (strcmp(tk, "txrxdelta") == 0) {
				unsigned int id;

				tk = strtok(NULL, " ");
				if (tk == NULL) {
					printf("%d\r\n", txrxdelta);
					goto cmd_finish;
				};
				txrxdelta = strtoul(tk, NULL, 10);
				if (txrxdelta < 1)
					txrxdelta = 1;
				if (txrxdelta > TXRXDELTAMAX)
					txrxdelta = TXRXDELTAMAX;
			} else if (strcmp(tk, "timeout") == 0) {
				unsigned int id;

				tk = strtok(NULL, " ");
				if (tk == NULL) {
					printf("%d\r\n", timeout);
					goto cmd_finish;
				};
				timeout = strtoul(tk, NULL, 10);
				if (timeout < 1)
					timeout = 1;
			} else {
				printf("unknown command `%s`\r\n", tk);
			}
			goto cmd_finish;
cmd_error:
			printf("can't parse command\r\n");
cmd_finish:
			printf(PS1);
			pos = 0;
			continue;
		}
		if (pos < CMD_LEN) {
			cmd[pos] = c;
			pos++;
		}
	}
}

void task_blink(void *vpars)
{
#ifdef TARGET_F407
	led_init(&f4d_led_blue);
	led_init(&f4d_led_orange);
	led_init(&f4d_led_green);
	while (1) {
		led_toggle(&f4d_led_blue);
		led_off(&f4d_led_orange);
		led_off(&f4d_led_green);
		vTaskDelay(100);
	}
#endif /* TARGET_F407 */

#ifdef TARGET_F091
	while (1) {
		GPIO_SetBits(LED_GPIO, LED_PIN);
		vTaskDelay(100);
		GPIO_ResetBits(LED_GPIO, LED_PIN);
		vTaskDelay(100);
	}
#endif /* TARGET_F091 */
}

void task_can(void *vpars)
{
	struct can_msg msg;
	unsigned int to;
	int len, ret;

	while (1) {
		len = can_recv(&msg);
		if (len == sizeof(msg) &&
		    msg.type == CAN_MSG_PING) {
			if (msg.sender != 0) {
				to = msg.sender;
				msg.sender = 0;
				can_xmit(to, &msg, len);
			} else {
				taskDISABLE_INTERRUPTS();
				if (ping_pending_count) {
					ret = ping_remove_pending(msg.data);
					ping_rx += ret;
					ping_pending_count -= ret;
				}
				taskENABLE_INTERRUPTS();
			}
		}
	}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
	while (1);
}
#endif
