#include "main.h"
#include "beep.h"
#include "key.h"
#include "stm32f10x.h"                  // Device header
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_tim.h"
#include "misc.h"

// ??:????????(Token????)
u32 sys_sec = 0;
u32 frequence = 0;//???
u16 num = 0;//???????

// ... ?????? ...

// ??:TIM4?????(Token????)
void TIM4_Int_Init(u16 arr,u16 psc)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); // ??TIM4??

	TIM_TimeBaseStructure.TIM_Period = arr; // ?????
	TIM_TimeBaseStructure.TIM_Prescaler = psc; // ?????
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; // ????
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // ????
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

	TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE ); // ??????

	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn; // TIM4????
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; // ?????0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2; // ????2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // ????
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(TIM4, ENABLE); // ??TIM4
}

// ??:TIM4??????(??????)
void TIM4_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) // ????????
	{
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update); // ??????
		sys_sec++; // ??????(??Token????)
	}
}

// ... ?????? ...