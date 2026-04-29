#include "main.h"
#include "stm32f10x.h"
#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "usart2.h"

// ????:MiMo Token??????(?????????)
void MimoToken_HandleResponse(u8 *buf, u16 len);

// ??/?????(????,????????)
u8 USART2_RX_BUF[USART2_MAX_RX_LEN];
u8 USART2_TX_BUF[USART2_MAX_TX_LEN];
// ????:?15?=????,???=??????(0x8000)
volatile u16 USART2_RX_STA = 0;
u8 u2_cmd;

/**
 * @brief  USART2???(ESP8266??)
 * @param  bound ???(?9600)
 */
void usart2_init(u32 bound)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // ????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    USART_DeInit(USART2); // ??USART2

    // USART2_TX -> PA2(??????)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART2_RX -> PA3(????)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ????(??????????)
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // ??????
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); // ??????
    USART_ITConfig(USART2, USART_IT_IDLE, ENABLE); // ??????(?????)
    USART_Cmd(USART2, ENABLE);                     // ??USART2

    u2_printf("USART2 Init OK (ESP8266) ...\r\n");
}

/**
 * @brief  USART2??????(??+??????)
 */
void USART2_IRQHandler(void)
{
    // ????:????????
    if (USART_GetITStatus(USART2, USART_IT_IDLE) != RESET)
    {
        USART_ReceiveData(USART2); // ????????
        USART2_RX_STA_SET();       // ????????????
    }

    // ??????:??????
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) == SET)
    {
        USART_ClearFlag(USART2, USART_FLAG_ORE);
        USART_ReceiveData(USART2);
    }

    // ????:???????????
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        u2_cmd = USART_ReceiveData(USART2);
        // ???????
        if ((USART2_RX_STA & 0x7FFF) < USART2_MAX_RX_LEN)
        {
            USART2_RX_BUF[USART2_RX_STA & 0x7FFF] = u2_cmd;
            USART2_RX_STA++; // ????+1
        }
        // gizPutData(&u2_cmd, 1); // ?????????,????
    }
}

/**
 * @brief  USART2???????(?u3_printf????)
 * @param  fmt ??????
 */
void u2_printf(char *fmt, ...)
{
    u16 i = 0;
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    // ????????????(+1 ???????)
    vsnprintf((char *)USART2_TX_BUF, USART2_MAX_TX_LEN + 1, fmt, arg_ptr);
    va_end(arg_ptr);

    // ?????,?????????
    while ((i < USART2_MAX_TX_LEN) && USART2_TX_BUF[i])
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET); // ????????
        USART_SendData(USART2, (u8)USART2_TX_BUF[i++]);
        while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET); // ??????
    }
}

/**
 * @brief  ??USART2????,???MiMo Token??
 */
void USART2_RX_STA_SET(void)
{
    // ??\r???,?????????
    if ((USART2_RX_STA & 0x7FFF) >= 2 && USART2_RX_BUF[(USART2_RX_STA & 0x7FFF) - 2] == '\r')
    {
        USART2_RX_BUF[(USART2_RX_STA & 0x7FFF) - 2] = 0;
        USART2_RX_STA -= 2; // ??\r???2???
    }
    else if ((USART2_RX_STA & 0x7FFF) > 0)
    {
        USART2_RX_BUF[USART2_RX_STA & 0x7FFF] = '\0';
    }

    USART2_RX_STA |= 0x8000; // ????????

    // ??MiMo Token??
    if (USART2_RX_STA & 0x8000)
    {
        MimoToken_HandleResponse(USART2_RX_BUF, USART2_RX_STA & 0x7FFF);
        USART2_RX_STA = 0; // ??????,???????
    }
}