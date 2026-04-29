#include "beep.h"
#include "Modules.h"
#include "stm32f10x.h"                  // Device header
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
void BEEP_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(BEEP_CLK, ENABLE ); //≈‰÷√ ±÷”
	
	GPIO_InitStructure.GPIO_Pin = BEEP_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(BEEP_GPIO_PROT,&GPIO_InitStructure);
	//GPIO_ResetBits(BEEP_GPIO_PROT,BEEP_GPIO_PIN); 	
	BEEP_OFF;
}

void toggleBEEP(void)
{

	if(driveData.Beep_Flag)
	{
		driveData.Beep_Flag=0;
	}
	else{
		driveData.Beep_Flag=1;
	}
}
