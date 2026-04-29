#ifndef	__MODULES_H_
#define __MODULES_H_

#include "stm32f10x.h"                  // Device header
#include "adc.h"


#define turbidity_K 2047.19					//浊度
////humi`: 湿度（单位：百分比，来自DHT11）
//- `temp`: 温度（单位：摄氏度，来自DHT11）
//- `lux`: 光照强度（单位：勒克斯，来自光敏电阻LDR）
//- `soilHumi`: 土壤湿度（单位可能是百分比，但代码中未实现，注释掉了）
//- `Smoge`: 烟雾浓度（单位：PPM，来自MQ2传感器）
//- `AQI`: 空气质量指数（单位：PPM，来自MQ135传感器）
//- `CO`: 一氧化碳浓度（单位：PPM，来自MQ7传感器）
//- `hPa`: 大气压强（单位：百帕，但代码中未实现）
typedef struct
{
	uint8_t humi;
	uint16_t temp;
	uint16_t lux;	
	uint16_t soilHumi;
	uint16_t Smoge;	
	uint16_t AQI;
	uint16_t CO;
	uint16_t hPa;
	uint16_t TDS;	//电导率
	uint16_t TS;	//浊度
	float PH;	//PH
}SensorModules;

typedef struct
{
	uint8_t humiValue;
	uint16_t tempValue;//温度
	uint16_t luxValue;	
	uint16_t soilHumiValue;
	uint16_t COValue;	
	uint16_t AQIValue;
	uint16_t hPaValue;
	uint16_t SmogeValue;
	uint32_t TDSValue;//电导率
	uint32_t NTUValue;//浊度
	uint16_t PHValue_H;//PH上线
	uint16_t PHValue_L;//PH下限
}SensorThresholdValue;

typedef struct
{
	uint8_t LED_Flag;
	uint8_t NOW_Curtain_Flag;
	uint8_t Curtain_Flag;	
	uint8_t NOW_Window_Flag;
	uint8_t Window_Flag;	
	uint8_t Fan_Flag;
	uint8_t Humidifier_Flag;
	uint8_t Bump_Flag;
	uint8_t Jdq_Flag;
	uint8_t Beep_Flag;
	
}DriveModules;
 	

extern SensorModules sensorData;			//声明传感器模块的结构体变量
extern SensorThresholdValue Sensorthreshold;	//声明传感器阈值结构体变量
extern DriveModules driveData;				//声明驱动器状态的结构体变量
void SensorScan(void);

#endif
