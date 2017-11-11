/*******************************************************************************
 * 文件名称：SerialApp.h
 * 功    能：SerialApp.c文件的头文件。
 ******************************************************************************/

#ifndef SERIALAPP_H
#define SERIALAPP_H

#ifdef __cplusplus
extern "C"
{
#endif
 
  

/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
/********************************************************************/

  

/* 以下这些常量仅仅作为例子使用它们可根据设备需要被修改。 */
/********************************************************************/
/*
   应用端点
 */ 
#define SERIALAPP_ENDPOINT           11

  
/*
   Profile ID
 */
#define SERIALAPP_PROFID             0x0F05
  

/*
   设备ID
 */ 
#define SERIALAPP_DEVICEID           0x0001
  

/*
   设备版本
 */   
#define SERIALAPP_DEVICE_VERSION     0
  

/*
   标识
 */ 
#define SERIALAPP_FLAGS              0

  
/*
   簇的数量
 */ 
#define SERIALAPP_MAX_CLUSTERS       2
  

/*
   发送数据簇
 */  
#define SERIALAPP_CLUSTERID1         1
  
  
/*
   响应信息簇
 */  
#define SERIALAPP_CLUSTERID2         2

  
/*
   发送数据重传事件 
 */
#define SERIALAPP_MSG_RTRY_EVT       0x0001
  

/*
   响应信息重传事件 
 */
#define SERIALAPP_RSP_RTRY_EVT       0x0002
  
  
/*
   发送数据发送事件 
 */
#define SERIALAPP_MSG_SEND_EVT       0x0004


/*
   最大重传次数 
 */
#define SERIALAPP_MAX_RETRIES        10


/*
   重传超时时间
   若本应用中设备使用终端设备这个网络角色，那么根据终端设备的特点，
   重传超时时间应该至少设置为终端设备轮询速率POLL_RATE（该值在f8wConfig.cfg
   文件中定义，默认值为1000毫秒）的2倍。
   若本应用中设备只使用路由器和协调器这两种网络角色，则重传超时时
   间可以设置得更小一些，例如100ms。
    
 */  
#define SERIALAPP_MSG_RTRY_TIMEOUT   POLL_RATE*2
#define SERIALAPP_RSP_RTRY_TIMEOUT   POLL_RATE*2


/*
   OTA信息流控时延 
 */
/* 从代码可分析，该值实际没用使用意义 */  
#define SERIALAPP_ACK_DELAY          1  
/* 若串口忙，则应通知对方应该延时该值这么长的时间后再重发 */
#define SERIALAPP_NAK_DELAY          250  


/*
   OTA信息流控状态 
 */
/* 接收成功（是指将接收到的OTA信息成功写入串口） */
#define OTA_SUCCESS                  ZSuccess     
/* 重复（是指接收到的OTA信息是重复信息） */
#define OTA_DUP_MSG                 (ZSuccess+1)  
/* 串口忙（是指将接收到的OTA信息写入串口失败，因为此时串口忙） */
#define OTA_SER_BUSY                (ZSuccess+2)   
/********************************************************************/


  
/* 全局变量 */
/********************************************************************/
/*
   本应用的任务ID变量。当SerialApp_Init()函数被调用时，   该变量可以
   获得任务ID值。
*/
extern byte SerialApp_TaskID;
/********************************************************************/



/*********************************************************************
 * 函数名称：SerialApp_Init
 * 功    能：SerialApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void SerialApp_Init( byte task_id );


/*********************************************************************
 * 函数名称：SerialApp_ProcessEvent
 * 功    能：SerialApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern UINT16 SerialApp_ProcessEvent( byte task_id, UINT16 events );




#ifdef __cplusplus
}
#endif

#endif /* SERIALAPP_H */