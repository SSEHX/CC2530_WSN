/*******************************************************************************
 * 文件名称：rangeext.h
 * 功    能：SE智能能源实验rangeext.c文件的头文件。
 ******************************************************************************/

#ifndef RANGEEXT_H
#define RANGEEXT_H

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
  增距器端点值
 */
#define RANGEEXT_ENDPOINT                       0x09

/* 
  增距器最大属性数
 */
#define RANGEEXT_MAX_ATTRIBUTES                 13

/* 
  增距器最大选项数
 */
#define RANGEEXT_MAX_OPTIONS                    1

/* 
  更新时间周期
 */
#define RANGEEXT_UPDATE_TIME_PERIOD             1000  // 更新时间事件以秒为单位

/* 
  应用程序事件
 */
#define RANGEEXT_IDENTIFY_TIMEOUT_EVT           0x0001
#define RANGEEXT_KEY_ESTABLISHMENT_REQUEST_EVT  0x0002

/* 
  变量
 */
/********************************************************************/
extern SimpleDescriptionFormat_t rangeExtSimpleDesc;
extern CONST zclAttrRec_t rangeExtAttrs[];
extern zclOptionRec_t rangeExtOptions[];
extern uint8 rangeExtDeviceEnabled;
extern uint16 rangeExtTransitionTime;
extern uint16 rangeExtIdentifyTime;
extern uint32 rangeExtTime;
/********************************************************************/



/*********************************************************************
 * 函数名称：rangeext_Init
 * 功    能：增距器的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void rangeext_Init( uint8 task_id );

/*********************************************************************
 * 函数名称：rangeext_event_loop
 * 功    能：增距器的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern uint16 rangeext_event_loop( uint8 task_id, uint16 events );


#ifdef __cplusplus
}
#endif

#endif /* RANGEEXT_H */
