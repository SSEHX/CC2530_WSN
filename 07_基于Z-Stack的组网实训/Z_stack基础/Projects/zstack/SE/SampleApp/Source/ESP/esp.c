/*******************************************************************************
 * 文件名称：esp.c
 * 功    能：SE智能能源实验。
 * 实验内容：ESP(能源服务门户)建立一个ZigBeePRO网路。其他设备控制器，比如IPD(
 *           预装显示设备)、PCT(可编程通信温控器)等发现该网络，并且加入到该网络中。
 *           ESP(能源服务门户)将通过无线方式管理其他设备。
 * 实验设备：SK-SmartRF05EB（装配有SK-CC2530EM）
 *           UP键：            发送制冷负载控制事件给PCT(可编程通信温控器)
 *           RIGHT键：         发送负载控制事件给负载控制器
 *           DOWN键：          发送消息到IPD(预装显示设备)
 *           LEFT键：          发送发现服务消息来获取PCT(可编程通信温控器)和
 *                             负载控制器的16位网络地址
 *           LED1(绿色)：      设备成功加入组1被点亮，退出组1后熄灭
 *           LED3(黄色)：      当成功建立网络或加入网络后此LED点亮
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "MT.h"
#include "MT_APP.h"
#include "ZDObject.h"
#include "AddrMgr.h"

#include "se.h"
#include "esp.h"
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

/*
   ESP最小间隔报告数
*/
#define ESP_MIN_REPORTING_INTERVAL       5


/* 本地变量 */
/********************************************************************/
static uint8 espTaskID;                              // esp操作系统任务ID
static afAddrType_t ipdAddr;                         // IPD的目的地址
static afAddrType_t pctAddr;                         // PCT的目的地址
static afAddrType_t loadControlAddr;                 // LCD负载控制器的目的地址
static zAddrType_t simpleDescReqAddr[2];             // 简单描述请求目的地址
static uint8 matchRspCount = 0;                      // 接收到的匹配响应数量
static zAddrType_t dstMatchAddr;                     // 匹配描述符请求的通用目的地址
static zclCCLoadControlEvent_t loadControlCmd;       // 负载控制命令的命令结构

/*
   匹配描述符请求簇列表
*/
static const cId_t matchClusterList[1] =
{
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL
};


/* 本地函数 */
/********************************************************************/
static void esp_HandleKeys( uint8 shift, uint8 keys );
static void esp_ProcessAppMsg( uint8 *msg );

#if defined (ZCL_KEY_ESTABLISH)
static uint8 esp_KeyEstablish_ReturnLinkKey( uint16 shortAddr );
#endif

#if defined (ZCL_ALARMS)
static void esp_ProcessAlarmCmd( uint8 srcEP, afAddrType_t *dstAddr,
                        uint16 clusterID, zclFrameHdr_t *hdr, uint8 len, uint8 *data );
#endif

static void esp_ProcessIdentifyTimeChange( void );


/* 本地函数 - 应用回调函数 */
/********************************************************************/

/*
   基本回调函数
*/
static uint8 esp_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo );

/*
   通用簇回调函数
*/
static void esp_BasicResetCB( void );
static void esp_IdentifyCB( zclIdentify_t *pCmd );
static void esp_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp );
static void esp_AlarmCB( zclAlarm_t *pAlarm );

/*
   SE回调函数
*/
static void esp_GetProfileCmdCB( zclCCGetProfileCmd_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_GetProfileRspCB( zclCCGetProfileRsp_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_ReqMirrorCmdCB( afAddrType_t *srcAddr, uint8 seqNum );
static void esp_ReqMirrorRspCB( zclCCReqMirrorRsp_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_MirrorRemCmdCB( afAddrType_t *srcAddr, uint8 seqNum );
static void esp_MirrorRemRspCB( zclCCMirrorRemRsp_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_GetCurrentPriceCB( zclCCGetCurrentPrice_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_GetScheduledPriceCB( zclCCGetScheduledPrice_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_PublishPriceCB( zclCCPublishPrice_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_DisplayMessageCB( zclCCDisplayMessage_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_CancelMessageCB( zclCCCancelMessage_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_GetLastMessageCB( afAddrType_t *srcAddr, uint8 seqNum );
static void esp_MessageConfirmationCB( zclCCMessageConfirmation_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_LoadControlEventCB( zclCCLoadControlEvent_t *pCmd,
                          afAddrType_t *srcAddr, uint8 status, uint8 seqNum);
static void esp_CancelLoadControlEventCB( zclCCCancelLoadControlEvent_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_CancelAllLoadControlEventsCB( zclCCCancelAllLoadControlEvents_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum);
static void esp_ReportEventStatusCB( zclCCReportEventStatus_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void esp_GetScheduledEventCB( zclCCGetScheduledEvent_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum);


/* 处理ZCL基本函数 - 输入命令/响应消息 */
/********************************************************************/
static void esp_ProcessZCLMsg( zclIncomingMsg_t *msg );

#if defined ( ZCL_READ )
static uint8 esp_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif

#if defined ( ZCL_WRITE )
static uint8 esp_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif 

#if defined ( ZCL_REPORT )
static uint8 esp_ProcessInConfigReportCmd( zclIncomingMsg_t *pInMsg );
static uint8 esp_ProcessInConfigReportRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 esp_ProcessInReadReportCfgCmd( zclIncomingMsg_t *pInMsg );
static uint8 esp_ProcessInReadReportCfgRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 esp_ProcessInReportCmd( zclIncomingMsg_t *pInMsg );
#endif 

static uint8 esp_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );

#if defined ( ZCL_DISCOVER )
static uint8 esp_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg );
#endif 


/*
   ZDO消息句柄函数
*/
static void esp_ProcessZDOMsg( zdoIncomingMsg_t *inMsg );

/*
   ZCL通用簇回调表
*/
static zclGeneral_AppCallbacks_t esp_GenCmdCallbacks =
{
  esp_BasicResetCB,              // 基本的簇复位命令
  esp_IdentifyCB,                // 识别命令
  esp_IdentifyQueryRspCB,        // 识别查询响应命令
  NULL,                          // 开关簇命令
  NULL,                          // 级别控制离开命令
  NULL,                          // 级别控制移动命令
  NULL,                          // 级别控制单步命令
  NULL,                          // 级别控制停止命令
  NULL,                          // 组响应命令
  NULL,                          // 场景保存请求命令
  NULL,                          // 场景重调用请求命令
  NULL,                          // 场景响应命令
  esp_AlarmCB,                   // 报警(响应)命令
  NULL,                          // RSSI定位命令
  NULL,                          // RSSI定位响应命令
};

/*
   ZCL SE簇回调表
*/
static zclSE_AppCallbacks_t esp_SECmdCallbacks =			
{
  esp_GetProfileCmdCB,                     // 获取 Profile 命令
  esp_GetProfileRspCB,                     // 获取 Profile 响应
  esp_ReqMirrorCmdCB,                      // 请求镜像命令
  esp_ReqMirrorRspCB,                      // 请求镜像响应
  esp_MirrorRemCmdCB,                      // 镜像移除命令
  esp_MirrorRemRspCB,                      // 镜像移除响应
  esp_GetCurrentPriceCB,                   // 获取当前价格
  esp_GetScheduledPriceCB,                 // 获取预定价格
  esp_PublishPriceCB,                      // 公布价格
  esp_DisplayMessageCB,                    // 显示消息命令
  esp_CancelMessageCB,                     // 取消消息命令
  esp_GetLastMessageCB,                    // 获取最后一个消息命令
  esp_MessageConfirmationCB,               // 消息确认
  esp_LoadControlEventCB,                  // 负载控制事件
  esp_CancelLoadControlEventCB,            // 退出负载控制事件
  esp_CancelAllLoadControlEventsCB,        // 退出所有负载控制事件
  esp_ReportEventStatusCB,                 // 报告事件状态
  esp_GetScheduledEventCB,                 // 获取预定事件
};


/*********************************************************************
 * 函数名称：esp_Init
 * 功    能：ESP的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void esp_Init( uint8 task_id )
{
  espTaskID = task_id;

  /* 注册一个SE端点 */
  zclSE_Init( &espSimpleDesc );

  /* 注册ZCL通用簇库回调函数 */
  zclGeneral_RegisterCmdCallbacks( ESP_ENDPOINT, &esp_GenCmdCallbacks );

  /* 注册ZCL SE簇库回调函数 */
  zclSE_RegisterCmdCallbacks( ESP_ENDPOINT, &esp_SECmdCallbacks );

  /* 注册应用程序属性列表 */
  zcl_registerAttrList( ESP_ENDPOINT, ESP_MAX_ATTRIBUTES, espAttrs );

  /* 注册应用程序簇选项列表 */
  zcl_registerClusterOptionList( ESP_ENDPOINT, ESP_MAX_OPTIONS, espOptions );

  /* 注册应用程序属性数据验证回调函数 */
  zcl_registerValidateAttrData( esp_ValidateAttrDataCB );

  /* 注册应用程序来接收未处理基础命令/响应消息 */
  zcl_registerForMsg( espTaskID );

  /* 注册匹配描述符和简单描述响应 */
  ZDO_RegisterForZDOMsg( espTaskID, Match_Desc_rsp );
  ZDO_RegisterForZDOMsg( espTaskID, Simple_Desc_rsp );

  /* 注册所有按键事件 - 将处理所有按键事件 */
  RegisterForKeys( espTaskID );

  /* 启动定时器使用操作系统时钟来同步ESP定时器 */
  osal_start_timerEx( espTaskID, ESP_UPDATE_TIME_EVT, ESP_UPDATE_TIME_PERIOD );

  /* 为PCT设置地址模式和目的端点字段 */
  pctAddr.addrMode = (afAddrMode_t)Addr16Bit;
  pctAddr.endPoint = ESP_ENDPOINT;

  /* 为LCD负载控制设置地址模式和目的端点字段 */
  loadControlAddr.addrMode = (afAddrMode_t)Addr16Bit;
  loadControlAddr.endPoint = ESP_ENDPOINT;

  /* 设置负载控制命令结构 */
  loadControlCmd.issuerEvent = 0x12345678;            // 任意id
  loadControlCmd.deviceGroupClass = 0x000000;         // 所有组地址
  loadControlCmd.startTime = 0x00000000;              // 开始时间 - 现在
  loadControlCmd.durationInMinutes = 0x0001;          // 持续时间1分钟
  loadControlCmd.criticalityLevel = 0x01;             // 绿色水平
  loadControlCmd.coolingTemperatureSetPoint = 0x076C; // 19摄氏度，66.2华氏
  loadControlCmd.eventControl = 0x00;                 // 没有随机启动或结束应用
  
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString("SE(ESP)", HAL_LCD_LINE_2 );
#endif
}


/*********************************************************************
 * 函数名称：esp_event_loop
 * 功    能：ESP的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
uint16 esp_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( espTaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case MT_SYS_APP_MSG:
          /* 从MT(串口)接收到消息 */
          esp_ProcessAppMsg( ((mtSysAppMsg_t *)MSGpkt)->appData );
          break;

        case ZCL_INCOMING_MSG:
          /* 输入ZCL基础命令/响应消息 */
          esp_ProcessZCLMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          /* 按键处理 */
          esp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_CB_MSG:
          /* ZDO发送注册消息 */ 
          esp_ProcessZDOMsg( (zdoIncomingMsg_t *)MSGpkt );
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt ); // 释放存储器
    }

    return (events ^ SYS_EVENT_MSG); // 返回未处理的事件
  }

  /* 处理由识别命令触发的识别超时事件 */
  if ( events & ESP_IDENTIFY_TIMEOUT_EVT )
  {
    if ( espIdentifyTime > 0 )
    {
      espIdentifyTime--;
    }
    esp_ProcessIdentifyTimeChange();

    return ( events ^ ESP_IDENTIFY_TIMEOUT_EVT );
  }

  /* 获取当前时间事件 */
  if ( events & ESP_UPDATE_TIME_EVT )
  {
    espTime = osal_getClock();
    osal_start_timerEx( espTaskID, ESP_UPDATE_TIME_EVT, ESP_UPDATE_TIME_PERIOD );

    return ( events ^ ESP_UPDATE_TIME_EVT );
  }

  /* 获取第一个匹配响应的简单描述符事件 */
  if ( events & SIMPLE_DESC_QUERY_EVT_1 )
  {
    ZDP_SimpleDescReq( &simpleDescReqAddr[0], simpleDescReqAddr[0].addr.shortAddr,
                        ESP_ENDPOINT, 0);

    return ( events ^ SIMPLE_DESC_QUERY_EVT_1 );
  }

  /* 获取第二个匹配响应的简单描述符事件 */
  if ( events & SIMPLE_DESC_QUERY_EVT_2 )
  {
    ZDP_SimpleDescReq( &simpleDescReqAddr[1], simpleDescReqAddr[1].addr.shortAddr,
                        ESP_ENDPOINT, 0);

    return ( events ^ SIMPLE_DESC_QUERY_EVT_2 );
  }

  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：esp_ProcessAppMsg
 * 功    能：检索MT串口系统应用程序消息过程的关键环节
 * 入口参数：msg   指向消息的指针。
 *           0     目的地址低位字节
 *           1     目的地址高位字节
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ProcessAppMsg( uint8 *msg )
{
  afAddrType_t dstAddr;

  dstAddr.addr.shortAddr = BUILD_UINT16( msg[0], msg[1] );
  esp_KeyEstablish_ReturnLinkKey( dstAddr.addr.shortAddr );
}


/*********************************************************************
 * 函数名称：esp_ProcessIdentifyTimeChange
 * 功    能：调用闪烁LED来指定识别时间属性值。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ProcessIdentifyTimeChange( void )
{
  if ( espIdentifyTime > 0 )
  {
    osal_start_timerEx( espTaskID, ESP_IDENTIFY_TIMEOUT_EVT, 1000 );
    HalLedBlink ( HAL_LED_4, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
    osal_stop_timerEx( espTaskID, ESP_IDENTIFY_TIMEOUT_EVT );
  }
}


#if defined (ZCL_KEY_ESTABLISH)
/*********************************************************************
 * 函数名称：esp_KeyEstablish_ReturnLinkKey
 * 功    能：获取被请求的密钥函数。
 * 入口参数：shortAddr 短地址
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 esp_KeyEstablish_ReturnLinkKey( uint16 shortAddr )
{
  APSME_LinkKeyData_t* keyData;
  uint8 status = ZFailure;
  uint8 buf[1+SEC_KEY_LEN];
  uint8 len = 1;
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
      len += SEC_KEY_LEN; // 状态 + 密钥值
    }
  }
  else
  {
    status = ZInvalidParameter; // 未知设备
  }

  buf[0] = status;

  if( status == ZSuccess )
  {
    osal_memcpy( &(buf[1]), keyData->key, SEC_KEY_LEN );
  }

  MT_BuildAndSendZToolResponse( ((uint8)MT_RPC_CMD_AREQ | (uint8)MT_RPC_SYS_DBG), MT_DEBUG_MSG,
                                  len, buf );
  return status;
}
#endif


/*********************************************************************
 * 函数名称：esp_HandleKeys
 * 功    能：ESP的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_1     SK-SmartRF10GW的UP键
 *                  HAL_KEY_SW_2     SK-SmartRF10GW的RIGHT键      
 *                  HAL_KEY_SW_3     SK-SmartRF10GW的DOWN键
 *                  HAL_KEY_SW_4     SK-SmartRF10GW的LEFT键             
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_HandleKeys( uint8 shift, uint8 keys )
{

    if ( keys & HAL_KEY_SW_1 )
    {
      /* 发送制冷负载控制事件给PCT(可编程通信温控器) */
      loadControlCmd.deviceGroupClass = HVAC_DEVICE_CLASS; // HVAC 压缩机或炉 - 位0被设置
      zclSE_LoadControl_Send_LoadControlEvent( ESP_ENDPOINT, &pctAddr, &loadControlCmd, TRUE, 0 );
    }

    if ( keys & HAL_KEY_SW_2 )
    {
      /* 发送负载控制事件给负载控制器 */
      loadControlCmd.deviceGroupClass = ONOFF_LOAD_DEVICE_CLASS; // 简单住宅开关负载 - 位7被设置
      zclSE_LoadControl_Send_LoadControlEvent( ESP_ENDPOINT, &loadControlAddr, &loadControlCmd, TRUE, 0 );
    }

    if ( keys & HAL_KEY_SW_3 )
    {
      /* 预装显示设备 */
      zclCCDisplayMessage_t displayCmd;             // 将要被发送到IPD消息的命令结构
      uint8 msgString[]="Rcvd MESSAGE Cmd";         // IPD上显示的消息

      /* 在IPD上显示发送的消息 */
      displayCmd.msgString.strLen = sizeof(msgString);
      displayCmd.msgString.pStr = msgString;

      zclSE_Message_Send_DisplayMessage( ESP_ENDPOINT, &ipdAddr, &displayCmd, TRUE, 0 );
    }

    if ( keys & HAL_KEY_SW_4 )
    {
      /* 执行PCT和负载控制器的发现服务*/
      HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );

      /* 初始化一个匹配描述请求(服务发现) */
      dstMatchAddr.addrMode = AddrBroadcast;
      dstMatchAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
      ZDP_MatchDescReq( &dstMatchAddr, NWK_BROADCAST_SHORTADDR,
                        ZCL_SE_PROFILE_ID,
                        1, (cId_t *)matchClusterList,
                        1, (cId_t *)matchClusterList,
                        FALSE );
    }
}


/*********************************************************************
 * 函数名称：esp_ValidateAttrDataCB
 * 功    能：检查为属性提供的数据值是否在该属性指定的范围内。
 * 入口参数：pAttr     指向属性的指针
 *           pAttrInfo 指向属性信息的指针            
 * 出口参数：无
 * 返 回 值：TRUE      有效数据
 *           FALSE     无效数据
 ********************************************************************/
static uint8 esp_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo )
{
  uint8 valid = TRUE;

  switch ( pAttrInfo->dataType )
  {
    case ZCL_DATATYPE_BOOLEAN:
      if ( ( *(pAttrInfo->attrData) != 0 ) && ( *(pAttrInfo->attrData) != 1 ) )
      {
        valid = FALSE;
      }
      break;

    default:
      break;
  }

  return ( valid );
}


/*********************************************************************
 * 函数名称：esp_BasicResetCB
 * 功    能：ZCL通用簇库回调函数。设置所有簇的所有属性为出厂默认设置。
 * 入口参数：无          
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_BasicResetCB( void )
{
  // 用户应该在此处理回到出厂设置的属性
}


/*********************************************************************
 * 函数名称：esp_IdentifyCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别命令后被调用。
 * 入口参数：pCmd      指向识别命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_IdentifyCB( zclIdentify_t *pCmd )
{
  espIdentifyTime = pCmd->identifyTime;
  esp_ProcessIdentifyTimeChange();
}


/*********************************************************************
 * 函数名称：esp_IdentifyQueryRspCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别查询命令
 *           后被调用。
 * 入口参数：pRsp    指向识别查询响应结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_AlarmCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序报警请求命令
 *           后被调用。
 * 入口参数：pAlarm   指向报警命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_AlarmCB( zclAlarm_t *pAlarm )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_GetProfileCmdCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           获取配置文件后被调用。
 * 入口参数：pCmd     指向获取配置文件结构的指针     
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_GetProfileCmdCB( zclCCGetProfileCmd_t *pCmd, afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( ZCL_SIMPLE_METERING )

  /* 一旦执行获取配置文件命令，计量装置应发送获取配置文件响应回调 */

  /* 下面的变量被初始化为测试用 */
  /* 在实际应用中，用户应查找在 pCmd->endTime 指定周期内的数据变量，
     并且返回相应的数据 */

  uint32 endTime;
  uint8  status = zclSE_SimpleMeter_GetProfileRsp_Status_Success;
  uint8  profileIntervalPeriod = PROFILE_INTERVAL_PERIOD_60MIN;
  uint8  numberOfPeriodDelivered = 5;
  uint24 intervals[] = {0xa00001, 0xa00002, 0xa00003, 0xa00004, 0xa00005};

  // endTime: 32位值，表示按时间间隔顺序被请求的最近结束时间。
  // 举个例子: 从下午2时至下午3:00收集到的数据将作为下午3:00区间（结束时间）
  // 返回的时间间隔块应与其最近块的结束时间相同或者比请求中的pCmd->endTime时间更晚

  // 请求结束时间值为0xFFFFFFFF 表示最近间隔块将被请求
  // 代码例子 - 假设所请求的块结束时间同请求相同。

  endTime = pCmd->endTime;

  // 发送获取配置响应命令回调
  zclSE_SimpleMetering_Send_GetProfileRsp( ESP_ENDPOINT, srcAddr, endTime,
                                           status,
                                           profileIntervalPeriod,
                                           numberOfPeriodDelivered, intervals,
                                           false, seqNum );
#endif 
}


/*********************************************************************
 * 函数名称：esp_GetProfileRspCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           获取配置文件响应后被调用。
 * 入口参数：pCmd     指向获取配置文件响应结构的指针     
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_GetProfileRspCB( zclCCGetProfileRsp_t *pCmd, afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_ReqMirrorCmdCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像命令后被调用。
 * 入口参数：srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ReqMirrorCmdCB( afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_ReqMirrorRspCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像响应后被调用。
 * 入口参数：pRsp     指向请求镜像响应结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ReqMirrorRspCB( zclCCReqMirrorRsp_t *pRsp, afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_MirrorRemCmdCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像移除命令后被调用。
 * 入口参数：srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_MirrorRemCmdCB( afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_MirrorRemRspCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像移除响应后被调用。
 * 入口参数：pCmd     指向镜像移除响应结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_MirrorRemRspCB( zclCCMirrorRemRsp_t *pCmd, afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_GetCurrentPriceCB
 * 功    能：ZCL SE 配置文件定价簇库回调函数。当接收到一个应用程序
 *           接收到获取当前价格后被调用。
 * 入口参数：pCmd     指向获取当前价格命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_GetCurrentPriceCB( zclCCGetCurrentPrice_t *pCmd,
                                   afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( ZCL_PRICING )
  /* 在获取当前价格命令后，设备应当发送带有当前时间消息的公布价格命令 */  
  zclCCPublishPrice_t cmd;

  osal_memset( &cmd, 0, sizeof( zclCCPublishPrice_t ) );

  cmd.providerId = 0xbabeface;
  cmd.priceTier = 0xfe;

  /* 
    复制当前定价信息请求显示设备的源地址，使ESP可以发送消息，并使用
    IPDAddr的目的地址
   */  

  osal_memcpy( &ipdAddr, srcAddr, sizeof ( afAddrType_t ) );

  zclSE_Pricing_Send_PublishPrice( ESP_ENDPOINT, srcAddr, &cmd, false, seqNum );
#endif
}


/*********************************************************************
 * 函数名称：esp_GetScheduledPriceCB
 * 功    能：ZCL SE 配置文件定价簇库回调函数。当接收到一个应用程序
 *           接收到获取预定价格后被调用。
 * 入口参数：pCmd     指向获取预定价格命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_GetScheduledPriceCB( zclCCGetScheduledPrice_t *pCmd, 
                                     afAddrType_t *srcAddr, uint8 seqNum )
{
  /* 在获取预定价格命令后，设备将为当前预定价格事件发送公布价格命令 */ 
  /* 例子代码如下 */

#if defined ( ZCL_PRICING )
  zclCCPublishPrice_t cmd;

  osal_memset( &cmd, 0, sizeof( zclCCPublishPrice_t ) );

  cmd.providerId = 0xbabeface;
  cmd.priceTier = 0xfe;

  zclSE_Pricing_Send_PublishPrice( ESP_ENDPOINT, srcAddr, &cmd, false, seqNum );

#endif
}


/*********************************************************************
 * 函数名称：esp_PublishPriceCB
 * 功    能：ZCL SE 配置文件定价簇库回调函数。当接收到一个应用程序
 *           接收公布价格后被调用。
 * 入口参数：pCmd     指向公布价格命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_PublishPriceCB( zclCCPublishPrice_t *pCmd,
                                afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_DisplayMessageCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到显示消息命令后被调用。
 * 入口参数：pCmd     指向显示消息命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_DisplayMessageCB( zclCCDisplayMessage_t *pCmd,
                                  afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( (char*)pCmd->msgString.pStr, HAL_LCD_LINE_6 );
#endif  
}


/*********************************************************************
 * 函数名称：esp_CancelMessageCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到取消消息命令后被调用。
 * 入口参数：pCmd     指向取消消息命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_CancelMessageCB( zclCCCancelMessage_t *pCmd,
                                 afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_GetLastMessageCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到获取最后消息命令后被调用。
 * 入口参数：srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_GetLastMessageCB( afAddrType_t *srcAddr, uint8 seqNum )
{
  /* 在获取最后消息命令后，设备应当回发显示消息命令给发送消息者 */  

#if defined ( ZCL_MESSAGE )
  zclCCDisplayMessage_t cmd;
  uint8 msg[10] = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29 };

  /* 为最后消息使用以下信息来填充命令 */
  cmd.messageId = 0xaabbccdd;
  cmd.messageCtrl.transmissionMode = 0;
  cmd.messageCtrl.importance = 1;
  cmd.messageCtrl.confirmationRequired = 1;
  cmd.durationInMinutes = 60;

  cmd.msgString.strLen = 10;
  cmd.msgString.pStr = msg;

  zclSE_Message_Send_DisplayMessage( ESP_ENDPOINT, srcAddr, &cmd,
                                     false, seqNum );
#endif
}


/*********************************************************************
 * 函数名称：esp_MessageConfirmationCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到确认消息命令后被调用。
 * 入口参数：pCmd     指向确认消息命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_MessageConfirmationCB( zclCCMessageConfirmation_t *pCmd,
                                  afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


#if defined (ZCL_LOAD_CONTROL)
/*********************************************************************
 * 函数名称：esp_SendReportEventStatus
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
static void esp_SendReportEventStatus( afAddrType_t *srcAddr, uint8 seqNum,
                                       uint32 eventID, uint32 startTime,
                                       uint8 eventStatus, uint8 criticalityLevel,
                                       uint8 eventControl )
{
  zclCCReportEventStatus_t *pRsp;

  pRsp = (zclCCReportEventStatus_t *)osal_mem_alloc( sizeof( zclCCReportEventStatus_t ) );

  if ( pRsp != NULL)
  {
    /* 强制性字段 - 使用输入数据 */
    pRsp->issuerEventID = eventID;
    pRsp->eventStartTime = startTime;
    pRsp->criticalityLevelApplied = criticalityLevel;
    pRsp->eventControl = eventControl;
    pRsp->eventStatus = eventStatus;
    pRsp->signatureType = SE_PROFILE_SIGNATURE_TYPE_ECDSA;

    /* esp_Signature 为静态数组，可在esp_data.c文件中更改 */
    osal_memcpy( pRsp->signature, espSignature, 16 );

    /* 可选性字段 - 默认使用未使用值来填充 */
    pRsp->coolingTemperatureSetPointApplied = SE_OPTIONAL_FIELD_TEMPERATURE_SET_POINT;
    pRsp->heatingTemperatureSetPointApplied = SE_OPTIONAL_FIELD_TEMPERATURE_SET_POINT;
    pRsp->averageLoadAdjustment = SE_OPTIONAL_FIELD_INT8;
    pRsp->dutyCycleApplied = SE_OPTIONAL_FIELD_UINT8;

    /* 
       发送应答响应
       DisableDefaultResponse 标志位被设置为假 - 我们建议打开默认响应，
       因为报告事件状态的命令没有作出响应
     */
    zclSE_LoadControl_Send_ReportEventStatus( ESP_ENDPOINT, srcAddr,
                                            pRsp, false, seqNum );
    osal_mem_free( pRsp );
  }
}
#endif  


/*********************************************************************
 * 函数名称：esp_LoadControlEventCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个负载控制事件命令后被调用。
 * 入口参数：pCmd             指向负载控制事件命令
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 *           Status           事件状态
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_LoadControlEventCB( zclCCLoadControlEvent_t *pCmd,
                                    afAddrType_t *srcAddr, uint8 status,
                                    uint8 seqNum )
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
  esp_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEvent,
                             pCmd->startTime, eventStatus,
                             pCmd->criticalityLevel, pCmd->eventControl);

  if ( status != ZCL_STATUS_INVALID_FIELD )
  {
    // 在此添加用户负载控制事件处理代码
  }
#endif  
}


/*********************************************************************
 * 函数名称：esp_CancelLoadControlEventCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个取消负载控制事件命令后被调用。
 * 入口参数：pCmd             指向取消负载控制事件命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_CancelLoadControlEventCB( zclCCCancelLoadControlEvent_t *pCmd,
                                          afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( ZCL_LOAD_CONTROL )
  if ( 0 )  // 如果要取消该事件，则使用事件存在标志代替 "0"
  {
    /* 用户可在此添加取消事件代码 */ 

    // 以下是列子代码，用来回发响应
    /*
    esp_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEventID,
                               // startTime
                               EVENT_STATUS_LOAD_CONTROL_EVENT_CANCELLED, // eventStatus
                               // Criticality level
                               // eventControl };
    */

  }
  else
  {
	/* 如果事件不存在，应答状态：驳回请求 */ 
    esp_SendReportEventStatus( srcAddr, seqNum, pCmd->issuerEventID,
                               SE_OPTIONAL_FIELD_UINT32,                  // 启动时间
                               EVENT_STATUS_LOAD_CONTROL_EVENT_RECEIVED,  // 事件状态
                               SE_OPTIONAL_FIELD_UINT8,                   // 临界状态等级
                               SE_OPTIONAL_FIELD_UINT8 );                 // 事件控制
  }
#endif  
}


/*********************************************************************
 * 函数名称：esp_CancelAllLoadControlEventsCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个取消所有负载控制事件命令后被调用。
 * 入口参数：pCmd             指向取消所有负载控制事件命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_CancelAllLoadControlEventsCB( zclCCCancelAllLoadControlEvents_t *pCmd,
                                                 afAddrType_t *srcAddr, uint8 seqNum )
{
  /* 
    在接收到取消所有负载控制事件命令后，接收到命令的设备应查询所有
    事件列表，并且给每个事件分别发送响应 
   */
}


/*********************************************************************
 * 函数名称：esp_ReportEventStatusCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个报告事件状态事件命令后被调用。
 * 入口参数：pCmd             指向报告事件状态命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ReportEventStatusCB( zclCCReportEventStatus_t *pCmd,
                                     afAddrType_t *srcAddr, uint8 seqNum)
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：esp_GetScheduledEventCB
 * 功    能：ZCL SE 配置文件负载控制簇库回调函数。当接收到一个应用程序
 *           接收到一个获取预置事件命令后被调用。
 * 入口参数：pCmd             指向获取预置事件命令结构指针
 *           srcAddr          指向源地址指针
 *           seqNum           该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_GetScheduledEventCB( zclCCGetScheduledEvent_t *pCmd,
                                     afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/* 
  处理ZDO输入消息函数
*/
/*********************************************************************
 * 函数名称：esp_ProcessZDOMsg
 * 功    能：处理ZDO输入消息函数
 * 入口参数：inMsg           待处理的消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ProcessZDOMsg( zdoIncomingMsg_t *inMsg )
{
  switch ( inMsg->clusterID )
  {
    case Match_Desc_rsp:
    {
      ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        
      if ( pRsp )
      {
        matchRspCount++; // 查到到匹配，增加响应计数器
        if ( pRsp->status == ZSuccess && pRsp->cnt )
        {
          if ( matchRspCount == 1 )
          {
            simpleDescReqAddr[0].addrMode = (afAddrMode_t)Addr16Bit;
            simpleDescReqAddr[0].addr.shortAddr = pRsp->nwkAddr;

            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON ); //点亮LED4

            /* 设置简单描述符查询事件 */
            osal_set_event( espTaskID, SIMPLE_DESC_QUERY_EVT_1 );
          }
          else if ( matchRspCount == 2 )
          {
            matchRspCount = 0; // 复位响应计数器
            simpleDescReqAddr[1].addrMode = (afAddrMode_t)Addr16Bit;
            simpleDescReqAddr[1].addr.shortAddr = pRsp->nwkAddr;

            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON ); //点亮LED4

            /* 设置简单描述符查询事件 */
            osal_set_event( espTaskID, SIMPLE_DESC_QUERY_EVT_2 );
          }
        }

        osal_mem_free( pRsp );
      }
    }
    break;

    case Simple_Desc_rsp:
    {
      ZDO_SimpleDescRsp_t *pSimpleDescRsp;   // 指向接收简单描述符响应
      pSimpleDescRsp = (ZDO_SimpleDescRsp_t *)osal_mem_alloc( sizeof( ZDO_SimpleDescRsp_t ) );

      if(pSimpleDescRsp)
      {
        ZDO_ParseSimpleDescRsp( inMsg, pSimpleDescRsp );
		
	/* PCT设备 */
        if( pSimpleDescRsp->simpleDesc.AppDeviceId == ZCL_SE_DEVICEID_PCT )
        {
          pctAddr.addr.shortAddr = pSimpleDescRsp->nwkAddr;
        }
	/* 负载控制设备 */
        else if ( pSimpleDescRsp->simpleDesc.AppDeviceId == ZCL_SE_DEVICEID_LOAD_CTRL_EXTENSION )
        {
          loadControlAddr.addr.shortAddr = pSimpleDescRsp->nwkAddr;
        }
        osal_mem_free( pSimpleDescRsp );
      }
    }
    break;
  }
}


/* 
  处理ZCL基础输入命令/响应消息
 */
/*********************************************************************
 * 函数名称：esp_ProcessZCLMsg
 * 功    能：处理ZCL基础输入消息函数
 * 入口参数：inMsg           待处理的消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void esp_ProcessZCLMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#if defined ( ZCL_READ )
    case ZCL_CMD_READ_RSP:
      esp_ProcessInReadRspCmd( pInMsg );
      break;
#endif 
	  
#if defined ( ZCL_WRITE )
    case ZCL_CMD_WRITE_RSP:
      esp_ProcessInWriteRspCmd( pInMsg );
      break;
#endif

#if defined ( ZCL_REPORT )
    case ZCL_CMD_CONFIG_REPORT:
      esp_ProcessInConfigReportCmd( pInMsg );
      break;

    case ZCL_CMD_CONFIG_REPORT_RSP:
      esp_ProcessInConfigReportRspCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG:
      esp_ProcessInReadReportCfgCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG_RSP:
      esp_ProcessInReadReportCfgRspCmd( pInMsg );
      break;

    case ZCL_CMD_REPORT:
      esp_ProcessInReportCmd( pInMsg );
      break;
#endif

    case ZCL_CMD_DEFAULT_RSP:
      esp_ProcessInDefaultRspCmd( pInMsg );
      break;
#if defined ( ZCL_DISCOVER )
    case ZCL_CMD_DISCOVER_RSP:
      esp_ProcessInDiscRspCmd( pInMsg );
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
 * 函数名称：esp_ProcessInReadRspCmd
 * 功    能：处理配置文件读响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 esp_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：esp_ProcessInWriteRspCmd
 * 功    能：处理配置文件写响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 esp_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
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


#if defined ( ZCL_REPORT )
/*********************************************************************
 * 函数名称：esp_ProcessInConfigReportCmd
 * 功    能：处理配置文件配置报告命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TURE            在属性列表中找到属性
 *           FALSE           未找到
 ********************************************************************/
static uint8 esp_ProcessInConfigReportCmd( zclIncomingMsg_t *pInMsg )
{
  zclCfgReportCmd_t *cfgReportCmd;
  zclCfgReportRec_t *reportRec;
  zclCfgReportRspCmd_t *cfgReportRspCmd;
  zclAttrRec_t attrRec;
  uint8 status;
  uint8 i, j = 0;

  cfgReportCmd = (zclCfgReportCmd_t *)pInMsg->attrCmd;

  /* 为响应命令分配空间 */
  cfgReportRspCmd = (zclCfgReportRspCmd_t *)osal_mem_alloc( 
	  sizeof ( zclCfgReportRspCmd_t ) + sizeof ( zclCfgReportStatus_t) 
	                                         * cfgReportCmd->numAttr );
  if ( cfgReportRspCmd == NULL )
    return FALSE;

  /* 处理每个报告配置记录属性 */
  for ( i = 0; i < cfgReportCmd->numAttr; i++ )
  {
    reportRec = &(cfgReportCmd->attrList[i]);

    status = ZCL_STATUS_SUCCESS;

    if ( zclFindAttrRec( ESP_ENDPOINT, pInMsg->clusterId, reportRec->attrID, &attrRec ) )
    {
      if ( reportRec->direction == ZCL_SEND_ATTR_REPORTS )
      {
        if ( reportRec->dataType == attrRec.attr.dataType )
        {
          /* 将被报告的属性 */ 
          if ( zcl_MandatoryReportableAttribute( &attrRec ) == TRUE )
          {
            if ( reportRec->minReportInt < ESP_MIN_REPORTING_INTERVAL ||
                 ( reportRec->maxReportInt != 0 &&
                   reportRec->maxReportInt < reportRec->minReportInt ) )
            {
              /* 无效字段 */
              status = ZCL_STATUS_INVALID_VALUE;
            }
            else
            {
              /* 
	        设置最小和最大报告间隔并且改变报告状态为 
		zclSetAttrReportInterval( pAttr, cfgReportCmd )
	       */
              status = ZCL_STATUS_UNREPORTABLE_ATTRIBUTE;  
            }
          }
          else
          {
            /* 属性不能被报告 */
            status = ZCL_STATUS_UNREPORTABLE_ATTRIBUTE;
          }
        }
        else
        {
          /* 属性数据类型不正确 */
          status = ZCL_STATUS_INVALID_DATA_TYPE;
        }
      }
      else
      {
	/* 预先报告属性值 */
        if ( zcl_MandatoryReportableAttribute( &attrRec ) == TRUE )
        {
          /* 设置超时周期 */
          /* status = zclSetAttrTimeoutPeriod( pAttr, cfgReportCmd ); */
          status = ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
        }
        else
        {
          /* 属性报告不能被接收 */
          status = ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
        }
      }
    }
    else
    {
      /* 属性不支持 */
      status = ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
    }

    /* 如果不成功将记录状态 */
    if ( status != ZCL_STATUS_SUCCESS )
    {
      cfgReportRspCmd->attrList[j].status = status;
      cfgReportRspCmd->attrList[j++].attrID = reportRec->attrID;
    }
  }

  if ( j == 0 )
  {
    /* 
      一旦所有属性被成功配置，包括在响应命令中的状态字段的单个属性状态记录
      将被设置成SUCCESS并且该属性ID字段被省略
     */
    cfgReportRspCmd->attrList[0].status = ZCL_STATUS_SUCCESS;
    cfgReportRspCmd->numAttr = 1;
  }
  else
  {
    cfgReportRspCmd->numAttr = j;
  }

  /* 回发响应 */
  zcl_SendConfigReportRspCmd( ESP_ENDPOINT, &(pInMsg->srcAddr),
                              pInMsg->clusterId, cfgReportRspCmd, 
                              ZCL_FRAME_SERVER_CLIENT_DIR,
                              true, pInMsg->zclHdr.transSeqNum );
  
  osal_mem_free( cfgReportRspCmd );

  return TRUE ;
}


/*********************************************************************
 * 函数名称：esp_ProcessInConfigReportRspCmd
 * 功    能：处理配置文件配置报告响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TURE            成功
 *           FALSE           未成功
 ********************************************************************/
static uint8 esp_ProcessInConfigReportRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclCfgReportRspCmd_t *cfgReportRspCmd;
  zclAttrRec_t attrRec;
  uint8 i;

  cfgReportRspCmd = (zclCfgReportRspCmd_t *)pInMsg->attrCmd;

  for (i = 0; i < cfgReportRspCmd->numAttr; i++)
  {
    if ( zclFindAttrRec( ESP_ENDPOINT, pInMsg->clusterId,
                         cfgReportRspCmd->attrList[i].attrID, &attrRec ) )
    {
      // 在此添加代码
    }
  }

  return TRUE;
}


/*********************************************************************
 * 函数名称：esp_ProcessInReadReportCfgCmd
 * 功    能：处理配置文件读取报告配置命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 esp_ProcessInReadReportCfgCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadReportCfgCmd_t *readReportCfgCmd;
  zclReadReportCfgRspCmd_t *readReportCfgRspCmd;
  zclReportCfgRspRec_t *reportRspRec;
  zclAttrRec_t attrRec;
  uint8 reportChangeLen;
  uint8 *dataPtr;
  uint8 hdrLen;
  uint8 dataLen = 0;
  uint8 status;
  uint8 i;

  readReportCfgCmd = (zclReadReportCfgCmd_t *)pInMsg->attrCmd;

  /* 得到响应长度(改变可报告字段为有效长度) */
  for ( i = 0; i < readReportCfgCmd->numAttr; i++ )
  {
    /* 由于支持属性为模拟数据类型，得到可报告字段长度 */
    if ( zclFindAttrRec( ESP_ENDPOINT, pInMsg->clusterId,
                         readReportCfgCmd->attrList[i].attrID, &attrRec ) )
    {
      if ( zclAnalogDataType( attrRec.attr.dataType ) )
      {
         reportChangeLen = zclGetDataTypeLength( attrRec.attr.dataType );

         /* 如果需要添加数据填充 */
         if ( PADDING_NEEDED( reportChangeLen ) )
           reportChangeLen++;

         dataLen += reportChangeLen;
      }
    }
  }

  hdrLen = sizeof( zclReadReportCfgRspCmd_t ) + 
                  ( readReportCfgCmd->numAttr * sizeof( zclReportCfgRspRec_t ) );

  /* 为响应命令分配空间 */
  readReportCfgRspCmd = (zclReadReportCfgRspCmd_t *)osal_mem_alloc( hdrLen + dataLen );

  if ( readReportCfgRspCmd == NULL )
    return FALSE; 

  dataPtr = (uint8 *)( (uint8 *)readReportCfgRspCmd + hdrLen );
  readReportCfgRspCmd->numAttr = readReportCfgCmd->numAttr;
  for (i = 0; i < readReportCfgCmd->numAttr; i++)
  {
    reportRspRec = &(readReportCfgRspCmd->attrList[i]);

    if ( zclFindAttrRec( ESP_ENDPOINT, pInMsg->clusterId,
                         readReportCfgCmd->attrList[i].attrID, &attrRec ) )
    {
      if ( zcl_MandatoryReportableAttribute( &attrRec ) == TRUE )
      {
        /* 获取报告配置 */
        /* status = zclReadReportCfg( readReportCfgCmd->attrID[i], reportRspRec ); */
        status = ZCL_STATUS_UNREPORTABLE_ATTRIBUTE; 
        if ( status == ZCL_STATUS_SUCCESS && zclAnalogDataType( attrRec.attr.dataType ) )
        {
          reportChangeLen = zclGetDataTypeLength( attrRec.attr.dataType );
          reportRspRec->reportableChange = dataPtr;

          /* 如果需要添加数据填充 */
          if ( PADDING_NEEDED( reportChangeLen ) )
            reportChangeLen++;
          dataPtr += reportChangeLen;
        }
      }
      else
      {
        /* 在强制可报告属性列表中没有属性 */
        status = ZCL_STATUS_UNREPORTABLE_ATTRIBUTE;
      }
    }
    else
    {
      /* 属性未找到 */
      status = ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
    }

    reportRspRec->status = status;
    reportRspRec->attrID = readReportCfgCmd->attrList[i].attrID;
  }

  /* 回发响应 */
  zcl_SendReadReportCfgRspCmd( ESP_ENDPOINT, &(pInMsg->srcAddr),
                               pInMsg->clusterId, readReportCfgRspCmd, 
                               ZCL_FRAME_SERVER_CLIENT_DIR,
                               true, pInMsg->zclHdr.transSeqNum );
  
  osal_mem_free( readReportCfgRspCmd );

  return TRUE;
}


/*********************************************************************
 * 函数名称：esp_ProcessInReadReportCfgRspCmd
 * 功    能：处理配置文件读取报告配置响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 esp_ProcessInReadReportCfgRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadReportCfgRspCmd_t *readReportCfgRspCmd;
  zclReportCfgRspRec_t *reportRspRec;
  uint8 i;

  readReportCfgRspCmd = (zclReadReportCfgRspCmd_t *)pInMsg->attrCmd;

  for ( i = 0; i < readReportCfgRspCmd->numAttr; i++ )
  {
    reportRspRec = &(readReportCfgRspCmd->attrList[i]);

    /* 通知原始读报告配置命令设备 */
    if ( reportRspRec->status == ZCL_STATUS_SUCCESS )
    {
      if ( reportRspRec->direction == ZCL_SEND_ATTR_REPORTS )
      {
        // 用户在此添加代码
      }
      else
      {
        // 用户在此添加代码
      }
    }
  }

  return TRUE;
}


/*********************************************************************
 * 函数名称：esp_ProcessInReportCmd
 * 功    能：处理配置文件报告命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 esp_ProcessInReportCmd( zclIncomingMsg_t *pInMsg )
{
  zclReportCmd_t *reportCmd;
  zclReport_t *reportRec;
  uint8 i;
  uint8 *meterData;
  char lcdBuf[13];

  reportCmd = (zclReportCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < reportCmd->numAttr; i++)
  {
    /* 设备被另外具有属性最新值的设备所通知 */
    reportRec = &(reportCmd->attrList[i]);

    if ( reportRec->attrID == ATTRID_SE_CURRENT_SUMMATION_DELIVERED )
    {
      /* 处理当前分发总和属性简单计量 */
      meterData = reportRec->attrData;

      /* 十六进制转ASCII转换处理 */
      for(i=0; i<6; i++)
      {
        if(meterData[5-i] == 0)
        {
          lcdBuf[i*2] = '0';
          lcdBuf[i*2+1] = '0';
        }
        else if(meterData[5-i] <= 0x0A)
        {
          lcdBuf[i*2] = '0';
          _ltoa(meterData[5-i],(uint8*)&lcdBuf[i*2+1],16);
        }
        else
        {
          _ltoa(meterData[5-i],(uint8*)&lcdBuf[i*2],16);
        }
      }

      /* 以16进制方式打印出当前分发总和 */
      HalLcdWriteString("Zigbee Coord esp", HAL_LCD_LINE_4 );
      HalLcdWriteString("Curr Summ Dlvd", HAL_LCD_LINE_5 );
      HalLcdWriteString(lcdBuf, HAL_LCD_LINE_6 );
    }
  }
  return TRUE;
}
#endif


/*********************************************************************
 * 函数名称：esp_ProcessInDefaultRspCmd
 * 功    能：处理配置文件默认响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 esp_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  return TRUE;
}


#if defined ( ZCL_DISCOVER )
/*********************************************************************
 * 函数名称：esp_ProcessInDiscRspCmd
 * 功    能：处理配置文件发现响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 esp_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg )
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
