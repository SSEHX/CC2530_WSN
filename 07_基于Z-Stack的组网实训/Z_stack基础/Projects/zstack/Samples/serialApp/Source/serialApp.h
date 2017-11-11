/*******************************************************************************
 * 文件名称：serialApp.h
 * 功    能：GenericApp.c文件的头文件。
 ******************************************************************************/

#ifndef GENERICAPP_H
#define GENERICAPP_H

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
#define GENERICAPP_ENDPOINT           10

  
/*
   Profile ID
 */
#define GENERICAPP_PROFID             0x0F04
  

/*
   设备ID
 */ 
#define GENERICAPP_DEVICEID           0x0001
  

/*
   设备版本
 */   
#define GENERICAPP_DEVICE_VERSION     0
  

/*
   标识
 */  
#define GENERICAPP_FLAGS              0

  
/*
   簇的数量
 */   
#define GENERICAPP_MAX_CLUSTERS       1
  


/*
   "Hello World"信息簇
 */   
#define GENERICAPP_CLUSTERID          1

  
/*
   
 */  
#define GENERICAPP_SEND_MSG_TIMEOUT   5000     // 每5秒一次


/*
   应用事件（OSAL）
 */  
#define GENERICAPP_SEND_MSG_EVT       0x0001
#define GENERICAPP_READ_UART_EVT       0x0002    
/********************************************************************/
  
  
/*********************************************************************
 * 函数名称：GenericApp_Init
 * 功    能：GenericApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void GenericApp_Init( byte task_id );


/*********************************************************************
 * 函数名称：GenericApp_ProcessEvent
 * 功    能：GenericApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern UINT16 GenericApp_ProcessEvent( byte task_id, UINT16 events );


#ifdef __cplusplus
}
#endif

#endif /* GENERICAPP_H */
