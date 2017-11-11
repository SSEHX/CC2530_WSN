/*******************************************************************************
 * 文件名称：simplemeter.h
 * 功    能：SE智能能源实验simplemeter.c文件的头文件。
 ******************************************************************************/

#ifndef SIMPLEMETER_H
#define SIMPLEMETER_H

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
  简单计量器端点值
 */
#define SIMPLEMETER_ENDPOINT                       0x09

/* 
  简单计量器最大属性数
 */
#define SIMPLEMETER_MAX_ATTRIBUTES                 50

/* 
  简单计量器最大选项数
 */
#define SIMPLEMETER_MAX_OPTIONS                    2

/* 
  更新时间周期
 */
#define SIMPLEMETER_UPDATE_TIME_PERIOD             1000   // 更新时间以秒为单位 
#define SIMPLEMETER_REPORT_PERIOD                  5000   // 报告间隔周期
#define SE_DEVICE_POLL_RATE                        8000   // SE终端设备查询速率 

/* 
  应用程序事件
 */
#define SIMPLEMETER_IDENTIFY_TIMEOUT_EVT           0x0001
#define SIMPLEMETER_UPDATE_TIME_EVT                0x0002
#define SIMPLEMETER_KEY_ESTABLISHMENT_REQUEST_EVT  0x0004
#define SIMPLEMETER_REPORT_ATTRIBUTE_EVT           0x0008


/* 
  变量
 */
/********************************************************************/
extern SimpleDescriptionFormat_t simpleMeterSimpleDesc;
extern CONST zclAttrRec_t simpleMeterAttrs[];
extern zclOptionRec_t simpleMeterOptions[];
extern uint8 simpleMeterDeviceEnabled;
extern uint16 simpleMeterTransitionTime;
extern uint16 simpleMeterIdentifyTime;
extern uint32 simpleMeterTime;
/********************************************************************/



/*********************************************************************
 * 函数名称：simplemeter_Init
 * 功    能：简单计量器的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void simplemeter_Init( uint8 task_id );


/*********************************************************************
 * 函数名称：simplemeter_event_loop
 * 功    能：简单计量器的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern uint16 simplemeter_event_loop( uint8 task_id, uint16 events );


#ifdef __cplusplus
}
#endif

#endif /* SIMPLEMETER_H */
