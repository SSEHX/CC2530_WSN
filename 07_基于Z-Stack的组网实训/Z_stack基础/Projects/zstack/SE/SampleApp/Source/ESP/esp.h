/*******************************************************************************
 * 文件名称：esp.h
 * 功    能：SE智能能源实验esp.c文件的头文件。
 ******************************************************************************/

#ifndef ESP_H
#define ESP_H

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
  ESP端点值
 */
#define ESP_ENDPOINT                 0x09

/* 
  ESP最大属性数
 */
#define ESP_MAX_ATTRIBUTES           53

/* 
  ESP最大选项数
 */
#define ESP_MAX_OPTIONS              8

/* 
  ESP更新时间周期
 */
#define ESP_UPDATE_TIME_PERIOD       1000     // 单位秒

/* 
  HVAC设备类
 */
#define HVAC_DEVICE_CLASS            0x000001 // HVAC 压缩机或炉 - 位0置位

/* 
  开发负载设备类
 */
#define ONOFF_LOAD_DEVICE_CLASS      0x000080 // 简单住宅开关负载 - 位7置位

/* 
  应用程序事件
 */
#define ESP_IDENTIFY_TIMEOUT_EVT     0x0001
#define ESP_UPDATE_TIME_EVT          0x0002
#define SIMPLE_DESC_QUERY_EVT_1      0x0004
#define SIMPLE_DESC_QUERY_EVT_2      0x0008
/********************************************************************/

/* 
  变量
 */
/********************************************************************/
extern SimpleDescriptionFormat_t espSimpleDesc;
extern CONST zclAttrRec_t espAttrs[];
extern zclOptionRec_t espOptions[];
extern uint8 espDeviceEnabled;
extern uint16 espTransitionTime;
extern uint16 espIdentifyTime;
extern uint32 espTime;
extern uint8 espSignature[];
/********************************************************************/



/*********************************************************************
 * 函数名称：esp_Init
 * 功    能：ESP的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void esp_Init( uint8 task_id );


/*********************************************************************
 * 函数名称：esp_event_loop
 * 功    能：ESP的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern uint16 esp_event_loop( uint8 task_id, uint16 events );


#ifdef __cplusplus
}
#endif

#endif /* esp_H */
