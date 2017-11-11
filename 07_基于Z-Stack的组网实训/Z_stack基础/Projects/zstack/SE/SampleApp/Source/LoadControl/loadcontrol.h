/*******************************************************************************
 * 文件名称：loadcontrol.h
 * 功    能：SE智能能源实验loadcontrol.c文件的头文件。
 ******************************************************************************/

#ifndef LOADCONTROL_H
#define LOADCONTROL_H

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
  负载控制器端点值
 */
#define LOADCONTROL_ENDPOINT                 0x09

/* 
  负载控制器最大属性数
 */
#define LOADCONTROL_MAX_ATTRIBUTES           16

/* 
  负载控制器最大选项数
 */
#define LOADCONTROL_MAX_OPTIONS              2

/* 
  更新时间周期
 */
#define LOADCONTROL_UPDATE_TIME_PERIOD       1000       // 更新时间事件以秒为单位
#define LOADCONTROL_LOAD_CTRL_PERIOD         60000      // 负载控制器事件持续1分钟，以毫秒为单位
#define LOADCONTROL_EVENT_ID                 0x12345678 // 负载控制器事件ID
#define START_TIME_NOW                       0x00000000 // 开启事件 - 现在马上执行
#define HVAC_DEVICE_CLASS                    0x000001   // HVAC  压缩机或炉 - 位0置位
#define ONOFF_LOAD_DEVICE_CLASS              0x000080   // 简单住宅开关负载 - 位7置位

/* 
  应用程序事件
 */
#define LOADCONTROL_IDENTIFY_TIMEOUT_EVT           0x0001
#define LOADCONTROL_UPDATE_TIME_EVT                0x0002
#define LOADCONTROL_KEY_ESTABLISHMENT_REQUEST_EVT  0x0004
#define LOADCONTROL_LOAD_CTRL_EVT                  0x0008

/* 
  变量
 */
/********************************************************************/
extern SimpleDescriptionFormat_t loadControlSimpleDesc;
extern CONST zclAttrRec_t loadControlAttrs[];
extern zclOptionRec_t loadControlOptions[];
extern uint8 loadControlDeviceEnabled;
extern uint16 loadControlTransitionTime;
extern uint16 loadControlIdentifyTime;
extern uint32 loadControlTime;
extern uint8 loadControlSignature[];
/********************************************************************/



/*********************************************************************
 * 函数名称：loadcontrol_Init
 * 功    能：LCD的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void loadcontrol_Init( uint8 task_id );


/*********************************************************************
 * 函数名称：loadcontrol_event_loop
 * 功    能：LCD的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern uint16 loadcontrol_event_loop( uint8 task_id, uint16 events );



#ifdef __cplusplus
}
#endif

#endif /* LOADCONTROL_H */
