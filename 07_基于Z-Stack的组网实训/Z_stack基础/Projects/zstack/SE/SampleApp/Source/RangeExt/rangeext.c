/*******************************************************************************
 * 文件名称：rangeext.c
 * 功    能：SE智能能源实验。
 * 实验内容：ESP(能源服务门户)建立一个ZigBeePRO网路。其他设备控制器，比如IPD(
 *           预装显示设备)、PCT(可编程通信温控器)等发现该网络，并且加入到该网络中。
 *           ESP(能源服务门户)将通过无线方式管理其他设备。
 * 实验设备：SK-SmartRF05BB（装配有SK-CC2530EM）
 *           RIGHT键：            加入网络
 *
 *           D1(绿色)：      设备成功加入组1被点亮，退出组1后熄灭
 *           D3(黄色)：      当成功建立网络或加入网络后此LED点亮
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "ZDApp.h"
#include "AddrMgr.h"
#include "se.h"
#include "rangeext.h"
#include "zcl_general.h"
#include "zcl_key_establish.h"
#include "onboard.h"

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
/********************************************************************/


/* 宏定义 */
/********************************************************************/

/*
   当前在强制报告属性列表中没有属性
*/
#define zcl_MandatoryReportableAttribute( a ) ( a == NULL )


/* 本地变量 */
/********************************************************************/
static uint8 rangeExtTaskID;              // 增距器操作系统任务ID
static devStates_t rangeExtNwkState;      // 网络状态变量
static uint8 rangeExtTransID;             // 传输id
static afAddrType_t ESPAddr;              // ESP目的地址
static uint8 linkKeyStatus;               // 从获取链接密钥函数返回的状态变量


/* 本地函数 */
/********************************************************************/
static void rangeext_HandleKeys( uint8 shift, uint8 keys );

#if defined ( ZCL_KEY_ESTABLISH )
static uint8 rangeext_KeyEstablish_ReturnLinkKey( uint16 shortAddr );
#endif 

#if defined ( ZCL_ALARMS )
static void rangeext_ProcessAlarmCmd( uint8 srcEP, afAddrType_t *dstAddr,
                                      uint16 clusterID, zclFrameHdr_t *hdr, 
                                      uint8 len, uint8 *data );
#endif 

static void rangeext_ProcessIdentifyTimeChange( void );
/********************************************************************/


/* 本地函数 - 应用回调函数 */
/********************************************************************/
/*
   基本回调函数
*/
static uint8 rangeext_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo );

/*
   通用簇回调函数
*/
static void rangeext_BasicResetCB( void );
static void rangeext_IdentifyCB( zclIdentify_t *pCmd );
static void rangeext_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp );
static void rangeext_AlarmCB( zclAlarm_t *pAlarm );

/* 处理ZCL基本函数 - 输入命令/响应消息 */
/********************************************************************/
static void rangeext_ProcessZCLMsg( zclIncomingMsg_t *msg );

#if defined ( ZCL_READ )
static uint8 rangeext_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif

#if defined ( ZCL_WRITE )
static uint8 rangeext_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif 

static uint8 rangeext_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );

#if defined ( ZCL_DISCOVER )
static uint8 rangeext_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg );
#endif 
/********************************************************************/

/*
   ZCL通用簇回调表
*/
static zclGeneral_AppCallbacks_t rangeext_GenCmdCallbacks =
{
  rangeext_BasicResetCB,         // 基本簇复位命令
  rangeext_IdentifyCB,           // 识别命令
  rangeext_IdentifyQueryRspCB,   // 识别查询响应命令
  NULL,                          // 开关簇命令
  NULL,                          // 级别控制离开命令
  NULL,                          // 级别控制移动命令
  NULL,                          // 级别控制单步命令
  NULL,                          // 级别控制停止命令
  NULL,                          // 组响应命令
  NULL,                          // 场景保存请求命令
  NULL,                          // 场景重调用请求命令
  NULL,                          // 场景响应命令
  rangeext_AlarmCB,              // 报警(响应)命令
  NULL,                          // RSSI定位命令
  NULL,                          // RSSI定位响应命令
};


/*********************************************************************
 * 函数名称：rangeext_Init
 * 功    能：增距器的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void rangeext_Init( uint8 task_id )
{
  rangeExtTaskID = task_id;

  // 设置ESP目的地址
  ESPAddr.addrMode = (afAddrMode_t)Addr16Bit;
  ESPAddr.endPoint = RANGEEXT_ENDPOINT;
  ESPAddr.addr.shortAddr = 0;

  /* 注册一个SE端点 */
  zclSE_Init( &rangeExtSimpleDesc );

  /* 注册ZCL通用簇库回调函数 */
  zclGeneral_RegisterCmdCallbacks( RANGEEXT_ENDPOINT, &rangeext_GenCmdCallbacks );

  /* 注册应用程序属性列表 */
  zcl_registerAttrList( RANGEEXT_ENDPOINT, RANGEEXT_MAX_ATTRIBUTES, rangeExtAttrs );

  /* 注册应用程序簇选项列表 */
  zcl_registerClusterOptionList( RANGEEXT_ENDPOINT, RANGEEXT_MAX_OPTIONS, rangeExtOptions );

  /* 注册应用程序属性数据验证回调函数 */
  zcl_registerValidateAttrData( rangeext_ValidateAttrDataCB );

  /* 注册应用程序来接收未处理基础命令/响应消息 */
  zcl_registerForMsg( rangeExtTaskID );

  /* 注册所有按键事件 - 将处理所有按键事件 */
  RegisterForKeys( rangeExtTaskID );
}


/*********************************************************************
 * 函数名称：rangeext_event_loop
 * 功    能：增距器的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
uint16 rangeext_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( rangeExtTaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          /* 输入ZCL基础命令/响应消息 */
          rangeext_ProcessZCLMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          /* 按键处理 */
          rangeext_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          rangeExtNwkState = (devStates_t)(MSGpkt->hdr.status);

          if (ZG_SECURE_ENABLED)
          {
            if ( rangeExtNwkState == DEV_ROUTER )
            {
              /* 检查链接密钥是否已经被建立 */
              linkKeyStatus = rangeext_KeyEstablish_ReturnLinkKey(ESPAddr.addr.shortAddr);

              if (linkKeyStatus != ZSuccess)
              {
                /* 发送密钥建立请求 */
                osal_set_event( rangeExtTaskID, RANGEEXT_KEY_ESTABLISHMENT_REQUEST_EVT);
              }
            }
          }
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt ); // 释放存储器
    }

    return (events ^ SYS_EVENT_MSG); // 返回未处理的事件
  }

  /* 初始化密钥建立请求事件 */
  if ( events & RANGEEXT_KEY_ESTABLISHMENT_REQUEST_EVT )
  {
    zclGeneral_KeyEstablish_InitiateKeyEstablishment(rangeExtTaskID, &ESPAddr, rangeExtTransID);

    return ( events ^ RANGEEXT_KEY_ESTABLISHMENT_REQUEST_EVT );
  }

  /* 被一个识别命令触发的识别超时事件处理 */
  if ( events & RANGEEXT_IDENTIFY_TIMEOUT_EVT )
  {
    if ( rangeExtIdentifyTime > 0 )
    {
      rangeExtIdentifyTime--;
    }
    rangeext_ProcessIdentifyTimeChange();

    return ( events ^ RANGEEXT_IDENTIFY_TIMEOUT_EVT );
  }

  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：rangeext_ProcessIdentifyTimeChange
 * 功    能：调用闪烁LED来指定识别时间属性值。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_ProcessIdentifyTimeChange( void )
{
  if ( rangeExtIdentifyTime > 0 )
  {
    osal_start_timerEx( rangeExtTaskID, RANGEEXT_IDENTIFY_TIMEOUT_EVT, 1000 );
    HalLedBlink ( HAL_LED_4, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
    osal_stop_timerEx( rangeExtTaskID, RANGEEXT_IDENTIFY_TIMEOUT_EVT );
  }
}

#if defined ( ZCL_KEY_ESTABLISH )
/*********************************************************************
 * 函数名称：rangeext_KeyEstablish_ReturnLinkKey
 * 功    能：获取被请求的密钥函数。
 * 入口参数：shortAddr 短地址
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 rangeext_KeyEstablish_ReturnLinkKey( uint16 shortAddr )
{
  APSME_LinkKeyData_t* keyData;
  uint8 status = ZFailure;
  AddrMgrEntry_t entry;

  /* 查找设备长地址 */
  entry.user = ADDRMGR_USER_DEFAULT;
  entry.nwkAddr = shortAddr;

  if ( AddrMgrEntryLookupNwk( &entry ) )
  {
    /* 检查APS链接密钥数据 */
    APSME_LinkKeyDataGet( entry.extAddr, &keyData );

    if ( (keyData != NULL) && (keyData->key != NULL) )
    {
      status = ZSuccess;
    }
  }
  else
  {
    status = ZInvalidParameter; // 未知设备
  }

  return status;
}
#endif


/*********************************************************************
 * 函数名称：rangeext_HandleKeys
 * 功    能：增距器的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_2     SK-SmartRF10BB的K2键       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_HandleKeys( uint8 shift, uint8 keys )
{
  if ( keys & HAL_KEY_SW_2 )
  {
    ZDOInitDevice(0); // 加入网络
  }
}


/*********************************************************************
 * 函数名称：rangeext_ValidateAttrDataCB
 * 功    能：检查为属性提供的数据值是否在该属性指定的范围内。
 * 入口参数：pAttr     指向属性的指针
 *           pAttrInfo 指向属性信息的指针            
 * 出口参数：无
 * 返 回 值：TRUE      有效数据
 *           FALSE     无效数据
 ********************************************************************/
static uint8 rangeext_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo )
{
  uint8 valid = TRUE;

  switch ( pAttrInfo->dataType )
  {
    case ZCL_DATATYPE_BOOLEAN:
      if ( ( *(pAttrInfo->attrData) != 0 ) && ( *(pAttrInfo->attrData) != 1 ) )
        valid = FALSE;
      break;

    default:
      break;
  }

  return ( valid );
}


/*********************************************************************
 * 函数名称：rangeext_BasicResetCB
 * 功    能：ZCL通用簇库回调函数。设置所有簇的所有属性为出厂默认设置。
 * 入口参数：无          
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_BasicResetCB( void )
{
  // 在此添加代码
}


/*********************************************************************
 * 函数名称：rangeext_IdentifyCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别命令后被调用。
 * 入口参数：pCmd      指向识别命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_IdentifyCB( zclIdentify_t *pCmd )
{
  rangeExtIdentifyTime = pCmd->identifyTime;
  rangeext_ProcessIdentifyTimeChange();
}


/*********************************************************************
 * 函数名称：rangeext_IdentifyQueryRspCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别查询命令
 *           后被调用。
 * 入口参数：pRsp    指向识别查询响应结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：rangeext_AlarmCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序报警请求命令
 *           后被调用。
 * 入口参数：pAlarm   指向报警命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_AlarmCB( zclAlarm_t *pAlarm )
{
  // 在这里添加用户代码
}


/* 
  处理ZCL基础输入命令/响应消息
 */
/*********************************************************************
 * 函数名称：rangeext_ProcessZCLMsg
 * 功    能：处理ZCL基础输入消息函数
 * 入口参数：inMsg           待处理的消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rangeext_ProcessZCLMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#if defined ( ZCL_READ )
    case ZCL_CMD_READ_RSP:
      rangeext_ProcessInReadRspCmd( pInMsg );
      break;
#endif 

#if defined ( ZCL_WRITE )
    case ZCL_CMD_WRITE_RSP:
      rangeext_ProcessInWriteRspCmd( pInMsg );
      break;
#endif 

    case ZCL_CMD_DEFAULT_RSP:
      rangeext_ProcessInDefaultRspCmd( pInMsg );
      break;

#if defined ( ZCL_DISCOVER )
    case ZCL_CMD_DISCOVER_RSP:
      rangeext_ProcessInDiscRspCmd( pInMsg );
      break;
#endif

    default:
      break;
  }

  if ( pInMsg->attrCmd != NULL )
  {
    /* 释放存储器命令 */
    osal_mem_free( pInMsg->attrCmd );
    pInMsg->attrCmd = NULL;
  }
}


#if defined ( ZCL_READ )
/*********************************************************************
 * 函数名称：rangeext_ProcessInReadRspCmd
 * 功    能：处理配置文件读响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 rangeext_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8 i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
    // 在此添加代码
  }

  return TRUE;
}
#endif 


#if defined ( ZCL_WRITE )
/*********************************************************************
 * 函数名称：rangeext_ProcessInWriteRspCmd
 * 功    能：处理配置文件写响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 rangeext_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8 i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < writeRspCmd->numAttr; i++)
  {
    // 在此添加代码
  }

  return TRUE;
}
#endif


/*********************************************************************
 * 函数名称：rangeext_ProcessInDefaultRspCmd
 * 功    能：处理配置文件默认响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 rangeext_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  return TRUE;
}


#if defined ( ZCL_DISCOVER )
/*********************************************************************
 * 函数名称：rangeext_ProcessInDiscRspCmd
 * 功    能：处理配置文件发现响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 rangeext_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverRspCmd_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // 设备被属性发现命令结果所通知
  }

  return TRUE;
}
#endif 
