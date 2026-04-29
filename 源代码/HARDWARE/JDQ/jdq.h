#ifndef __JDQ_H
#define	__JDQ_H
#include "stm32f10x.h"
#include "delay.h"


/***************根据自己需求更改****************/
//继电器模块 GPIO宏定义


#define	JSQ_CLK							RCC_APB2Periph_GPIOB

#define JSQ_GPIO_PIN 				GPIO_Pin_0

#define JSQ_GPIO_PORT				GPIOB

#define JSQ_ON 							GPIO_SetBits(JSQ_GPIO_PORT,JSQ_GPIO_PIN)
#define JSQ_OFF 						GPIO_ResetBits(JSQ_GPIO_PORT,JSQ_GPIO_PIN)

#define jdq      						PBout(0)

/*********************END**********************/

void JDQ_Init(void);

#endif

