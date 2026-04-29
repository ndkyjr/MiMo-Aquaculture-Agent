#include <stdio.h>
#include "stm32f10x.h"
#include "delay.h"
#include "stdarg.h"	 	  	 
#include "string.h"	
#include "timer.h"
#include "flash.h"
#include "usart2.h"
#include "usart3.h"  // 蓝牙通信头文件
#include "adc.h"
#include "ds18b20.h"
#include "beep.h"
#include "Modules.h"
#include "jdq.h"
#include "key.h"
#include "main.h"
#include "oled.h"
#include "misc.h"

/****************�췽�����ӹ�����*******************
											STM32
											
*��Ŀ 		:    STM32����ˮ�ʼ��(������)
*�汾			:    V1.0
*MCU			:    STM32F103C8T6
*�ӿ�			:    ������
*BILIBILI	:    �췽������
*С����		:    �췽������
*CSDN			:    �췽������
*��ȨIP		:    ���絥Ƭ����ơ��췽����YFC���ӡ�������Ƭ�����
**********************BEGIN***********************/

// ====================== 新增：MiMo Token 相关定义 ======================
#define MIMO_TOKEN_BUFFER_SIZE 256  // Token通信缓冲区大小
#define MIMO_FLASH_TOKEN_ADDR 0x0801F010  // Token存储Flash地址（避开原有阈值存储区）
#define MIMO_DEVICE_ID "WATER_MONITOR_001"  // 设备唯一标识（需替换为你的设备ID）
#define MIMO_DEVICE_SECRET "DEVICE_SECRET_123456"  // 设备密钥（需替换为你的密钥）
#define MIMO_TOKEN_VALID_TIME 3600  // Token有效期（秒），示例1小时

// MiMo Token 申请状态枚举
typedef enum {
    MIMO_TOKEN_IDLE = 0,        // 空闲
    MIMO_TOKEN_REQUESTING,      // 申请中
    MIMO_TOKEN_VALID,           // Token有效
    MIMO_TOKEN_EXPIRED,         // Token过期
    MIMO_TOKEN_ERROR            // 申请失败
} MimoTokenState;

// MiMo Token 数据结构体
typedef struct {
    char token[64];             // Token字符串
    uint32_t createTime;        // Token创建时间（系统秒数）
    MimoTokenState state;       // Token状态
} MimoTokenInfo;

MimoTokenInfo g_mimoToken;      // 全局Token信息
uint32_t g_sysTickSec = 0;      // 系统运行秒数（用于Token过期判断）
// ====================== MiMo Token 定义结束 ======================

#define turbidity_K 2047.19
#define WS_DATA_BUFFER_SIZE 100  // WebSocket数据缓冲区大小
#define BT_DATA_BUFFER_SIZE 100  // 蓝牙数据缓冲区大小
void Key_Init(void);//定义按键初始化
void display(void);

// 发送JSON格式数据到HC-05蓝牙模块
void SendBluetoothData(float temp, float ph, float turbidity)
{
    char jsonBuffer[BT_DATA_BUFFER_SIZE];
    snprintf(jsonBuffer, BT_DATA_BUFFER_SIZE,
             "{\"temperature\":%.1f,\"ph\":%.2f,\"turbidity\":%.1f}",
             temp, ph, turbidity);
    u3_printf("%s\r\n", jsonBuffer);
}

// 发送JSON格式数据到ESP8266（通过WebSocket转发给上位机）- 恢复用于MiMo通信
void SendWebSocketData(float temp, float ph, float turbidity)
{
    char jsonBuffer[WS_DATA_BUFFER_SIZE];
    snprintf(jsonBuffer, WS_DATA_BUFFER_SIZE,
             "{\"temperature\":%.1f,\"ph\":%.2f,\"turbidity\":%.1f}",
             temp, ph, turbidity);
    u2_printf("%s\r\n", jsonBuffer);
}

// ====================== 新增：MiMo Token 核心函数 ======================
/**
 * @brief  加载Flash中存储的MiMo Token
 * @param  无
 * @retval 无
 */
void MimoToken_LoadFromFlash(void)
{
    // 从Flash读取Token信息
    FLASH_ReadBuffer(MIMO_FLASH_TOKEN_ADDR, (uint8_t*)&g_mimoToken, sizeof(MimoTokenInfo));
    
    // 校验Token有效性（判断是否过期）
    if (g_mimoToken.state == MIMO_TOKEN_VALID) {
        if ((g_sysTickSec - g_mimoToken.createTime) >= MIMO_TOKEN_VALID_TIME) {
            g_mimoToken.state = MIMO_TOKEN_EXPIRED;  // Token过期
        }
    } else {
        // Flash中无有效Token，初始化状态
        memset(&g_mimoToken, 0, sizeof(MimoTokenInfo));
        g_mimoToken.state = MIMO_TOKEN_IDLE;
    }
}

/**
 * @brief  保存MiMo Token到Flash
 * @param  无
 * @retval 无
 */
void MimoToken_SaveToFlash(void)
{
    FLASH_WriteBuffer(MIMO_FLASH_TOKEN_ADDR, (uint8_t*)&g_mimoToken, sizeof(MimoTokenInfo));
}

/**
 * @brief  构造MiMo Token申请请求
 * @param  buffer: 存储请求的缓冲区
 * @param  bufSize: 缓冲区大小
 * @retval 无
 */
void MimoToken_BuildRequest(char* buffer, uint16_t bufSize)
{
    // 构造Token申请JSON请求（适配MiMo平台接口格式）
    snprintf(buffer, bufSize,
             "{\"cmd\":\"apply_token\",\"device_id\":\"%s\",\"device_secret\":\"%s\",\"timestamp\":%lu}",
             MIMO_DEVICE_ID, MIMO_DEVICE_SECRET, g_sysTickSec);
}

/**
 * @brief  解析MiMo Token响应
 * @param  response: 服务器响应数据
 * @retval 0:成功 -1:失败
 */
int MimoToken_ParseResponse(const char* response)
{
    // 示例响应格式：{"code":0,"msg":"success","token":"xxxxxx"}
    char tokenStr[64] = {0};
    int code = -1;
    
    // 简易JSON解析（实际项目建议用JSON库）
    if (strstr(response, "\"code\":0") != NULL && strstr(response, "\"token\":\"") != NULL) {
        // 提取Token字符串
        const char* tokenStart = strstr(response, "\"token\":\"") + 8;
        const char* tokenEnd = strstr(tokenStart, "\"");
        if (tokenStart && tokenEnd && (tokenEnd - tokenStart) < 64) {
            strncpy(g_mimoToken.token, tokenStart, tokenEnd - tokenStart);
            g_mimoToken.createTime = g_sysTickSec;
            g_mimoToken.state = MIMO_TOKEN_VALID;
            MimoToken_SaveToFlash();  // 保存到Flash
            return 0;
        }
    }
    
    // 解析失败
    g_mimoToken.state = MIMO_TOKEN_ERROR;
    return -1;
}

/**
 * @brief  申请MiMo Token（主函数）
 * @param  useBluetooth: 1-通过蓝牙(USART3) 0-通过WiFi(USART2)
 * @retval 0:申请中 -1:失败
 */
int MimoToken_Apply(uint8_t useBluetooth)
{
    if (g_mimoToken.state == MIMO_TOKEN_REQUESTING) {
        return 0;  // 已有申请在进行
    }
    
    char requestBuf[MIMO_TOKEN_BUFFER_SIZE] = {0};
    MimoToken_BuildRequest(requestBuf, MIMO_TOKEN_BUFFER_SIZE);
    
    // 发送申请请求
    g_mimoToken.state = MIMO_TOKEN_REQUESTING;
    if (useBluetooth) {
        u3_printf("%s\r\n", requestBuf);  // 蓝牙发送
    } else {
        u2_printf("%s\r\n", requestBuf);  // WiFi发送
    }
    
    // 等待响应（实际需在串口中断中处理，此处简化）
    delay_ms(1000);
    return 0;
}

/**
 * @brief  检测MiMo Token有效性，过期则重申请
 * @param  无
 * @retval 无
 */
void MimoToken_CheckAndReapply(void)
{
    switch (g_mimoToken.state) {
        case MIMO_TOKEN_EXPIRED:
        case MIMO_TOKEN_IDLE:
        case MIMO_TOKEN_ERROR:
            // 优先用WiFi申请，失败则用蓝牙
            if (wifiConfigMode == 0) {
                MimoToken_Apply(0);  // WiFi申请
            } else {
                MimoToken_Apply(1);  // 蓝牙申请
            }
            break;
        case MIMO_TOKEN_VALID:
            // 检测是否过期
            if ((g_sysTickSec - g_mimoToken.createTime) >= MIMO_TOKEN_VALID_TIME) {
                g_mimoToken.state = MIMO_TOKEN_EXPIRED;
            }
            break;
        default:
            break;
    }
}

/**
 * @brief  串口接收MiMo Token响应（需在USART2/3中断中调用）
 * @param  data: 接收的数据
 * @param  len: 数据长度
 * @retval 无
 */
void MimoToken_HandleResponse(const char* data, uint16_t len)
{
    if (g_mimoToken.state != MIMO_TOKEN_REQUESTING) {
        return;
    }
    
    char responseBuf[MIMO_TOKEN_BUFFER_SIZE] = {0};
    strncpy(responseBuf, data, len);
    MimoToken_ParseResponse(responseBuf);
}
// ====================== MiMo Token 函数结束 ======================

u8 eer_f;
float TDS_DAT;					
float T;																		
u8 b_1s;
extern uint8_t valueFlashFlag;
u8 smart_config;

#define KEY_Long1	11
#define KEY_1	1
#define KEY_2	2
#define KEY_3	3
#define KEY_4	4

#define FLASH_START_ADDR	0x0801f000	//д�����ʼ��ַ

SensorModules sensorData;								//�������������ݽṹ�����
SensorThresholdValue Sensorthreshold;		//������������ֵ�ṹ�����
DriveModules driveData;									//����������״̬�ṹ�����

uint8_t mode = 1;	//ϵͳģʽ  1�Զ�  2�ֶ�  3����

//ϵͳ��̬����
static uint8_t count_a = 1;  //�Զ�ģʽ������
static uint8_t count_m = 1;  //�ֶ�ģʽ������
static uint8_t count_s = 1;	 //����ģʽ������

uint8_t wifiConfigMode = 0;  // 配网模式标志 0=正常, 1=配网中

unsigned char p[16]=" ";		//��ʾˮ�����ݻ�����
unsigned char t[16]=" ";		//��ʾTDS���ݻ�����
unsigned char z[16]=" ";		//��ʾ�Ƕ����ݻ�����
unsigned char h[16]=" ";		//��ʾPH���ݻ�����

//��ʾ�˵�����
enum 
{
	AUTO_MODE = 1,
	MANUAL_MODE,
	SETTINGS_MODE
}MODE_PAGES;	


void WIFI_Contection(u8 key)//WiFi连接控制
{
	// 注释：机智云连接控制已移除，ESP8266使用自定义WebSocket固件
	if(key==4)
	{
		// 新增：按键4触发MiMo Token重新申请
		MimoToken_Apply(0); // WiFi申请Token
	}
	if(key==3)
	{
		// 新增：按键3触发蓝牙申请Token
		MimoToken_Apply(1); // 蓝牙申请Token
	}
}

//Э���ʼ��
void Gizwits_Init(void)
{
	// 替换为简单的系统初始化
	TIM3_Int_Init(1000, 7199); // 100ms定时器（用于数据采集定时）
}

/**
  * @brief  ��¼�Զ�ģʽ�����°�KEY2�Ĵ���
  * @param  ��
  * @retval ���ش���
  */
uint8_t SetAuto(void)  
{
	if(KeyNum == KEY_2)
	{
		KeyNum = 0;
		count_a++;
		if (count_a > 2)
		{
			count_a = 1;
		}
		OLED_Clear();
	}
	return count_a;
}

/**
  * @brief  ��ʾ�˵�1�Ĺ̶�����
  * @param  ��
  * @retval ��
  */
void OLED_autoPage1(void)		//�Զ�ģʽ�˵���һҳ
{
	//��ʾ��ˮ�£���
	OLED_ShowChinese(0,0,38,16,1); 	//ˮ
	OLED_ShowChinese(16,0,0,16,1);	//��
	OLED_ShowChar(32,0,':',16,1);
	OLED_ShowChar(80,0,'C',16,1);
	
	//��ʾ��TDS����
	OLED_ShowChar(0,16,'T',16,1);	//T
	OLED_ShowChar(8,16,'D',16,1);//D
	OLED_ShowChar(16,16,'S',16,1);	//S
	OLED_ShowChar(32,16,':',16,1);
	OLED_ShowChar(80,16,'p',16,1);
	OLED_ShowChar(88,16,'p',16,1);
	OLED_ShowChar(96,16,'m',16,1);
	
	//��ʾ���Ƕȣ���
	OLED_ShowChinese(0,32,50,16,1); //��
	OLED_ShowChinese(16,32,2,16,1);//��
	OLED_ShowChar(32,32,':',16,1);
	
	//��ʾ��PH����
	OLED_ShowChar(0,48,'P',16,1);	//P
	OLED_ShowChar(16,48,'H',16,1);//H
	OLED_ShowChar(32,48,':',16,1);

    // 新增：显示MiMo Token状态
    if (g_mimoToken.state == MIMO_TOKEN_VALID) {
        OLED_ShowChar(112, 0, 'T', 16, 1); // T表示Token有效
    } else {
        OLED_ShowChar(112, 0, 'F', 16, 1); // F表示Token无效
    }
    
	OLED_Refresh();
}

/**
  * @brief  ��ʾ�ֶ�ģʽ�����ѡ�����
  * @param  num Ϊ��ʾ��λ��
  * @retval ��
  */
void OLED_manualOption(uint8_t num)
{
	switch(num)
	{
		case 1:	
			OLED_ShowChar(0, 0,'>',16,1);
			OLED_ShowChar(0,16,' ',16,1);
			OLED_ShowChar(0,32,' ',16,1);
			OLED_ShowChar(0,48,' ',16,1);
			break;
		case 2:	
			OLED_ShowChar(0, 0,' ',16,1);
			OLED_ShowChar(0,16,'>',16,1);
			OLED_ShowChar(0,32,' ',16,1);
			OLED_ShowChar(0,48,' ',16,1);
			break;
	}
}

/**
  * @brief  �ֶ�ģʽ���ƺ���
  * @param  ��
  * @retval ��
  */
void ManualControl(uint8_t num)
{
	switch(num)
	{
		case 1:	
			//��ʾ���迪��
			if(KeyNum == KEY_3)
				driveData.Jdq_Flag=1;
			if(KeyNum == KEY_4)
				driveData.Jdq_Flag=0;
			break;
		case 2:	
			if(KeyNum == KEY_3)
				driveData.Beep_Flag=1;
			if(KeyNum == KEY_4)
				driveData.Beep_Flag=0;
			break;
		default: break;
	}
}

/**
  * @brief  ��¼�ֶ�ģʽ�����°�KEY2�Ĵ���
  * @param  ��
  * @retval ���ش���
  */
uint8_t SetManual(void)  
{
	if(KeyNum == KEY_2)
	{
		KeyNum = 0;
		count_m++;
		if (count_m > 2)  		//һ�����Կ��Ƶ���������
		{
			count_m = 1;
		}
	}
	return count_m;
}

/**
  * @brief  ��ʾ�ֶ�ģʽ���ý���1
  * @param  ��
  * @retval ��
  */
void OLED_manualPage1(void)
{
	//��ʾ���̵�����
	OLED_ShowChinese(16,0,51,16,1);	
	OLED_ShowChinese(32,0,52,16,1);	
	OLED_ShowChinese(48,0,37,16,1);
	OLED_ShowChar(64,0,':',16,1);

	//��ʾ����������
	OLED_ShowChinese(16,16,53,16,1);	
	OLED_ShowChinese(32,16,54,16,1);
	OLED_ShowChinese(48,16,37,16,1);
	OLED_ShowChar(64,16,':',16,1);
}

/**
  * @brief  ��ʾ�ֶ�ģʽ���ò�������1
  * @param  ��
  * @retval ��
  */
void ManualSettingsDisplay1(void)
{
	if(driveData.Jdq_Flag)
	{
		OLED_ShowChinese(96,0,40,16,1); 	//��
	}
	else
	{
		OLED_ShowChinese(96,0,42,16,1); 	//��
	}
	
	if(driveData.Beep_Flag)
	{
		OLED_ShowChinese(96,16,40,16,1); 	//��
	}
	else
	{
		OLED_ShowChinese(96,16,42,16,1); 	//��
	}
}

/**
  * @brief  ��ʾ��ֵ�����ѡ�����
  * @param  num Ϊ��ʾ��λ��
  * @retval ��
  */
void OLED_settingsOption(uint8_t num)
{
	switch(num)
	{
		case 1:	
			OLED_ShowChar(0, 0,'>',16,1);
			OLED_ShowChar(0,16,' ',16,1);
			OLED_ShowChar(0,32,' ',16,1);
			OLED_ShowChar(0,48,' ',16,1);
			break;
		case 2:	
			OLED_ShowChar(0, 0,' ',16,1);
			OLED_ShowChar(0,16,'>',16,1);
			OLED_ShowChar(0,32,' ',16,1);
			OLED_ShowChar(0,48,' ',16,1);
			break;
		case 3:	
			OLED_ShowChar(0, 0,' ',16,1);
			OLED_ShowChar(0,16,' ',16,1);
			OLED_ShowChar(0,32,'>',16,1);
			OLED_ShowChar(0,48,' ',16,1);
			break;
		case 4:	
			OLED_ShowChar(0, 0,' ',16,1);
			OLED_ShowChar(0,16,' ',16,1);
			OLED_ShowChar(0,32,' ',16,1);
			OLED_ShowChar(0,48,'>',16,1);
			break;
		case 5:	
			OLED_ShowChar(0, 0,'>',16,1);
			OLED_ShowChar(0,16,' ',16,1);
			OLED_ShowChar(0,32,' ',16,1);
			OLED_ShowChar(0,48,' ',16,1);
			break;
		default: break;
	}
}

/**
  * @brief  ��ֵ���ú���
  * @param  ��
  * @retval ��
  */
void ThresholdSettings(uint8_t num)
{
	switch (num)
	{
		case 1:
			if (KeyNum == KEY_3)
			{
				KeyNum = 0;
				if(Sensorthreshold.tempValue<80)
				{
					Sensorthreshold.tempValue++;
				}
			}
			else if (KeyNum == KEY_4)
			{
				KeyNum = 0;
				if(Sensorthreshold.tempValue>0)
				{
					Sensorthreshold.tempValue--;
				}
			}
			break;
			
		case 2:
			if (KeyNum == KEY_3)
			{
				KeyNum = 0;
				if(Sensorthreshold.TDSValue<999)
				{
					Sensorthreshold.TDSValue+=10;
				}
			}
			else if (KeyNum == KEY_4)
			{
				KeyNum = 0;
				if(Sensorthreshold.TDSValue>0)
				{
					Sensorthreshold.TDSValue-=10;
				}
			}	
			break;
			
		case 3:
			if (KeyNum == KEY_3)
			{
				KeyNum = 0;
				if(Sensorthreshold.PHValue_H<14)
				{
					Sensorthreshold.PHValue_H++;
				}
			}
			else if (KeyNum == KEY_4)
			{
				KeyNum = 0;
				if(Sensorthreshold.PHValue_H>0)
				{
					Sensorthreshold.PHValue_H--;
				}
			}
			break;
			
		case 4:
			if (KeyNum == KEY_3)
			{
				KeyNum = 0;
				if(Sensorthreshold.PHValue_L<14)
				{
					Sensorthreshold.PHValue_L++;
				}
			}
			else if (KeyNum == KEY_4)
			{
				KeyNum = 0;
				if(Sensorthreshold.PHValue_L>0)
				{
					Sensorthreshold.PHValue_L--;
				}
			}
			break;
			
		case 5:
			if (KeyNum == KEY_3)
			{
				KeyNum = 0;
				if(Sensorthreshold.NTUValue<2000)
				{
					Sensorthreshold.NTUValue+=10;
				}
			}
			else if (KeyNum == KEY_4)
			{
				KeyNum = 0;
				if(Sensorthreshold.NTUValue>0)
				{
					Sensorthreshold.NTUValue-=10;
				}
			}
			break;
		default: break;		
	}
}

/**
  * @brief  ��¼��ֵ�����°�KEY2�Ĵ���
  * @param  ��
  * @retval ���ش���
  */
uint8_t SetSelection(void)
{
	if(KeyNum == KEY_2)
	{
		KeyNum = 0;
		count_s++;
		if (count_s == 5)
		{
			OLED_Clear();
		}
		else if (count_s > 5)
		{
			OLED_Clear();
			count_s = 1;
		}
	}
	return count_s;
}

/**
  * @brief  ��ʾϵͳ��ֵ���ý���1
  * @param  ��
  * @retval ��
  */
void OLED_settingsPage1(void)
{
	//��ʾ��ˮ�¡�
	OLED_ShowChinese(16,0,38,16,1);	
	OLED_ShowChinese(32,0,0,16,1);	
	OLED_ShowChar(48,0,':',16,1);

	//��ʾ��TDS��
	OLED_ShowChar(16,16,'T',16,1);	
	OLED_ShowChar(24,16,'D',16,1);	
	OLED_ShowChar(32,16,'S',16,1);	
	OLED_ShowChar(48,16,':',16,1);
	
	//��ʾ��PH_H��
	OLED_ShowChar(16,32,'P',16,1);	
	OLED_ShowChar(24,32,'H',16,1);	
	OLED_ShowChar(32,32,'_',16,1);	
	OLED_ShowChar(40,32,'H',16,1);	
	OLED_ShowChar(48,32,':',16,1);
	
	//��ʾ��PH_L��
	OLED_ShowChar(16,48,'P',16,1);	
	OLED_ShowChar(24,48,'H',16,1);	
	OLED_ShowChar(32,48,'_',16,1);	
	OLED_ShowChar(40,48,'L',16,1);	
	OLED_ShowChar(48,48,':',16,1);
}

void OLED_settingsPage2(void)
{
	//��ʾ���Ƕȣ���
	OLED_ShowChinese(16,0,50,16,1);	
	OLED_ShowChinese(32,0,2,16,1);	
	OLED_ShowChar(48,0,':',16,1);
}

void SettingsThresholdDisplay2(void)
{
	//��ʾ�Ƕ�
	OLED_ShowNum(90,0, Sensorthreshold.NTUValue, 3,16,1);
}

void SensorDataDisplay1(void)
{
	//��ʾˮ������
	sprintf((char*)p,"%4.1f",(float)sensorData.temp/10);
	OLED_ShowString(48,0,p ,16,1);
	//��ʾtds
	sprintf((char*)t,"%03d",(int)sensorData.TDS);
	OLED_ShowString(48,16,t ,16,1);
	//��ʾ�Ƕ�
	sprintf((char*)z,"%04d",(int)sensorData.TS);
	OLED_ShowString(48,32,z ,16,1);
	
	//��ʾPH
	sprintf((char*)h,"%4.1f",(float)sensorData.PH);
	OLED_ShowString(48,48,h ,16,1);
}

/**
  * @brief  ���ƺ���
  * @param  ��
  * @retval ��
  */
void Control_Manager(void)
{
	//�̵�������
	if(driveData.Jdq_Flag)
	{
		jdq=1;
	}
	else
	{
		jdq=0;
	}
	
	//����������
	if(driveData.Beep_Flag)
	{
		eer_f=10;
	}
	else
	{
		eer_f=0;
	}
}

/**
  * @brief  �Զ�ģʽ���ƺ���
  * @param  ��
  * @retval ��
  */
void AutoControl(void)
{	
	if(sensorData.TS>Sensorthreshold.NTUValue){
		driveData.Beep_Flag = 1;
		driveData.Jdq_Flag = 1;
	}	
	else if(sensorData.temp>Sensorthreshold.tempValue*10)
	{
		driveData.Jdq_Flag = 1;
		driveData.Beep_Flag = 1;
	}
	else if(sensorData.PH<Sensorthreshold.PHValue_L||sensorData.PH>Sensorthreshold.PHValue_H)
	{
		driveData.Beep_Flag = 1;
		driveData.Jdq_Flag = 1;
	}
	else if(sensorData.TDS>Sensorthreshold.TDSValue)
	{
		driveData.Jdq_Flag = 1;
		driveData.Beep_Flag = 1;
	}
	else{
		driveData.Beep_Flag = 0;
		driveData.Jdq_Flag = 0;
	}
}

void SettingsThresholdDisplay1(void)
{
	//��ʾˮ��
	OLED_ShowNum(90,0, Sensorthreshold.tempValue, 2,16,1);
	//��ʾTDS
	OLED_ShowNum(90,16, Sensorthreshold.TDSValue, 3,16,1);
		
	//��ʾPH_H
	OLED_ShowNum(90, 32, Sensorthreshold.PHValue_H, 2,16,1);
	//��ʾPH_L
	OLED_ShowNum(90, 48, Sensorthreshold.PHValue_L, 2,16,1);
}

// ====================== 新增：系统秒数定时器中断（用于Token过期） ======================
void TIM4_Int_Init(u16 arr, u16 psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); // 使能TIM4时钟

    TIM_TimeBaseStructure.TIM_Period = arr; // 设置自动重装值
    TIM_TimeBaseStructure.TIM_Prescaler = psc; // 设置预分频值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 设置时钟分割:TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数模式
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure); // 初始化TIM4

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE); // 允许TIM4更新中断

    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn; // TIM4中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; // 抢占优先级2
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3; // 子优先级3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // IRQ通道使能
    NVIC_Init(&NVIC_InitStructure); // 初始化NVIC寄存器

    TIM_Cmd(TIM4, ENABLE); // 使能TIM4
}

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        g_sysTickSec++; // 系统秒数+1
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update); // 清除中断标志
    }
}
// ====================== 定时器中断结束 ======================

int main(void)
{	
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //�����ж����ȷ���
    usart2_init(9600);  // 恢复：ESP8266用于MiMo Token通信
    usart3_init(9600);	// 串口3初始化（用于HC-05蓝牙）
    delay_init(72);			//ϵͳ��ʱ������ʼ��
    delay_ms(500);

    TIM2_Int_Init(10000,3600);//�������ʱ��  ����õ���ʱ��2��Ϊ0.5�����жϷ���Դ
    ADC1_DMA_Config();
    Key_Init();				//�������ų�ʼ��	
    JDQ_Init();				//�̵�����ʼ��
    BEEP_Init();			//��������ʼ��
    OLED_Init();//OLED��Ļ��ʼ��		
    OLED_Clear();

    TIM1_Int_Init(72-1,1000-1);//����ɨ��
    DS18B20_Init();//��ʼ����ʪ�ȴ�����
    delay_ms(100);

    // ====================== 新增：MiMo Token 初始化 ======================
    TIM4_Int_Init(999, 7199); // 1秒定时器（72M/(7199+1)/(999+1)=1Hz）
    MimoToken_LoadFromFlash(); // 加载Flash中存储的Token
    // ====================== MiMo Token 初始化结束 ======================

    Sensorthreshold.tempValue = FLASH_R(FLASH_START_ADDR);	//��ָ��ҳ�ĵ�ַ��FLASH
    Sensorthreshold.NTUValue = FLASH_R(FLASH_START_ADDR+2);	//��ָ��ҳ�ĵ�ַ��FLASH
    Sensorthreshold.PHValue_L = FLASH_R(FLASH_START_ADDR+4);	//��ָ��ҳ�ĵ�ַ��FLASH
    Sensorthreshold.PHValue_H = FLASH_R(FLASH_START_ADDR+6);	//��ָ��ҳ�ĵ�ַ��FLASH
    Sensorthreshold.TDSValue = FLASH_R(FLASH_START_ADDR+8);	//��ָ��ҳ�ĵ�ַ��FLASH
    delay_ms(10);

    Sensorthreshold.tempValue=30;
    Sensorthreshold.NTUValue=500;
    Sensorthreshold.TDSValue=200;
    Sensorthreshold.PHValue_L=4;
    Sensorthreshold.PHValue_H=10;

    while (1)
    {
        // ====================== 新增：MiMo Token 检测与重申请 ======================
        MimoToken_CheckAndReapply();
        // ====================== MiMo Token 检测结束 ======================

        do
        {
            currentDataPoint.valuetempts = (float)sensorData.temp / 10.0f;
            currentDataPoint.valuePH_L = sensorData.PH;
            currentDataPoint.valuePH_H = sensorData.PH;
            currentDataPoint.valuezhuodu_ts = sensorData.TS;
            currentDataPoint.valuetds_ts = sensorData.TDS;
        }while(0);//��ֵ�ϴ�������
		
        SensorScan();	//��ȡ����������
        // 发送数据到HC-05蓝牙模块（携带Token）
        SendBluetoothData(sensorData.temp / 10.0f, sensorData.PH, sensorData.TS);
        
        if (wifiConfigMode == 0)  // 配网模式标志为0时才执行正常显示
        {
            switch(mode)
            {
                case AUTO_MODE:	//�Զ�ģʽ
                    OLED_autoPage1();	//��ʾ��ҳ��1�̶���Ϣ
                    SensorDataDisplay1();	//��ʾ������1����
                    AutoControl();//�Զ�ģʽ���ƺ���
                    /*����1����ʱ�л�ģʽ*/
                    if (KeyNum == KEY_1)//�̰�   //ϵͳģʽmode  1�Զ�  2�ֶ�  3����
                    {
                        KeyNum = 0;
                        mode = MANUAL_MODE;//�ֶ�ģʽ
                        count_m = 1;//�ֶ�ģʽ��key2���µĴ�����һ
                        OLED_Clear();
                    }
                    if (KeyNum == KEY_Long1)//����
                    {
                        KeyNum = 0;
                        mode = SETTINGS_MODE;//��ֵ����ģʽ
                        count_s = 1;//��ֵ����ģʽ��key2���µĴ���
                        OLED_Clear();
                    }
                    Control_Manager();//ִ����ִ����Ӧ��ָ��
                    SendBluetoothData(sensorData.temp / 10.0f, sensorData.PH, sensorData.TS);
                    break;
                    
                case MANUAL_MODE://�ֶ�ģʽ
                    OLED_manualOption(SetManual());//�ֶ�ģʽҳ������Ϣ��ʾ
                    ManualControl(SetManual());//�ֶ�ģʽ����Ӧ�ı�־λ��һ������
                    if (SetManual() <= 2)		
                    {	
                        OLED_manualPage1();//�ֶ�ģʽ����1��ʾ
                        ManualSettingsDisplay1();//�ֶ�ģʽ�¿�/�ص���ʾ
                    }
                    
                    if (KeyNum == KEY_1)   //ϵͳģʽmode  0�ֶ�  1�Զ���Ĭ�ϣ�
                    {
                        KeyNum = 0;
                        mode = AUTO_MODE;//�ص��Զ�ģʽ
                        count_a = 1;//�Զ�ģʽ��key2���µĴ���
                        OLED_Clear();
                    }
                    SendBluetoothData(sensorData.temp / 10.0f, sensorData.PH, sensorData.TS);
                    Control_Manager();//ִ����ִ����Ӧ��ָ��
                    break;

                case SETTINGS_MODE://��ֵ����ģʽ
                    OLED_settingsOption(SetSelection());	//ʵ����ֵ����ҳ���ѡ����
                    ThresholdSettings(SetSelection());	//ʵ����ֵ���ڹ���	
                    
                    if (SetSelection() <= 4)		
                    {				
                        OLED_settingsPage1();	//��ʾ��ֵ���ý���1�̶���Ϣ
                        SettingsThresholdDisplay1();	//��ʾ��������ֵ1����	
                    }
                    else	//������ʾ����ҳ��2
                    {			
                        OLED_settingsPage2();	//��ʾ��ֵ���ý���2�̶���Ϣ
                        SettingsThresholdDisplay2();	//��ʾ��������ֵ2����	
                    }
                    //�ж��Ƿ��˳���ֵ���ý���
                    if (KeyNum == KEY_1)
                    {
                        KeyNum = 0;
                        mode = AUTO_MODE;	//��ת��������
                        count_a = 1;//�Զ�ģʽ��key2���µĴ���
                        OLED_Clear();	//����
                        
                        //�洢�޸ĵĴ�������ֵ��flash��	
                        FLASH_W(FLASH_START_ADDR, Sensorthreshold.tempValue, 
                                Sensorthreshold.NTUValue,
                                Sensorthreshold.PHValue_L, 
                                Sensorthreshold.PHValue_H,
                                Sensorthreshold.TDSValue);
                    }
                    break;
                default: break;
            }
            if (valueFlashFlag)
            {
                valueFlashFlag = 0;
                FLASH_W(FLASH_START_ADDR, Sensorthreshold.tempValue, 
                        Sensorthreshold.NTUValue,
                        Sensorthreshold.PHValue_L,
                        Sensorthreshold.PHValue_H,
                        Sensorthreshold.TDSValue);			
            }
        }
    }
}
