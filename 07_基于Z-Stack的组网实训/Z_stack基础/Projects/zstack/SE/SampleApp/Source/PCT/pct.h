/*******************************************************************************
 * 文件名称：pct.h
 * 功    能：SE智能能源实验pct.c文件的头文件。
 ******************************************************************************/

#ifndef PCT_H
#define PCT_H

#ifdef __cplusplus
extern "C"
{
#endif

/* 包含头文件 */
/********************************************************************/
#include "zcl.h"
/********************************************************************/


/* 宏定义 */
/********************************************************************/
/* 
  可编程通信温控器端点值
 */
#define PCT_ENDPOINT                 0x09

/* 
  可编程通信温控器最大属性数
 */
#define PCT_MAX_ATTRIBUTES           16

/* 
  可编程通信温控器最大选项数
 */
#define PCT_MAX_OPTIONS              3

/* 
  更新时间周期
 */
#define PCT_UPDATE_TIME_PERIOD       1000       // 更新时间事件以秒为单位
#define PCT_LOAD_CTRL_PERIOD         60000      // 可编程通信温控器事件持续1分钟，以毫秒为单位
#define LOADCONTROL_EVENT_ID         0x12345678 // 负载控制器事件ID
#define START_TIME_NOW               0x00000000 // 开启事件 - 现在马上执行
#define HVAC_DEVICE_CLASS            0x000001   // HVAC  压缩机或炉 - 位0置位
#define ONOFF_LOAD_DEVICE_CLASS      0x000080   // 简单住宅开关负载 - 位7置位
#define SE_DEVICE_POLL_RATE          8000       // SE终端设备查询速率

/* 
  应用程序事件
 */
#define PCT_IDENTIFY_TIMEOUT_EVT           0x0001
#define PCT_UPDATE_TIME_EVT                0x0002
#define PCT_KEY_ESTABLISHMENT_REQUEST_EVT  0x0004
#define PCT_LOAD_CTRL_EVT                  0x0008

/* 
  变量
 */
/********************************************************************/
extern SimpleDescriptionFormat_t pctSimpleDesc;
extern CONST zclAttrRec_t pctAttrs[];
extern zclOptionRec_t pctOptions[];
extern uint8 pctDeviceEnabled;
extern uint16 pctTransitionTime;
extern uint16 pctIdentifyTime;
extern uint32 pctTime;
extern uint8 pctSignature[];
/********************************************************************/



/*********************************************************************
 * 函数名称：pct_Init
 * 功    能：PCT的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void pct_Init( uint8 task_id );


/*********************************************************************
 * 函数名称：pct_event_loop
 * 功    能：PCT的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern uint16 pct_event_loop( uint8 task_id, uint16 events );


#ifdef __cplusplus
}
#endif

#endif /* PCT_H */
