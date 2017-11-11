/*******************************************************************************
 * 文件名称：TransmitApp.h
 * 功    能：TransmitApp.c文件的头文件。
 ******************************************************************************/

#ifndef TRANSMITAPP_H
#define TRANSMITAPP_H

#ifdef __cplusplus
extern "C"
{
#endif

  
/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
/********************************************************************/

  
/* 全局变量 */
/********************************************************************/
/*
   本应用的任务ID变量。当SerialApp_Init()函数被调用时，   该变量可以
   获得任务ID值。
*/
extern byte TransmitApp_TaskID;
/********************************************************************/


/* 以下这些常量仅仅作为例子使用它们可根据设备需要被修改。 */
/********************************************************************/

/*
   应用端点
 */ 
#define TRANSMITAPP_ENDPOINT           1

/*
   Profile ID
 */
#define TRANSMITAPP_PROFID             0x0F05

/*
   设备ID
 */ 
#define TRANSMITAPP_DEVICEID           0x0001

/*
   设备版本
 */  
#define TRANSMITAPP_DEVICE_VERSION     0

/*
   标识
 */ 
#define TRANSMITAPP_FLAGS              0

/*
   簇的数量
 */ 
#define TRANSMITAPP_MAX_CLUSTERS       1

/*
   数据发送簇
 */ 
#define TRANSMITAPP_CLUSTERID_TESTMSG  1

/*
   数据发送事件 
 */
#define TRANSMITAPP_SEND_MSG_EVT       0x0001

/*
   接收显示事件 
 */
#define TRANSMITAPP_RCVTIMER_EVT       0x0002

/*
   发送错误事件 
 */
#define TRANSMITAPP_SEND_ERR_EVT       0x0004
/********************************************************************/



/* 宏 */
/********************************************************************/

/*
   定义该宏则使能数据包分片
 */
//#define TRANSMITAPP_FRAGMENTED
/********************************************************************/



/*********************************************************************
 * 函数名称：SerialApp_Init
 * 功    能：SerialApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void TransmitApp_Init( byte task_id );


/*********************************************************************
 * 函数名称：TransmitApp_ProcessEvent
 * 功    能：TransmitApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern UINT16 TransmitApp_ProcessEvent( byte task_id, UINT16 events );

/*********************************************************************
 * 函数名称：TransmitApp_ChangeState
 * 功    能：切换发送/等待状态标志
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void TransmitApp_ChangeState( void );


/*********************************************************************
 * 函数名称：TransmitApp_SetSendEvt
 * 功    能：设置数据发送事件标志
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void TransmitApp_SetSendEvt( void );


#ifdef __cplusplus
}
#endif

#endif /* TRANSMITAPP_H */
