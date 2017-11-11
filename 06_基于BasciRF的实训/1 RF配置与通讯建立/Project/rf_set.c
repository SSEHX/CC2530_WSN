#include "hal_defs.h"
#include "hal_cc8051.h"
#include "hal_int.h"
#include "hal_mcu.h"
#include "hal_board.h"
#include "hal_led.h"
#include "hal_rf.h"
#include "basic_rf.h"
#include "hal_uart.h" 
#include <stdio.h>
#include <string.h>
#include	<stdarg.h>
uint8   scan_key();

#define MAX_SEND_BUF_LEN  128
#define MAX_RECV_BUF_LEN  128
static uint8 pTxData[MAX_SEND_BUF_LEN]; //定义无线发送缓冲区的大小
static uint8 pRxData[MAX_RECV_BUF_LEN]; //定义无线接收缓冲区的大小


#define MAX_UART_SEND_BUF_LEN  128
#define MAX_UART_RECV_BUF_LEN  128
uint8 uTxData[MAX_UART_SEND_BUF_LEN]; //定义串口发送缓冲区的大小
uint8 uRxData[MAX_UART_RECV_BUF_LEN]; //定义串口接收缓冲区的大小
uint16 uTxlen = 0;
uint16 uRxlen = 0;

/*****点对点通讯地址设置******/
#define RF_CHANNEL                20         // 频道 11~26
#define PAN_ID                    0x1A5B     //网络id 
#define MY_ADDR                   0x1015     // 本机模块地址
#define SEND_ADDR                 0xAC3A     //发送地址
/**************************************************/
static basicRfCfg_t basicRfConfig;
// 无线RF初始化
void ConfigRf_Init(void)
{
    basicRfConfig.panId       =   PAN_ID;        //zigbee的ID号设置
    basicRfConfig.channel     =   RF_CHANNEL;    //zigbee的频道设置
    basicRfConfig.myAddr      =  MY_ADDR;   //设置本机地址
    basicRfConfig.ackRequest  =   TRUE;          //应答信号
    while(basicRfInit(&basicRfConfig) == FAILED); //检测zigbee的参数是否配置成功
    basicRfReceiveOn();                // 打开RF
}

/********************MAIN************************/
void main(void)
{
    uint16 len = 0;
    halBoardInit();  //模块相关资源的初始化
    ConfigRf_Init(); //无线收发参数的配置初始化 
    HAL_LED_SET_1();    // led1 on
    HAL_LED_SET_2();    // led2 on
    while(1)
    {
       if(scan_key())   //有按键，则发送数据
       {           
            halLedToggle(3);       // 绿灯取反，发送指示
            basicRfSendPacket(SEND_ADDR,"ZIGBEE TEST\r\n",13);            
                          
        } 
    
        if(basicRfPacketIsReady())   // 判断有无收到zigbee信号
        {
            halLedToggle(4);      // 红灯取反，接收指示
            len = basicRfReceive(pRxData, MAX_RECV_BUF_LEN, NULL);   // 接收数据
        } 
    }
}
/************/

#define key_io P1_2


uint8   scan_key()
{
    static  uint8   keysta=1;
    if (key_io)
    {       //按键新开
        keysta=1;
        return 0;        
    }
    else
    {
       if(keysta==0)
           return 0;
       keysta=0;
       return 1; 
    }
}