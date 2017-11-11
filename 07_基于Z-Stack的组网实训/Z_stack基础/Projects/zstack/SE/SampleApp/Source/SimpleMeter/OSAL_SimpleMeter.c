/*******************************************************************************
 * 文件名称：OSAL_SimpleMeter.c
 * 功    能：简单计量器任务初始化。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
#include "hal_drivers.h"
#include "OSAL_Tasks.h"

#if defined ( MT_TASK )
  #include "MT_TASK.h"
#endif

#include "ZDApp.h"
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  #include "ZDNwkMgr.h"
#endif
#if defined ( ZIGBEE_FRAGMENTATION )
  #include "aps_frag.h"
#endif

#if defined ( ZCL_KEY_ESTABLISH )
  #include "zcl_key_establish.h"
#endif

#include "simplemeter.h"
/********************************************************************/



/* 全局变量 */
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
  APSF_ProcessEvent,            // 应用支持子层消息分割任务处理函数
#endif
  ZDApp_event_loop,             // ZigBee设备应用任务处理函数
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_event_loop,          // 网络管理层任务处理函数
#endif
  zcl_event_loop,               // ZCL任务处理函数
#if defined ( ZCL_KEY_ESTABLISH )
  zclKeyEstablish_event_loop,   // ZCL密钥建立任务处理函数
#endif
  simplemeter_event_loop        // 用户应用层任务处理函数，由用户自己编写
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
  zcl_Init( taskID++ );
#if defined ( ZCL_KEY_ESTABLISH )
  zclGeneral_KeyEstablish_Init( taskID++ );
#endif
  simplemeter_Init( taskID );
}
