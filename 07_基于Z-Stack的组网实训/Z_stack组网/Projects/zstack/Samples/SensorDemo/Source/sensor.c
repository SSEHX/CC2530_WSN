/**************************************************************************************************
  Filename:       sensor.c  */



#include "hal_defs.h"
#include "hal_types.h"
#include "hal_mcu.h"
#include "ioCC2530.h"
#include "sh10.h"
#include "sensor.h"
#include "UART_PRINT.h"
#include "OnBoard.h"

 
/******************************************************************************
 * GLOBAL VARIABLES
 */
void hal_LightSensor_Init(void);
static uint16 readVoltage(void);
//static volatile uint32 value;

void hal_LightSensor_Init(void)
{
	APCFG  |=1;   
        P0SEL  |= (1 << (0));	
        P0DIR  &= ~(1 << (0));	
        P0INP |=1; 
        //P0INP &= ~1;
        //P2INP |= 0x20;
        
}
/******************************************************************************
 * @fn          readVoltage
 *
 * @brief       read voltage from ADC
 *
 * @param       none
 *              
 * @return      voltage
 *******/
static uint16 readVoltage(void)
{

   uint32 value;
   int16 value1;
   hal_LightSensor_Init(); 
  // Clear ADC interrupt flag 
  ADCIF = 0;

  ADCCON3 = (0x80 | 0x10 | 0x00);
  //ADCCON3 = (0x80 | 0x10 | 0x0c);
  //采用基准电压avdd5:3.3V
  //  Wait for the conversion to finish 
  while ( !ADCIF );

  // Get the result
  value1 = ADCH;
  value1 = value1<< 8;
  value1 |= ADCL;
  if(value1<0)
      value1=0;
  value=value1;

  // value now contains measurement of 3.3V
  // 0 indicates 0V and 32767 indicates 3.3V
  // voltage = (value*3.3)/32767 volts
  // we will multiply by this by 10 to allow units of 0.01 volts
  value = value >> 6;   // divide first by 2^6
  value = (uint32)(value * 330);
  value = value >> 9;   // ...and later by 2^9...to prevent overflow during multiplication

  return (uint16)value;
 
}

/******************************************************************************
 * @fn          readsensor
 *
 * @brief       read readsensor
 *
 * @param       sensorid
 *              
 * @return      sensor value
 *******/
void readsensor(uint8 sensorid, uint16 *val1, uint16 *val2)
{
       *val1=0;
       *val2=0;
       
       if(sensorid == 0x01)
       {    //温湿度
          
           //HAL_DISABLE_INTERRUPTS();
           call_sht11();

           *val1=(int)S.DateString1[0]*256+S.DateString1[1];
           *val2=(int)S.DateString1[2]*256+S.DateString1[3];
#if defined (UART_SENSOR)
           uart_printf("数据：");
           uart_datas(S.DateString1, 4);
           uart_printf("温度：%d.%d, 湿度：%d.%d\r\n",*val1/10,*val1%10,*val2/10,*val2%10);
#endif
           //HAL_ENABLE_INTERRUPTS();
           return;    
      } 
       if(sensorid == 0x11)
       {    //人体 p0.1 
            APCFG &= ~(1<<1);
            P0SEL  &= ~(1 << (1));
            P0DIR  &= ~(1 << (1));	
            
           // P2INP  &= ~(1 << (5));   // 0上拉，1下拉
            //P0INP  &= ~(1 << (1));    // 1--三态
            //P0INP  |= (1 << (1));
             // 03人体传感器，测i/o高低
            *val1=0;
            if( P0_1==0 )
               *val1=0x01; 
            return;    
       } 
       
       //if(sensorid == 0x21)   // ADC型传感器
       {    //光照  adc  
            *val1=readVoltage();
            #if defined (UART_SENSOR)
            uart_printf("电压：%d.%d\r\n",*val1/100,*val1%100);
            #endif
            // 计算 val2值（实际含义值）
            return;    
       } 
       
    
}