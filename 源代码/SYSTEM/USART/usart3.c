#include "stm32f10x.h"
#include "misc.h"
#include "stm32f10x_usart.h"
#include "usart3.h"

// 前置声明：MiMo Token响应解析函数（需自行实现具体逻辑）
void MimoToken_HandleResponse(u8 *buf, u16 len);

// 接收/发送缓冲区（全局变量，与头文件声明对应）
u8 USART3_RX_BUF[USART3_MAX_RX_LEN];
u8 USART3_TX_BUF[USART3_MAX_TX_LEN];
// 接收状态：低15位=接收长度，最高位=接收完成标志(0x8000)
volatile u16 USART3_RX_STA = 0;
u8 u3_cmd;

/**
 * @brief  USART3初始化（HC-05蓝牙通信）
 * @param  bound 波特率（如9600）
 */
void usart3_init(u32 bound)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    USART_DeInit(USART3); // 复位USART3

    // USART3_TX -> PB10（推挽复用输出）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // USART3_RX -> PB11（浮空输入）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 中断配置（优先级可根据系统调整）
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2; // 与USART2区分优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 串口参数配置
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART3, &USART_InitStructure);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 使能接收中断
    USART_ITConfig(USART3, USART_IT_IDLE, ENABLE); // 使能空闲中断（帧接收完成）
    USART_Cmd(USART3, ENABLE);                     // 使能USART3

    u3_printf("USART3 Init OK (HC-05) ...\r\n");
}

/**
 * @brief  USART3中断服务函数（接收+空闲中断处理）
 */
void USART3_IRQHandler(void)
{
    // 空闲中断：一帧数据接收完成
    if (USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        USART_ReceiveData(USART3); // 清除空闲中断标志
        USART3_RX_STA_SET();       // 标记接收完成并解析响应
    }

    // 溢出标志处理：防止数据丢失
    if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) == SET)
    {
        USART_ClearFlag(USART3, USART_FLAG_ORE);
        USART_ReceiveData(USART3);
    }

    // 接收中断：逐字节存储数据到缓冲区
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        u3_cmd = USART_ReceiveData(USART3);
        // 缓冲区溢出保护
        if ((USART3_RX_STA & 0x7FFF) < USART3_MAX_RX_LEN)
        {
            USART3_RX_BUF[USART3_RX_STA & 0x7FFF] = u3_cmd;
            USART3_RX_STA++; // 接收长度+1
        }
    }
}

/**
 * @brief  USART3格式化打印函数
 * @param  fmt 格式化字符串
 */
void u3_printf(char *fmt, ...)
{
    u16 i = 0;
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    // 格式化字符串到发送缓冲区（+1 预留终止符空间）
    vsnprintf((char *)USART3_TX_BUF, USART3_MAX_TX_LEN + 1, fmt, arg_ptr);
    va_end(arg_ptr);

    // 逐字节发送，确保不超缓冲区长度
    while ((i < USART3_MAX_TX_LEN) && USART3_TX_BUF[i])
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET); // 等待发送寄存器空
        USART_SendData(USART3, (u8)USART3_TX_BUF[i++]);
        while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET); // 等待发送完成
    }
}

/**
 * @brief  标记USART3接收完成，并解析MiMo Token响应
 */
void USART3_RX_STA_SET(void)
{
    // 处理\r换行符，替换为字符串终止符
    if ((USART3_RX_STA & 0x7FFF) >= 2 && USART3_RX_BUF[(USART3_RX_STA & 0x7FFF) - 2] == '\r')
    {
        USART3_RX_BUF[(USART3_RX_STA & 0x7FFF) - 2] = 0;
        USART3_RX_STA -= 2; // 移除\r占用的2个字节
    }
    else if ((USART3_RX_STA & 0x7FFF) > 0)
    {
        USART3_RX_BUF[USART3_RX_STA & 0x7FFF] = '\0';
    }

    USART3_RX_STA |= 0x8000; // 置位接收完成标志

    // 解析MiMo Token响应
    if (USART3_RX_STA & 0x8000)
    {
        MimoToken_HandleResponse(USART3_RX_BUF, USART3_RX_STA & 0x7FFF);
        USART3_RX_STA = 0; // 重置接收状态，准备下一次接收
    }
}