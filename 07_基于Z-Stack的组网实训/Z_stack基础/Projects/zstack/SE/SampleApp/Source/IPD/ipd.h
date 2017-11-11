/*******************************************************************************
 * 文件名称：ipd.h
 * 功    能：SE智能能源实验ipd.c文件的头文件。
 ******************************************************************************/

#ifndef IPD_H
#define IPD_H

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
  IPD端点值
 */
#define IPD_ENDPOINT                       0x09

/* 
  IPD最大属性数
 */
#define IPD_MAX_ATTRIBUTES                 13

/* 
  IPD最大选项数
 */
#define IPD_MAX_OPTIONS                    3

/* 
  更新时间周期
 */
#define IPD_UPDATE_TIME_PERIOD             1000   // 更新时间事件单位为秒
#define IPD_GET_PRICING_INFO_PERIOD        5000   // 获取价格信息命令间隔
#define SE_DEVICE_POLL_RATE                8000   // SE终端设备查询速率

/* 
  应用程序事件
 */
#define IPD_IDENTIFY_TIMEOUT_EVT           0x0001
#define IPD_UPDATE_TIME_EVT                0x0002
#define IPD_KEY_ESTABLISHMENT_REQUEST_EVT  0x0004
#define IPD_GET_PRICING_INFO_EVT           0x0008


/* 
  变量
 */
/********************************************************************/
extern SimpleDescriptionFormat_t ipdSimpleDesc;
extern CONST zclAttrRec_t ipdAttrs[];
extern zclOptionRec_t ipdOptions[];
extern uint8 ipdDeviceEnabled;
extern uint16 ipdTransitionTime;
extern uint16 ipdIdentifyTime;
extern uint32 ipdTime;
/********************************************************************/



/*********************************************************************
 * 函数名称：esp_Init
 * 功    能：ESP的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void ipd_Init( uint8 task_id );


/*********************************************************************
 * 函数名称：esp_event_loop
 * 功    能：ESP的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern uint16 ipd_event_loop( uint8 task_id, uint16 events );


#ifdef __cplusplus
}
#endif

#endif /* IPD_H */
