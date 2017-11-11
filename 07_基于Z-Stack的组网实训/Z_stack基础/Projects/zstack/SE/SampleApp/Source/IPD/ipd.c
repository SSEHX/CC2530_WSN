/*******************************************************************************
 * 文件名称：ipd.c
 * 功    能：SE智能能源实验。
 * 实验内容：ESP(能源服务门户)建立一个ZigBeePRO网路。其他设备控制器，比如IPD(
 *           预装显示设备)、PCT(可编程通信温控器)等发现该网络，并且加入到该网络中。
 *           ESP(能源服务门户)将通过无线方式管理其他设备。
 * 实验设备：SK-SmartRF05EB（装配有SK-CC2530EM）
 *           RIGHT键：            加入网络
 *
 *           LED1(绿色)：      设备成功加入组1被点亮，退出组1后熄灭
 *           LED3(黄色)：      当成功建立网络或加入网络后此LED点亮
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "ZDApp.h"
#include "AddrMgr.h"

#include "se.h"
#include "ipd.h"
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
static uint8 ipdTaskID;            // ipd操作系统任务ID
static uint8 ipdTransID;           // 传输id
static devStates_t ipdNwkState;    // 网络状态变量
static afAddrType_t ESPAddr;       // ESP目的地址
static uint8 linkKeyStatus;        // 从获取链接密钥函数返回的状态变量
static uint8 option;               // 发送选项字段
/********************************************************************/

/* 本地函数 */
/********************************************************************/
static void ipd_HandleKeys( uint8 shift, uint8 keys );

#if defined (ZCL_KEY_ESTABLISH)
static uint8 ipd_KeyEstablish_ReturnLinkKey( uint16 shortAddr );
#endif 

#if defined ( ZCL_ALARMS )
static void ipd_ProcessAlarmCmd( uint8 srcEP, afAddrType_t *dstAddr,
       uint16 clusterID, zclFrameHdr_t *hdr, uint8 len, uint8 *data );
#endif 

static void ipd_ProcessIdentifyTimeChange( void );
/********************************************************************/

/* 本地函数 - 应用回调函数 */
/********************************************************************/

/*
   基本回调函数
*/
static uint8 ipd_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo );

/*
   通用簇回调函数
*/
static void ipd_BasicResetCB( void );
static void ipd_IdentifyCB( zclIdentify_t *pCmd );
static void ipd_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp );
static void ipd_AlarmCB( zclAlarm_t *pAlarm );

/*
   SE回调函数
*/
static void ipd_GetCurrentPriceCB( zclCCGetCurrentPrice_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void ipd_GetScheduledPriceCB( zclCCGetScheduledPrice_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void ipd_PublishPriceCB( zclCCPublishPrice_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void ipd_DisplayMessageCB( zclCCDisplayMessage_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void ipd_CancelMessageCB( zclCCCancelMessage_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );
static void ipd_GetLastMessageCB( afAddrType_t *srcAddr, uint8 seqNum );
static void ipd_MessageConfirmationCB( zclCCMessageConfirmation_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum );


/* 处理ZCL基本函数 - 输入命令/响应消息 */
/********************************************************************/
static void ipd_ProcessZCLMsg( zclIncomingMsg_t *msg );

#if defined ( ZCL_READ )
static uint8 ipd_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif

#if defined ( ZCL_WRITE )
static uint8 ipd_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif 

static uint8 ipd_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );

#if defined ( ZCL_DISCOVER )
static uint8 ipd_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg );
#endif 
/********************************************************************/


/*
   ZCL通用簇回调表
*/
static zclGeneral_AppCallbacks_t ipd_GenCmdCallbacks =
{
  ipd_BasicResetCB,              // 基本簇复位命令
  ipd_IdentifyCB,                // 识别命令
  ipd_IdentifyQueryRspCB,        // 识别查询响应命令
  NULL,                          // 开关簇命令
  NULL,                          // 级别控制离开命令
  NULL,                          // 级别控制移动命令
  NULL,                          // 级别控制单步命令
  NULL,                          // 级别控制停止命令
  NULL,                          // 组响应命令
  NULL,                          // 场景保存请求命令
  NULL,                          // 场景重调用请求命令
  NULL,                          // 场景响应命令
  ipd_AlarmCB,                   // 报警(响应)命令
  NULL,                          // RSSI定位命令
  NULL,                          // RSSI定位响应命令
};

/*
   ZCL SE簇回调表
*/
static zclSE_AppCallbacks_t ipd_SECmdCallbacks =			
{
  NULL,                             // 获取 Profile 命令
  NULL,                             // 获取 Profile 响应
  NULL,                             // 请求镜像命令
  NULL,                             // 请求镜像响应
  NULL,                             // 镜像移除命令
  NULL,                             // 镜像移除响应
  ipd_GetCurrentPriceCB,            // 获取当前价格
  ipd_GetScheduledPriceCB,          // 获取预定价格
  ipd_PublishPriceCB,               // 公布价格
  ipd_DisplayMessageCB,             // 显示消息命令
  ipd_CancelMessageCB,              // 取消消息命令
  ipd_GetLastMessageCB,             // 获取最后一个消息命令
  ipd_MessageConfirmationCB,        // 消息确认
  NULL,                             // 负载控制事件
  NULL,                             // 退出负载控制事件
  NULL,                             // 退出所有负载控制事件
  NULL,                             // 报告事件状态
  NULL,                             // 获取预定事件
};



/*********************************************************************
 * 函数名称：ipd_Init
 * 功    能：IPD的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void ipd_Init( uint8 task_id )
{
  ipdTaskID = task_id;
  ipdNwkState = DEV_INIT;
  ipdTransID = 0;

  /* 
     可以在此处添加对设备硬件的初始化代码。 
     若硬件是本应用专用的，可以将对该硬件的初始化代码添加在此处。 
     若硬件不是本应用专用的，可以将初始化代码添加在Zmain.c文件中的main()函数。
  */

  // 设置ESP目的地址
  ESPAddr.addrMode = (afAddrMode_t)Addr16Bit;
  ESPAddr.endPoint = IPD_ENDPOINT;
  ESPAddr.addr.shortAddr = 0;

  /* 注册一个SE端点 */
  zclSE_Init( &ipdSimpleDesc );

  /* 注册ZCL通用簇库回调函数 */
  zclGeneral_RegisterCmdCallbacks( IPD_ENDPOINT, &ipd_GenCmdCallbacks );

  /* 注册ZCL SE簇库回调函数 */
  zclSE_RegisterCmdCallbacks( IPD_ENDPOINT, &ipd_SECmdCallbacks );

  /* 注册应用程序属性列表 */
  zcl_registerAttrList( IPD_ENDPOINT, IPD_MAX_ATTRIBUTES, ipdAttrs );

  /* 注册应用程序簇选项列表 */
  zcl_registerClusterOptionList( IPD_ENDPOINT, IPD_MAX_OPTIONS, ipdOptions );

  /* 注册应用程序属性数据验证回调函数 */
  zcl_registerValidateAttrData( ipd_ValidateAttrDataCB );

  /* 注册应用程序来接收未处理基础命令/响应消息 */
  zcl_registerForMsg( ipdTaskID );

  /* 注册所有按键事件 - 将处理所有按键事件 */
  RegisterForKeys( ipdTaskID );

  /* 启动定时器使用操作系统时钟来同步IPD定时器 */
  osal_start_timerEx( ipdTaskID, IPD_UPDATE_TIME_EVT, IPD_UPDATE_TIME_PERIOD );
}


/*********************************************************************
 * 函数名称：ipd_event_loop
 * 功    能：IPD的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
uint16 ipd_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( ipdTaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          /* 输入ZCL基础命令/响应消息 */
          ipd_ProcessZCLMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          /* 按键处理 */
          ipd_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          ipdNwkState = (devStates_t)(MSGpkt->hdr.status);

          if (ZG_SECURE_ENABLED)
          {
            if ( ipdNwkState == DEV_END_DEVICE )
            {
              /* 检查链接密钥是否已经被建立 */
              linkKeyStatus = ipd_KeyEstablish_ReturnLinkKey(ESPAddr.addr.shortAddr);

              if (linkKeyStatus != ZSuccess)
              {
                /* 发送密钥建立请求 */
                osal_set_event( ipdTaskID, IPD_KEY_ESTABLISHMENT_REQUEST_EVT);
              }
              else
              {
                /* 链接密钥已经建立 结束发送报告 */
                osal_start_timerEx( ipdTaskID, IPD_GET_PRICING_INFO_EVT, IPD_GET_PRICING_INFO_PERIOD );
              }
            }
          }
          else
          {
            osal_start_timerEx( ipdTaskID, IPD_GET_PRICING_INFO_EVT, IPD_GET_PRICING_INFO_PERIOD );
          }

          /* 设置SE终端设备查询速率 */
          NLME_SetPollRate ( SE_DEVICE_POLL_RATE ); 

          break;

#if defined( ZCL_KEY_ESTABLISH )
        case ZCL_KEY_ESTABLISH_IND:
          osal_start_timerEx( ipdTaskID, IPD_GET_PRICING_INFO_EVT, IPD_GET_PRICING_INFO_PERIOD );
          break;
#endif

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt ); // 释放存储器
    }

    return (events ^ SYS_EVENT_MSG); // 返回未处理的事件
  }

  /* 初始化密钥建立请求事件 */
  if ( events & IPD_KEY_ESTABLISHMENT_REQUEST_EVT )
  {
    zclGeneral_KeyEstablish_InitiateKeyEstablishment(ipdTaskID, &ESPAddr, ipdTransID);

    return ( events ^ IPD_KEY_ESTABLISHMENT_REQUEST_EVT );
  }

  /* 获取当前价格事件 */
  if ( events & IPD_GET_PRICING_INFO_EVT )
  {
    zclSE_Pricing_Send_GetCurrentPrice( IPD_ENDPOINT, &ESPAddr, option, TRUE, 0 );
    osal_start_timerEx( ipdTaskID, IPD_GET_PRICING_INFO_EVT, IPD_GET_PRICING_INFO_PERIOD );

    return ( events ^ IPD_GET_PRICING_INFO_EVT );
  }

  /* 被一个识别命令触发的识别超时事件处理 */
  if ( events & IPD_IDENTIFY_TIMEOUT_EVT )
  {
    if ( ipdIdentifyTime > 0 )
    {
      ipdIdentifyTime--;
    }
    ipd_ProcessIdentifyTimeChange();

    return ( events ^ IPD_IDENTIFY_TIMEOUT_EVT );
  }

  /* 获取当前时间事件 */
  if ( events & IPD_UPDATE_TIME_EVT )
  {
    ipdTime = osal_getClock();
    osal_start_timerEx( ipdTaskID, IPD_UPDATE_TIME_EVT, IPD_UPDATE_TIME_PERIOD );

    return ( events ^ IPD_UPDATE_TIME_EVT );
  }

  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：ipd_ProcessIdentifyTimeChange
 * 功    能：调用闪烁LED来指定识别时间属性值。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_ProcessIdentifyTimeChange( void )
{
  if ( ipdIdentifyTime > 0 )
  {
    osal_start_timerEx( ipdTaskID, IPD_IDENTIFY_TIMEOUT_EVT, 1000 );
    HalLedBlink ( HAL_LED_4, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
    osal_stop_timerEx( ipdTaskID, IPD_IDENTIFY_TIMEOUT_EVT );
  }
}


#if defined (ZCL_KEY_ESTABLISH)
/*********************************************************************
 * 函数名称：ipd_KeyEstablish_ReturnLinkKey
 * 功    能：获取被请求的密钥函数。
 * 入口参数：shortAddr 短地址
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 ipd_KeyEstablish_ReturnLinkKey( uint16 shortAddr )
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
 * 函数名称：ipd_HandleKeys
 * 功    能：IPD的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_2     SK-SmartRF10BB的K2键       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_HandleKeys( uint8 shift, uint8 keys )
{
  if ( keys & HAL_KEY_SW_2 )
  {
    ZDOInitDevice(0); // 加入网络
  }
}


/*********************************************************************
 * 函数名称：ipd_ValidateAttrDataCB
 * 功    能：检查为属性提供的数据值是否在该属性指定的范围内。
 * 入口参数：pAttr     指向属性的指针
 *           pAttrInfo 指向属性信息的指针            
 * 出口参数：无
 * 返 回 值：TRUE      有效数据
 *           FALSE     无效数据
 ********************************************************************/
static uint8 ipd_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo )
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
 * 函数名称：ipd_BasicResetCB
 * 功    能：ZCL通用簇库回调函数。设置所有簇的所有属性为出厂默认设置。
 * 入口参数：无          
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_BasicResetCB( void )
{
  // 在此添加代码
}


/*********************************************************************
 * 函数名称：ipd_IdentifyCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别命令后被调用。
 * 入口参数：pCmd      指向识别命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_IdentifyCB( zclIdentify_t *pCmd )
{
  ipdIdentifyTime = pCmd->identifyTime;
  ipd_ProcessIdentifyTimeChange();
}


/*********************************************************************
 * 函数名称：ipd_IdentifyQueryRspCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别查询命令
 *           后被调用。
 * 入口参数：pRsp    指向识别查询响应结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：ipd_AlarmCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序报警请求命令
 *           后被调用。
 * 入口参数：pAlarm   指向报警命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_AlarmCB( zclAlarm_t *pAlarm )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * @fn      ipd_GetCurrentPriceCB
 *
 * @brief   Callback from the ZCL SE Profile Pricing Cluster Library when
 *          it received a Get Current Price for
 *          this application.
 *
 * @param   pCmd - pointer to structure for Get Current Price command
 * @param   srcAddr - source address
 * @param   seqNum - sequence number for this command
 *
 * @return  none
 */
/*********************************************************************
 * 函数名称：ipd_GetCurrentPriceCB
 * 功    能：ZCL SE 配置文件定价簇库回调函数。当接收到一个应用程序
 *           接收到获取当前价格后被调用。
 * 入口参数：pCmd     指向获取当前价格命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_GetCurrentPriceCB( zclCCGetCurrentPrice_t *pCmd,
                                   afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( ZCL_PRICING )
  /* 在获取当前价格命令后，设备应当发送带有当前时间消息的公布价格命令 */  
  zclCCPublishPrice_t cmd;

  osal_memset( &cmd, 0, sizeof( zclCCPublishPrice_t ) );

  cmd.providerId = 0xbabeface;
  cmd.priceTier = 0xfe;

  zclSE_Pricing_Send_PublishPrice( IPD_ENDPOINT, srcAddr, &cmd, false, seqNum );
#endif 
}


/*********************************************************************
 * 函数名称：ipd_GetScheduledPriceCB
 * 功    能：ZCL SE 配置文件定价簇库回调函数。当接收到一个应用程序
 *           接收到获取预定价格后被调用。
 * 入口参数：pCmd     指向获取预定价格命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_GetScheduledPriceCB( zclCCGetScheduledPrice_t *pCmd, 
                                     afAddrType_t *srcAddr, uint8 seqNum )
{
  /* 在获取预定价格命令后，设备将为当前预定价格事件发送公布价格命令 */ 
  /* 例子代码如下 */

#if defined ( ZCL_PRICING )
  zclCCPublishPrice_t cmd;

  osal_memset( &cmd, 0, sizeof( zclCCPublishPrice_t ) );

  cmd.providerId = 0xbabeface;
  cmd.priceTier = 0xfe;

  zclSE_Pricing_Send_PublishPrice( IPD_ENDPOINT, srcAddr, &cmd, false, seqNum );

#endif  
}


/*********************************************************************
 * 函数名称：ipd_PublishPriceCB
 * 功    能：ZCL SE 配置文件定价簇库回调函数。当接收到一个应用程序
 *           接收公布价格后被调用。
 * 入口参数：pCmd     指向公布价格命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_PublishPriceCB( zclCCPublishPrice_t *pCmd,
                                afAddrType_t *srcAddr, uint8 seqNum )
{
  if ( pCmd )
  {
    /* 显示提供者设备ID字段 */ 
    HalLcdWriteString("Provider ID", HAL_LCD_LINE_5);
    HalLcdWriteValue(pCmd->providerId, 10, HAL_LCD_LINE_6);
  }
}


/*********************************************************************
 * 函数名称：ipd_DisplayMessageCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到显示消息命令后被调用。
 * 入口参数：pCmd     指向显示消息命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_DisplayMessageCB( zclCCDisplayMessage_t *pCmd,
                                  afAddrType_t *srcAddr, uint8 seqNum )
{
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( (char*)pCmd->msgString.pStr, HAL_LCD_LINE_6);
#endif  
}


/*********************************************************************
 * 函数名称：ipd_CancelMessageCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到取消消息命令后被调用。
 * 入口参数：pCmd     指向取消消息命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_CancelMessageCB( zclCCCancelMessage_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：ipd_GetLastMessageCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到获取最后消息命令后被调用。
 * 入口参数：srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_GetLastMessageCB( afAddrType_t *srcAddr, uint8 seqNum )
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

  zclSE_Message_Send_DisplayMessage( IPD_ENDPOINT, srcAddr, &cmd,
                                     false, seqNum );
#endif  
}


/*********************************************************************
 * 函数名称：ipd_MessageConfirmationCB
 * 功    能：ZCL SE 配置文件消息簇库回调函数。当接收到一个应用程序
 *           接收到确认消息命令后被调用。
 * 入口参数：pCmd     指向确认消息命令结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_MessageConfirmationCB( zclCCMessageConfirmation_t *pCmd,
                                       afAddrType_t *srcAddr, uint8 seqNum )
{
 // 在这里添加用户代码
}


/* 
  处理ZCL基础输入命令/响应消息
 */
/*********************************************************************
 * 函数名称：ipd_ProcessZCLMsg
 * 功    能：处理ZCL基础输入消息函数
 * 入口参数：inMsg           待处理的消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void ipd_ProcessZCLMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#if defined ( ZCL_READ )
    case ZCL_CMD_READ_RSP:
      ipd_ProcessInReadRspCmd( pInMsg );
      break;
#endif 

#if defined ( ZCL_WRITE )
    case ZCL_CMD_WRITE_RSP:
      ipd_ProcessInWriteRspCmd( pInMsg );
      break;
#endif 

    case ZCL_CMD_DEFAULT_RSP:
      ipd_ProcessInDefaultRspCmd( pInMsg );
      break;

#if defined ( ZCL_DISCOVER )
    case ZCL_CMD_DISCOVER_RSP:
      ipd_ProcessInDiscRspCmd( pInMsg );
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
 * 函数名称：ipd_ProcessInReadRspCmd
 * 功    能：处理配置文件读响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 ipd_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：ipd_ProcessInWriteRspCmd
 * 功    能：处理配置文件写响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 ipd_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：ipd_ProcessInDefaultRspCmd
 * 功    能：处理配置文件默认响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 ipd_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  return TRUE;
}


#if defined ( ZCL_DISCOVER )
/*********************************************************************
 * 函数名称：ipd_ProcessInDiscRspCmd
 * 功    能：处理配置文件发现响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 ipd_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg )
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
