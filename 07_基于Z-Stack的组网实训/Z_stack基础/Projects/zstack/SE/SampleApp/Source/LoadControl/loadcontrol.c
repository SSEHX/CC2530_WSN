/*******************************************************************************
 * 文件名称：loadcontrol.c
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
#include "loadcontrol.h"
#include "zcl_general.h"
#include "zcl_se.h"
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
static uint8 loadControlTaskID;                // LCD负载控制器操作系统任务ID
static devStates_t loadControlNwkState;        // 网络状态变量
static uint8 loadControlTransID;               // 传输id
static afAddrType_t ESPAddr;                   // ESP目的地址
static uint8 linkKeyStatus;                    // 从获取链接密钥函数返回的状态变量
static zclCCReportEventStatus_t rsp;           // 发送选项字段
/********************************************************************/


/* 本地函数 */
/********************************************************************/
static void loadcontrol_HandleKeys( uint8 shift, uint8 keys );

#if defined (ZCL_KEY_ESTABLISH)
static uint8 loadcontrol_KeyEstablish_ReturnLinkKey( uint16 shortAddr );
#endif 

#if defined ( ZCL_ALARMS )
static void loadcontrol_ProcessAlarmCmd( uint8 srcEP, afAddrType_t *dstAddr,
                                         uint16 clusterID, zclFrameHdr_t *hdr, 
                                         uint8 len, uint8 *data );
#endif 

static void loadcontrol_ProcessIdentifyTimeChange( void );
/********************************************************************/

/* 本地函数 - 应用回调函数 */
/********************************************************************/
/*
   基本回调函数
*/
static uint8 loadcontrol_ValidateAttrDataCB( zclAttrRec_t *pAttr, 
                                             zclWriteRec_t *pAttrInfo );

/*
   通用簇回调函数
*/
static void loadcontrol_BasicResetCB( void );
static void loadcontrol_IdentifyCB( zclIdentify_t *pCmd );
static void loadcontrol_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp );
static void loadcontrol_AlarmCB( zclAlarm_t *pAlarm );

/*
   SE回调函数
*/
static void loadcontrol_LoadControlEventCB( zclCCLoadControlEvent_t *pCmd,
                                          afAddrType_t *srcAddr, uint8 status, uint8 seqNum);
static void loadcontrol_CancelLoadControlEventCB( zclCCCancelLoadControlEvent_t *pCmd,
                                          afAddrType_t *srcAddr, uint8 seqNum );
static void loadcontrol_CancelAllLoadControlEventsCB( zclCCCancelAllLoadControlEvents_t *pCmd,
                                          afAddrType_t *srcAddr, uint8 seqNum);
static void loadcontrol_ReportEventStatusCB( zclCCReportEventStatus_t *pCmd,
                                          afAddrType_t *srcAddr, uint8 seqNum );
static void loadcontrol_GetScheduledEventCB( zclCCGetScheduledEvent_t *pCmd,
                                          afAddrType_t *srcAddr, uint8 seqNum);


/* 处理ZCL基本函数 - 输入命令/响应消息 */
/********************************************************************/
static void loadcontrol_ProcessZCLMsg( zclIncomingMsg_t *msg );

#if defined ( ZCL_READ )
static uint8 loadcontrol_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif

#if defined ( ZCL_WRITE )
static uint8 loadcontrol_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif 

static uint8 loadcontrol_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );

#if defined ( ZCL_DISCOVER )
static uint8 loadcontrol_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg );
#endif 
/********************************************************************/


/*
   ZCL通用簇回调表
*/
static zclGeneral_AppCallbacks_t loadcontrol_GenCmdCallbacks =
{
  loadcontrol_BasicResetCB,              // 基本簇复位命令
  loadcontrol_IdentifyCB,                // 识别命令
  loadcontrol_IdentifyQueryRspCB,        // 识别查询响应命令
  NULL,                                  // 开关簇命令
  NULL,                                  // 级别控制离开命令
  NULL,                                  // 级别控制移动命令
  NULL,                                  // 级别控制单步命令
  NULL,                                  // 级别控制停止命令
  NULL,                                  // 组响应命令
  NULL,                                  // 场景保存请求命令
  NULL,                                  // 场景重调用请求命令
  NULL,                                  // 场景响应命令
  loadcontrol_AlarmCB,                   // 报警(响应)命令
  NULL,                                  // RSSI定位命令
  NULL,                                  // RSSI定位响应命令
};

/*
   ZCL SE簇回调表
*/
static zclSE_AppCallbacks_t loadcontrol_SECmdCallbacks =			
{
  NULL,                                       // 获取 Profile 命令
  NULL,                                       // 获取 Profile 响应
  NULL,                                       // 请求镜像命令
  NULL,                                       // 请求镜像响应
  NULL,                                       // 镜像移除命令
  NULL,                                       // 镜像移除响应
  NULL,                                       // 获取当前价格
  NULL,                                       // 获取预定价格
  NULL,                                       // 公布价格
  NULL,                                       // 显示消息命令
  NULL,                                       // 取消消息命令
  NULL,                                       // 获取最后一个消息命令
  NULL,                                       // 消息确认
  loadcontrol_LoadControlEventCB,             // 负载控制事件
  loadcontrol_CancelLoadControlEventCB,       // 退出负载控制事件
  loadcontrol_CancelAllLoadControlEventsCB,   // 退出所有负载控制事件
  loadcontrol_ReportEventStatusCB,            // 报告事件状态
  loadcontrol_GetScheduledEventCB,            // 获取预定事件
};



/*********************************************************************
 * 函数名称：loadcontrol_Init
 * 功    能：LCD的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void loadcontrol_Init( uint8 task_id )
{
  loadControlTaskID = task_id;

  // 设置ESP目的地址
  ESPAddr.addrMode = (afAddrMode_t)Addr16Bit;
  ESPAddr.endPoint = LOADCONTROL_ENDPOINT;
  ESPAddr.addr.shortAddr = 0;

  /* 注册一个SE端点 */
  zclSE_Init( &loadControlSimpleDesc );

  /* 注册ZCL通用簇库回调函数 */
  zclGeneral_RegisterCmdCallbacks( LOADCONTROL_ENDPOINT, &loadcontrol_GenCmdCallbacks );

  /* 注册ZCL SE簇库回调函数 */
  zclSE_RegisterCmdCallbacks( LOADCONTROL_ENDPOINT, &loadcontrol_SECmdCallbacks );

  /* 注册应用程序属性列表 */
  zcl_registerAttrList( LOADCONTROL_ENDPOINT, LOADCONTROL_MAX_ATTRIBUTES, loadControlAttrs );

  /* 注册应用程序簇选项列表 */
  zcl_registerClusterOptionList( LOADCONTROL_ENDPOINT, LOADCONTROL_MAX_OPTIONS, loadControlOptions );

  /* 注册应用程序属性数据验证回调函数 */
  zcl_registerValidateAttrData( loadcontrol_ValidateAttrDataCB );

  /* 注册应用程序来接收未处理基础命令/响应消息 */
  zcl_registerForMsg( loadControlTaskID );

  /* 注册所有按键事件 - 将处理所有按键事件 */
  RegisterForKeys( loadControlTaskID );

  /* 启动定时器使用操作系统时钟来同步LCD定时器 */
  osal_start_timerEx( loadControlTaskID, LOADCONTROL_UPDATE_TIME_EVT, LOADCONTROL_UPDATE_TIME_PERIOD );
}


/*********************************************************************
 * 函数名称：loadcontrol_event_loop
 * 功    能：LCD的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
uint16 loadcontrol_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( loadControlTaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          /* 输入ZCL基础命令/响应消息 */
          loadcontrol_ProcessZCLMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          /* 按键处理 */
          loadcontrol_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          loadControlNwkState = (devStates_t)(MSGpkt->hdr.status);

          if (ZG_SECURE_ENABLED)
          {
            if ( loadControlNwkState == DEV_ROUTER )
            {
              /* 检查链接密钥是否已经被建立 */
              linkKeyStatus = loadcontrol_KeyEstablish_ReturnLinkKey(ESPAddr.addr.shortAddr);

              if (linkKeyStatus != ZSuccess)
              {
                /* 发送密钥建立请求 */
                osal_set_event( loadControlTaskID, LOADCONTROL_KEY_ESTABLISHMENT_REQUEST_EVT);
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
  if ( events & LOADCONTROL_KEY_ESTABLISHMENT_REQUEST_EVT )
  {
    zclGeneral_KeyEstablish_InitiateKeyEstablishment(loadControlTaskID, 
                                         &ESPAddr, loadControlTransID);

    return ( events ^ LOADCONTROL_KEY_ESTABLISHMENT_REQUEST_EVT );
  }

  /* 被一个识别命令触发的识别超时事件处理 */
  if ( events & LOADCONTROL_IDENTIFY_TIMEOUT_EVT )
  {
    if ( loadControlIdentifyTime > 0 )
    {
      loadControlIdentifyTime--;
    }
    loadcontrol_ProcessIdentifyTimeChange();

    return ( events ^ LOADCONTROL_IDENTIFY_TIMEOUT_EVT );
  }

  /* 获取当前时间事件 */
  if ( events & LOADCONTROL_UPDATE_TIME_EVT )
  {
    loadControlTime = osal_getClock();
    osal_start_timerEx( loadControlTaskID, LOADCONTROL_UPDATE_TIME_EVT, 
                                           LOADCONTROL_UPDATE_TIME_PERIOD );

    return ( events ^ LOADCONTROL_UPDATE_TIME_EVT );
  }

  /* 处理负载控制器完毕事件 */ 
  if ( events & LOADCONTROL_LOAD_CTRL_EVT )
  {
    /* 
      发送应答响应
      DisableDefaultResponse 标志位被设置为假 - 我们建议打开默认响应，
      因为报告事件状态的命令没有作出响应
     */

    rsp.eventStatus = EVENT_STATUS_LOAD_CONTROL_EVENT_COMPLETED;
    zclSE_LoadControl_Send_ReportEventStatus( LOADCONTROL_ENDPOINT, &ESPAddr,
                                            &rsp, false, loadControlTransID );
    /* LCD界面显示完成该事件 */ 
    HalLcdWriteString("Load Evt Complete", HAL_LCD_LINE_6);
    HalLedSet(HAL_LED_4, HAL_LED_MODE_OFF);

    return ( events ^ LOADCONTROL_LOAD_CTRL_EVT );
  }

  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：loadcontrol_ProcessIdentifyTimeChange
 * 功    能：调用闪烁LED来指定识别时间属性值。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_ProcessIdentifyTimeChange( void )
{
  if ( loadControlIdentifyTime > 0 )
  {
    osal_start_timerEx( loadControlTaskID, LOADCONTROL_IDENTIFY_TIMEOUT_EVT, 1000 );
    HalLedBlink ( HAL_LED_4, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
    osal_stop_timerEx( loadControlTaskID, LOADCONTROL_IDENTIFY_TIMEOUT_EVT );
  }
}


#if defined (ZCL_KEY_ESTABLISH)
/*********************************************************************
 * 函数名称：loadcontrol_KeyEstablish_ReturnLinkKey
 * 功    能：获取被请求的密钥函数。
 * 入口参数：shortAddr 短地址
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 loadcontrol_KeyEstablish_ReturnLinkKey( uint16 shortAddr )
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
 * 函数名称：loadcontrol_HandleKeys
 * 功    能：ESP的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_2     SK-SmartRF10BB的K2键       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_HandleKeys( uint8 shift, uint8 keys )
{
  if ( keys & HAL_KEY_SW_2 )
  {
    ZDOInitDevice(0); // 加入网络
  }
}


/*********************************************************************
 * 函数名称：loadcontrol_ValidateAttrDataCB
 * 功    能：检查为属性提供的数据值是否在该属性指定的范围内。
 * 入口参数：pAttr     指向属性的指针
 *           pAttrInfo 指向属性信息的指针            
 * 出口参数：无
 * 返 回 值：TRUE      有效数据
 *           FALSE     无效数据
 ********************************************************************/
static uint8 loadcontrol_ValidateAttrDataCB( zclAttrRec_t *pAttr, 
                                             zclWriteRec_t *pAttrInfo )
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
 * 函数名称：loadcontrol_BasicResetCB
 * 功    能：ZCL通用簇库回调函数。设置所有簇的所有属性为出厂默认设置。
 * 入口参数：无          
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_BasicResetCB( void )
{
  // 在此添加代码
}


/*********************************************************************
 * 函数名称：loadcontrol_IdentifyCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别命令后被调用。
 * 入口参数：pCmd      指向识别命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_IdentifyCB( zclIdentify_t *pCmd )
{
  loadControlIdentifyTime = pCmd->identifyTime;
  loadcontrol_ProcessIdentifyTimeChange();
}


/*********************************************************************
 * 函数名称：loadcontrol_IdentifyQueryRspCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别查询命令
 *           后被调用。
 * 入口参数：pRsp    指向识别查询响应结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：loadcontrol_AlarmCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序报警请求命令
 *           后被调用。
 * 入口参数：pAlarm   指向报警命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_AlarmCB( zclAlarm_t *pAlarm )
{
  // 在这里添加用户代码
}


#if defined (ZCL_LOAD_CONTROL)
/*********************************************************************
 * 函数名称：loadcontrol_SendReportEventStatus
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到一个负载事件命令后被调用。
 * 入口参数：srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 *           eventID          该事件的ID
 *           startTime        该事件启动时间
 *           eventStatus      事件状态
 *           criticalityLevel 时间临界级别
 *　　　　   eventControl 　　该事件的事件控制
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_SendReportEventStatus( afAddrType_t *srcAddr, uint8 seqNum,
                                               uint32 eventID, uint32 startTime,
                                               uint8 eventStatus, uint8 criticalityLevel,
                                               uint8 eventControl )
{
  /* 强制性字段 - 使用输入数据 */
  rsp.issuerEventID = eventID;
  rsp.eventStartTime = startTime;
  rsp.criticalityLevelApplied = criticalityLevel;
  rsp.eventControl = eventControl;
  rsp.eventStatus = eventStatus;
  rsp.signatureType = SE_PROFILE_SIGNATURE_TYPE_ECDSA;

  /* loadcontrol_Signature 为静态数组，可在loadcontrol_data.c文件中更改 */
  osal_memcpy( rsp.signature, loadControlSignature, SE_PROFILE_SIGNATURE_LENGTH );

  /* 可选性字段 - 默认使用未使用值来填充 */
  rsp.coolingTemperatureSetPointApplied = SE_OPTIONAL_FIELD_TEMPERATURE_SET_POINT;
  rsp.heatingTemperatureSetPointApplied = SE_OPTIONAL_FIELD_TEMPERATURE_SET_POINT;
  rsp.averageLoadAdjustment = SE_OPTIONAL_FIELD_INT8;
  rsp.dutyCycleApplied = SE_OPTIONAL_FIELD_UINT8;

  /* 
    发送应答响应
    DisableDefaultResponse 标志位被设置为假 - 我们建议打开默认响应，
    因为报告事件状态的命令没有作出响应
   */
  zclSE_LoadControl_Send_ReportEventStatus( LOADCONTROL_ENDPOINT, srcAddr,
                                            &rsp, false, seqNum );
}
#endif


/*********************************************************************
 * 函数名称：loadcontrol_LoadControlEventCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个负载控制事件命令后被调用。
 * 入口参数：pCmd             指向负载控制事件命令
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 *           Status           事件状态
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_LoadControlEventCB( zclCCLoadControlEvent_t *pCmd,
                                            afAddrType_t *srcAddr, uint8 status,
                                            uint8 seqNum)
{
#if defined ( ZCL_LOAD_CONTROL )
  /* 
    根据智能能源说明，在获取到负载控制事件命令后，设备应当回发
    报告事件状态命令
   */  
  uint8 eventStatus;

  if ( status == ZCL_STATUS_INVALID_FIELD )
  {
    /* 如果输入信息为无效字段，那么将回发驳回状态 */
    eventStatus = EVENT_STATUS_LOAD_CONTROL_EVENT_REJECTED;
  }
  else
  {
    /* 如果输入信息为有效字段，那么将回发接收状态 */
    eventStatus = EVENT_STATUS_LOAD_CONTROL_EVENT_RECEIVED;
  }

  /* 回发响应 */
  loadcontrol_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEvent,
                                     pCmd->startTime, eventStatus,
                                     pCmd->criticalityLevel, pCmd->eventControl);

  if ( status != ZCL_STATUS_INVALID_FIELD )
  {
    /* 启动负载控制事件 */
    if ( pCmd->issuerEvent == LOADCONTROL_EVENT_ID )
    {
      if ( pCmd->startTime == START_TIME_NOW ) // 启动时间 = 现在
      {
        /* 回发状态事件 = 负载控制事件开始 */
        eventStatus = EVENT_STATUS_LOAD_CONTROL_EVENT_STARTED;
        loadcontrol_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEvent,
                                           pCmd->startTime, eventStatus,
                                           pCmd->criticalityLevel, pCmd->eventControl);
        /* 是否是住宅开关负载 */
        if ( pCmd->deviceGroupClass == ONOFF_LOAD_DEVICE_CLASS ) 
        {
          HalLcdWriteString("Load Evt Started", HAL_LCD_LINE_6);
        }
        /* 是否是HVAC压缩机/炉开关负载 */
        else if ( pCmd->deviceGroupClass == HVAC_DEVICE_CLASS )
        {
          HalLcdWriteString("PCT Evt Started", HAL_LCD_LINE_6);
        }

        HalLedBlink ( HAL_LED_4, 0, 50, 500 );

        osal_start_timerEx( loadControlTaskID, LOADCONTROL_LOAD_CTRL_EVT,
                          ( LOADCONTROL_LOAD_CTRL_PERIOD * (pCmd->durationInMinutes)) );
      }
    }
  }
#endif
}


/*********************************************************************
 * 函数名称：loadcontrol_CancelLoadControlEventCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个取消负载控制事件命令后被调用。
 * 入口参数：pCmd             指向取消负载控制事件命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_CancelLoadControlEventCB( zclCCCancelLoadControlEvent_t *pCmd,
                                                afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( ZCL_LOAD_CONTROL )
  if ( 0 )  // 如果要取消该事件，则使用事件存在标志代替 "0"
  {
    /* 用户可在此添加取消事件代码 */ 

    // 以下是列子代码，用来回发响应
    /*
    loadcontrol_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEventID,
                                     // startTime
                                     EVENT_STATUS_LOAD_CONTROL_EVENT_CANCELLED, // eventStatus
                                     // Criticality level
                                     // eventControl };
    */
  }
  else
  {
    /* 如果事件不存在，应答状态：驳回请求 */ 
    loadcontrol_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEventID,
                                     SE_OPTIONAL_FIELD_UINT32,                 // 启动时间
                                     EVENT_STATUS_LOAD_CONTROL_EVENT_RECEIVED, // 事件状态
                                     SE_OPTIONAL_FIELD_UINT8,                  // 临界状态等级
                                     SE_OPTIONAL_FIELD_UINT8 );                // 事件控制
  }
#endif
}


/*********************************************************************
 * 函数名称：loadcontrol_CancelAllLoadControlEventsCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个取消所有负载控制事件命令后被调用。
 * 入口参数：pCmd             指向取消所有负载控制事件命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_CancelAllLoadControlEventsCB( zclCCCancelAllLoadControlEvents_t *pCmd,
                                                      afAddrType_t *srcAddr, uint8 seqNum )
{
  /* 
    在接收到取消所有负载控制事件命令后，接收到命令的设备应查询所有
    事件列表，并且给每个事件分别发送响应 
   */
}


/*********************************************************************
 * 函数名称：loadcontrol_ReportEventStatusCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个报告事件状态事件命令后被调用。
 * 入口参数：pCmd             指向报告事件状态命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_ReportEventStatusCB( zclCCReportEventStatus_t *pCmd,
                                             afAddrType_t *srcAddr, uint8 seqNum)
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：loadcontrol_GetScheduledEventCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个获取预置事件命令后被调用。
 * 入口参数：pCmd             指向获取预置事件命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_GetScheduledEventCB( zclCCGetScheduledEvent_t *pCmd,
                                             afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}



/* 
  处理ZCL基础输入命令/响应消息
 */
/*********************************************************************
 * 函数名称：loadcontrol_ProcessZCLMsg
 * 功    能：处理ZCL基础输入消息函数
 * 入口参数：inMsg           待处理的消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void loadcontrol_ProcessZCLMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#if defined ( ZCL_READ )
    case ZCL_CMD_READ_RSP:
      loadcontrol_ProcessInReadRspCmd( pInMsg );
      break;
#endif 

#if defined ( ZCL_WRITE )
    case ZCL_CMD_WRITE_RSP:
      loadcontrol_ProcessInWriteRspCmd( pInMsg );
      break;
#endif 

    case ZCL_CMD_DEFAULT_RSP:
      loadcontrol_ProcessInDefaultRspCmd( pInMsg );
      break;

#if defined ( ZCL_DISCOVER )
    case ZCL_CMD_DISCOVER_RSP:
      loadcontrol_ProcessInDiscRspCmd( pInMsg );
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
 * 函数名称：loadcontrol_ProcessInReadRspCmd
 * 功    能：处理配置文件读响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 loadcontrol_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：loadcontrol_ProcessInWriteRspCmd
 * 功    能：处理配置文件写响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 loadcontrol_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：loadcontrol_ProcessInDefaultRspCmd
 * 功    能：处理配置文件默认响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 loadcontrol_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  return TRUE;
}


#if defined ( ZCL_DISCOVER )
/*********************************************************************
 * 函数名称：loadcontrol_ProcessInDiscRspCmd
 * 功    能：处理配置文件发现响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 loadcontrol_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg )
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