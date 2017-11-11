/*******************************************************************************
 * 文件名称：OSAL_GenericApp.c
 * 功    能：本文件包含应当由用户进行设置和修改的所有配置以及其他函数。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
#include "hal_drivers.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"

#if defined ( MT_TASK )
  #include "MT.h"
  #include "MT_TASK.h"
#endif

#include "nwk.h"
#include "APS.h"
#include "ZDApp.h"
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  #include "ZDNwkMgr.h"
#endif
#if defined ( ZIGBEE_FRAGMENTATION )
  #include "aps_frag.h"
#endif
   
#include "SwitchApp.h"
/********************************************************************/
/* 全局变量 */
//extern void oadAppInit(uint8 id);
//extern uint16 oadAppEvt(uint8 id, uint16 evts);
/********************************************************************/
/*
   任务数组：存放每个任务的相应的处理函数指针
   注意：下表中各项的顺序必须与下面的osalInitTasks()函数
         中任务初始化调用的顺序一致
*/
const pTaskEventHandlerFn tasksArr[] = {
  macEventLoop,                 // MAC层任务处理函数
  nwk_event_loop,               // 网络层任务处理函数
  Hal_ProcessEvent,             // 硬件抽象层任务处理函数
#if defined( MT_TASK )
  MT_ProcessEvent,              // 监控测试任务处理函数
#endif
  APS_event_loop,               // 应用支持子层任务处理函数
#if defined ( ZIGBEE_FRAGMENTATION )
  APSF_ProcessEvent,            // APSF层任务处理函数
#endif
  ZDApp_event_loop,             // ZigBee设备应用任务处理函数
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_event_loop,          // 网络管理层任务处理函数
#endif
  //oadAppEvt,
  GenericApp_ProcessEvent       // 用户应用层任务处理函数，由用户自己编写
};


/*
   任务数量
*/
const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );


/*
   任务事件
*/
uint16 *tasksEvents;
/********************************************************************/


 /*********************************************************************
 * 函数名称：osalInitTasks
 * 功    能：初始化系统中的每一个任务。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void osalInitTasks( void )
{
  uint8 taskID = 0;

  tasksEvents = (uint16 *)osal_mem_alloc( sizeof( uint16 ) * tasksCnt);
  osal_memset( tasksEvents, 0, (sizeof( uint16 ) * tasksCnt));

  macTaskInit( taskID++ );
  nwk_init( taskID++ );
  Hal_Init( taskID++ );
#if defined( MT_TASK )
  MT_TaskInit( taskID++ );
#endif
  APS_Init( taskID++ );
#if defined ( ZIGBEE_FRAGMENTATION )
  APSF_Init( taskID++ );
#endif
  ZDApp_Init( taskID++ );
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_Init( taskID++ );
#endif
  //oadAppInit(taskID++);
  GenericApp_Init( taskID );
}


