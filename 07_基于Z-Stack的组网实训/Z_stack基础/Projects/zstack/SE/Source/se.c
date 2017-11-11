/*******************************************************************************
 * 文件名称：se.c
 * 功    能：SE的初始化函数文件。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
#include "OSAL.h"
#include "zcl.h"
#include "zcl_general.h"
#include "zcl_se.h"
#include "zcl_key_establish.h"
#include "se.h"
/********************************************************************/



/* 本地函数 */
/*********************************************************************
 * 函数名称：zclSE_Init
 * 功    能：SE的初始化函数。
 * 入口参数：*simpleDesc 使用SE Profile注册简单描述符。该函数同时也注册
 *           Profile的簇转换表。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zclSE_Init( SimpleDescriptionFormat_t *simpleDesc )
{
  endPointDesc_t *epDesc;

  /* 注册应用程序端点描述符，分配内存不会被释放 */
  epDesc = osal_mem_alloc( sizeof ( endPointDesc_t ) );
  if ( epDesc )
  {
    /* 填充端点描述符 */
    epDesc->endPoint = simpleDesc->EndPoint;
    epDesc->task_id = &zcl_TaskID;   // 所有消息首先发送ZCL
    epDesc->simpleDesc = simpleDesc;
    epDesc->latencyReq = noLatencyReqs;

    /* 使用AF注册端点描述符 */
    afRegister( epDesc );
  }
}
