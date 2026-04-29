#ifndef __MAIN_H
#define	__MAIN_H

#include <stdio.h>
#include "stm32f10x.h"
#include "delay.h"
#include "stdarg.h"	 	  	 
#include "string.h"	
#include "timer.h"
#include "flash.h"

#include "usart2.h"
//#include "usart3.h"
#include "adc.h"
#include "gizwits_product.h" 
#include "ds18b20.h"
//#include "dht11.h" 


//#include "gizwits_product.h"
//#include "gizwits_protocol.h"
//#include <stdint.h>
//#include <stdbool.h>
//#include <stdlib.h>

#define  U8  u8
#define  U16  u16

extern u8 b_1s,eer_f,smart_config;
extern u32 set_code[21];//设置参数


extern u8 b_1s,eer_f,water_pump_f,medicinal_f,heat_f,smart_config;;
extern u32 set_code[21];//设置参数
extern u8 run_mod;//工作模式


extern u16 User_ID,integral;
extern u8 mod;//显示模式


#endif



