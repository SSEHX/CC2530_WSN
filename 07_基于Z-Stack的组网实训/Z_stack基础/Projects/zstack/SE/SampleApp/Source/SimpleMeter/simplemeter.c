/*******************************************************************************
 * 文件名称：simplemeter.c
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
#include "simplemeter.h"
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

/* 常量 */
/********************************************************************/
/* 
  简单计量器最小报告间隔数 
 */
#define SIMPLEMETER_MIN_REPORTING_INTERVAL       5

/* 本地变量 */
/********************************************************************/
static uint8 simpleMeterTaskID;                       // esp操作系统任务ID
static uint8 simpleMeterTransID;                      // 传输id
static devStates_t simpleMeterNwkState;               // 网络状态变量
static afAddrType_t ESPAddr;                          // ESP目的地址
static zclReportCmd_t *pReportCmd;                    // 报告命令结构
static uint8 numAttr = 1;                             // 报告中的属性数
extern uint8 simpleMeterCurrentSummationDelivered[];  // 在simplemeter_data.c文件中被定义
static uint8 linkKeyStatus;                           // 从获取链接密钥函数返回的状态变量


/* 本地函数 */
/********************************************************************/
static void simplemeter_HandleKeys( uint8 shift, uint8 keys );

#if defined (ZCL_KEY_ESTABLISH)
static uint8 simplemeter_KeyEstablish_ReturnLinkKey( uint16 shortAddr );
#endif 

#if defined ( ZCL_ALARMS )
static void simplemeter_ProcessAlarmCmd( uint8 srcEP, afAddrType_t *dstAddr,
                        uint16 clusterID, zclFrameHdr_t *hdr, uint8 len, uint8 *data );
#endif 

static void simplemeter_ProcessIdentifyTimeChange( void );
/********************************************************************/


/* 本地函数 - 应用回调函数 */
/********************************************************************/
/*
   基本回调函数
*/
static uint8 simplemeter_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo );

/*
   通用簇回调函数
*/
static void simplemeter_BasicResetCB( void );
static void simplemeter_IdentifyCB( zclIdentify_t *pCmd );
static void simplemeter_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp );
static void simplemeter_AlarmCB( zclAlarm_t *pAlarm );

/*
   SE回调函数
*/
static void simplemeter_GetProfileCmdCB( zclCCGetProfileCmd_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum );
static void simplemeter_GetProfileRspCB( zclCCGetProfileRsp_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum );
static void simplemeter_ReqMirrorCmdCB( afAddrType_t *srcAddr, uint8 seqNum );
static void simplemeter_ReqMirrorRspCB( zclCCReqMirrorRsp_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum );
static void simplemeter_MirrorRemCmdCB( afAddrType_t *srcAddr, uint8 seqNum );
static void simplemeter_MirrorRemRspCB( zclCCMirrorRemRsp_t *pCmd,
                                        afAddrType_t *srcAddr, uint8 seqNum );

/* 处理ZCL基本函数 - 输入命令/响应消息 */
/********************************************************************/
static void simplemeter_ProcessZCLMsg( zclIncomingMsg_t *msg );

#if defined ( ZCL_READ )
static uint8 simplemeter_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif 

#if defined ( ZCL_WRITE )
static uint8 simplemeter_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif

#if defined ( ZCL_REPORT )
static uint8 simplemeter_ProcessInConfigReportCmd( zclIncomingMsg_t *pInMsg );
static uint8 simplemeter_ProcessInConfigReportRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 simplemeter_ProcessInReadReportCfgCmd( zclIncomingMsg_t *pInMsg );
static uint8 simplemeter_ProcessInReadReportCfgRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 simplemeter_ProcessInReportCmd( zclIncomingMsg_t *pInMsg );
#endif 

static uint8 simplemeter_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );

#if defined ( ZCL_DISCOVER )
static uint8 simplemeter_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg );
#endif 


/*
   ZCL通用簇回调表
*/
static zclGeneral_AppCallbacks_t simplemeter_GenCmdCallbacks =
{
  simplemeter_BasicResetCB,      // 基本的簇复位命令
  simplemeter_IdentifyCB,        // 识别命令
  simplemeter_IdentifyQueryRspCB,// 识别查询响应命令
  NULL,                          // 开关簇命令
  NULL,                          // 级别控制离开命令
  NULL,                          // 级别控制移动命令
  NULL,                          // 级别控制单步命令
  NULL,                          // 级别控制停止命令
  NULL,                          // 组响应命令
  NULL,                          // 场景保存请求命令
  NULL,                          // 场景重调用请求命令
  NULL,                          // 场景响应命令
  simplemeter_AlarmCB,           // 报警(响应)命令
  NULL,                          // RSSI定位命令
  NULL,                          // RSSI定位响应命令
};

/*
   ZCL SE簇回调表
*/
static zclSE_AppCallbacks_t simplemeter_SECmdCallbacks =			
{
  simplemeter_GetProfileCmdCB,   // 获取 Profile 命令
  simplemeter_GetProfileRspCB,   // 获取 Profile 响应
  simplemeter_ReqMirrorCmdCB,    // 请求镜像命令
  simplemeter_ReqMirrorRspCB,    // 请求镜像响应
  simplemeter_MirrorRemCmdCB,    // 镜像移除命令
  simplemeter_MirrorRemRspCB,    // 镜像移除响应
  NULL,                          // 获取当前价格
  NULL,                          // 获取预定价格
  NULL,                          // 公布价格
  NULL,                          // 显示消息命令
  NULL,                          // 取消消息命令
  NULL,                          // 获取最后一个消息命令
  NULL,                          // 消息确认
  NULL,                          // 负载控制事件
  NULL,                          // 退出负载控制事件
  NULL,                          // 退出所有负载控制事件
  NULL,                          // 报告事件状态
  NULL,                          // 获取预定事件
};


/*********************************************************************
 * 函数名称：rangeext_Init
 * 功    能：增距器的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void simplemeter_Init( uint8 task_id )
{
  simpleMeterTaskID = task_id;
  simpleMeterNwkState = DEV_INIT;
  simpleMeterTransID = 0;

  // 设置ESP目的地址
  ESPAddr.addrMode = (afAddrMode_t)Addr16Bit;
  ESPAddr.endPoint = SIMPLEMETER_ENDPOINT;
  ESPAddr.addr.shortAddr = 0;

  /* 注册一个SE端点 */
  zclSE_Init( &simpleMeterSimpleDesc );

  /* 注册ZCL通用簇库回调函数 */
  zclGeneral_RegisterCmdCallbacks( SIMPLEMETER_ENDPOINT, &simplemeter_GenCmdCallbacks );

  // Register the ZCL SE Cluster Library callback functions
  zclSE_RegisterCmdCallbacks( SIMPLEMETER_ENDPOINT, &simplemeter_SECmdCallbacks );
  /* 注册应用程序属性列表 */
  zcl_registerAttrList( SIMPLEMETER_ENDPOINT, SIMPLEMETER_MAX_ATTRIBUTES, simpleMeterAttrs );

  /* 注册应用程序簇选项列表 */
  zcl_registerClusterOptionList( SIMPLEMETER_ENDPOINT, SIMPLEMETER_MAX_OPTIONS, simpleMeterOptions );

  /* 注册应用程序属性数据验证回调函数 */
  zcl_registerValidateAttrData( simplemeter_ValidateAttrDataCB );

  /* 注册应用程序来接收未处理基础命令/响应消息 */
  zcl_registerForMsg( simpleMeterTaskID );

  /* 注册所有按键事件 - 将处理所有按键事件 */
  RegisterForKeys( simpleMeterTaskID );

  /* 启动定时器来同步简单计量器和操作系统时钟 */
  osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_UPDATE_TIME_EVT, 
                      SIMPLEMETER_UPDATE_TIME_PERIOD );

  /* 为简单计量器设置属性ID */
  pReportCmd = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) + 
                                      ( numAttr * sizeof( zclReport_t ) ) );
  if ( pReportCmd != NULL )
  {
    pReportCmd->numAttr = numAttr;

    /* 设立最初的属性 */
    pReportCmd->attrList[0].attrID = ATTRID_SE_CURRENT_SUMMATION_DELIVERED;
    pReportCmd->attrList[0].dataType = ZCL_DATATYPE_UINT48;
    pReportCmd->attrList[0].attrData = simpleMeterCurrentSummationDelivered;

    /* 设立剩余属性 */
  }
}


/*********************************************************************
 * 函数名称：simplemeter_event_loop
 * 功    能：简单计量器的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
uint16 simplemeter_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( simpleMeterTaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          /* 输入ZCL基础命令/响应消息 */
          simplemeter_ProcessZCLMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          /* 按键处理 */
          simplemeter_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          simpleMeterNwkState = (devStates_t)(MSGpkt->hdr.status);

          if (ZG_SECURE_ENABLED)
          {
            if ( (simpleMeterNwkState == DEV_ROUTER) || (simpleMeterNwkState == DEV_END_DEVICE) )
            {
              /* 检查链接密钥是否已经被建立 */
              linkKeyStatus = simplemeter_KeyEstablish_ReturnLinkKey(ESPAddr.addr.shortAddr);

              if (linkKeyStatus != ZSuccess)
              {
                /* 发送密钥建立请求 */
                osal_set_event( simpleMeterTaskID, SIMPLEMETER_KEY_ESTABLISHMENT_REQUEST_EVT);

              }
              else
              {
                /* 链接密钥已经建立，结束发送报告 */
                osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_REPORT_ATTRIBUTE_EVT, 
                                    SIMPLEMETER_REPORT_PERIOD );
              }
            }
          }
          else
          {
            osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_REPORT_ATTRIBUTE_EVT, 
                                SIMPLEMETER_REPORT_PERIOD );
          }

          /*设置SE终端设备查询速率*/
          NLME_SetPollRate ( SE_DEVICE_POLL_RATE ); 
          break;

#if defined( ZCL_KEY_ESTABLISH )
        case ZCL_KEY_ESTABLISH_IND:
          osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_REPORT_ATTRIBUTE_EVT, 
                              SIMPLEMETER_REPORT_PERIOD );
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
  if ( events & SIMPLEMETER_KEY_ESTABLISHMENT_REQUEST_EVT )
  {
    zclGeneral_KeyEstablish_InitiateKeyEstablishment(simpleMeterTaskID, 
                                          &ESPAddr, simpleMeterTransID);

    return ( events ^ SIMPLEMETER_KEY_ESTABLISHMENT_REQUEST_EVT );
  }

  /* 发送报告属性事件 */ 
  if ( events & SIMPLEMETER_REPORT_ATTRIBUTE_EVT )
  {
    if ( pReportCmd != NULL )
    {
      zcl_SendReportCmd( SIMPLEMETER_ENDPOINT, &ESPAddr,
                         ZCL_CLUSTER_ID_SE_SIMPLE_METERING, pReportCmd,
                         ZCL_FRAME_SERVER_CLIENT_DIR, 1, 0 );

      osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_REPORT_ATTRIBUTE_EVT,
                          SIMPLEMETER_REPORT_PERIOD );
    }

    return ( events ^ SIMPLEMETER_REPORT_ATTRIBUTE_EVT );
  }

  /* 被一个识别命令触发的识别超时事件处理 */
  if ( events & SIMPLEMETER_IDENTIFY_TIMEOUT_EVT )
  {
    if ( simpleMeterIdentifyTime > 0 )
    {
      simpleMeterIdentifyTime--;
    }
    simplemeter_ProcessIdentifyTimeChange();

    return ( events ^ SIMPLEMETER_IDENTIFY_TIMEOUT_EVT );
  }

  /* 获取当前时间事件 */ 
  if ( events & SIMPLEMETER_UPDATE_TIME_EVT )
  {
    simpleMeterTime = osal_getClock();
    osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_UPDATE_TIME_EVT, 
                        SIMPLEMETER_UPDATE_TIME_PERIOD );

    return ( events ^ SIMPLEMETER_UPDATE_TIME_EVT );
  }

  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：simplemeter_ProcessIdentifyTimeChange
 * 功    能：调用闪烁LED来指定识别时间属性值。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_ProcessIdentifyTimeChange( void )
{
  if ( simpleMeterIdentifyTime > 0 )
  {
    osal_start_timerEx( simpleMeterTaskID, SIMPLEMETER_IDENTIFY_TIMEOUT_EVT, 1000 );
    HalLedBlink ( HAL_LED_4, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
    osal_stop_timerEx( simpleMeterTaskID, SIMPLEMETER_IDENTIFY_TIMEOUT_EVT );
  }
}


#if defined ( ZCL_KEY_ESTABLISH )
/*********************************************************************
 * 函数名称：simplemeter_KeyEstablish_ReturnLinkKey
 * 功    能：获取被请求的密钥函数。
 * 入口参数：shortAddr 短地址
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 simplemeter_KeyEstablish_ReturnLinkKey( uint16 shortAddr )
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
 * 函数名称：simplemeter_HandleKeys
 * 功    能：简单计量器的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_2     SK-SmartRF10BB的K2键       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_HandleKeys( uint8 shift, uint8 keys )
{
  if ( keys & HAL_KEY_SW_2 )
  {
    ZDOInitDevice(0); // 加入网络
  }
}


/*********************************************************************
 * 函数名称：simplemeter_ValidateAttrDataCB
 * 功    能：检查为属性提供的数据值是否在该属性指定的范围内。
 * 入口参数：pAttr     指向属性的指针
 *           pAttrInfo 指向属性信息的指针            
 * 出口参数：无
 * 返 回 值：TRUE      有效数据
 *           FALSE     无效数据
 ********************************************************************/
static uint8 simplemeter_ValidateAttrDataCB( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo )
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
 * 函数名称：simplemeter_BasicResetCB
 * 功    能：ZCL通用簇库回调函数。设置所有簇的所有属性为出厂默认设置。
 * 入口参数：无          
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_BasicResetCB( void )
{
  // 在此添加代码
}


/*********************************************************************
 * 函数名称：simplemeter_IdentifyCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别命令后被调用。
 * 入口参数：pCmd      指向识别命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_IdentifyCB( zclIdentify_t *pCmd )
{
  simpleMeterIdentifyTime = pCmd->identifyTime;
  simplemeter_ProcessIdentifyTimeChange();
}


/*********************************************************************
 * 函数名称：simplemeter_IdentifyQueryRspCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序识别查询命令
 *           后被调用。
 * 入口参数：pRsp    指向识别查询响应结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_IdentifyQueryRspCB( zclIdentifyQueryRsp_t *pRsp )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：simplemeter_AlarmCB
 * 功    能：ZCL通用簇库回调函数。当接收到一个应用程序报警请求命令
 *           后被调用。
 * 入口参数：pAlarm   指向报警命令结构的指针       
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_AlarmCB( zclAlarm_t *pAlarm )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：simplemeter_GetProfileCmdCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           获取配置文件后被调用。
 * 入口参数：pCmd     指向获取配置文件结构的指针     
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_GetProfileCmdCB( zclCCGetProfileCmd_t *pCmd, afAddrType_t *srcAddr, uint8 seqNum )
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
  zclSE_SimpleMetering_Send_GetProfileRsp( SIMPLEMETER_ENDPOINT, srcAddr, endTime,
                                           status,
                                           profileIntervalPeriod,
                                           numberOfPeriodDelivered, intervals,
                                           false, seqNum );
#endif 
}


/*********************************************************************
 * 函数名称：simplemeter_GetProfileRspCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           获取配置文件响应后被调用。
 * 入口参数：pCmd     指向获取配置文件响应结构的指针     
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_GetProfileRspCB( zclCCGetProfileRsp_t *pCmd, afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：simplemeter_ReqMirrorCmdCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像命令后被调用。
 * 入口参数：srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_ReqMirrorCmdCB( afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：simplemeter_ReqMirrorRspCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像响应后被调用。
 * 入口参数：pRsp     指向请求镜像响应结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_ReqMirrorRspCB( zclCCReqMirrorRsp_t *pRsp, afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：simplemeter_MirrorRemCmdCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像移除命令后被调用。
 * 入口参数：srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_MirrorRemCmdCB( afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/*********************************************************************
 * 函数名称：simplemeter_MirrorRemRspCB
 * 功    能：ZCL SE 配置文件简单计量簇库回调函数。当接收到一个应用程序
 *           接收到请求镜像移除响应后被调用。
 * 入口参数：pCmd     指向镜像移除响应结构的指针
 *           srcAddr  指向源地址指针
 *           seqNum   该命令的序列号
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_MirrorRemRspCB( zclCCMirrorRemRsp_t *pCmd, 
                                  afAddrType_t *srcAddr, uint8 seqNum )
{
  // 在这里添加用户代码
}


/* 
  处理ZCL基础输入命令/响应消息
 */
/*********************************************************************
 * 函数名称：simplemeter_ProcessZCLMsg
 * 功    能：处理ZCL基础输入消息函数
 * 入口参数：inMsg           待处理的消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void simplemeter_ProcessZCLMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#if defined ( ZCL_READ )
    case ZCL_CMD_READ_RSP:
      simplemeter_ProcessInReadRspCmd( pInMsg );
      break;
#endif 

#if defined ( ZCL_WRITE )
    case ZCL_CMD_WRITE_RSP:
      simplemeter_ProcessInWriteRspCmd( pInMsg );
      break;
#endif 

#if defined ( ZCL_REPORT )
    case ZCL_CMD_CONFIG_REPORT:
      simplemeter_ProcessInConfigReportCmd( pInMsg );
      break;

    case ZCL_CMD_CONFIG_REPORT_RSP:
      simplemeter_ProcessInConfigReportRspCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG:
      simplemeter_ProcessInReadReportCfgCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG_RSP:
      simplemeter_ProcessInReadReportCfgRspCmd( pInMsg );
      break;

    case ZCL_CMD_REPORT:
      simplemeter_ProcessInReportCmd( pInMsg );
      break;
#endif

    case ZCL_CMD_DEFAULT_RSP:
      simplemeter_ProcessInDefaultRspCmd( pInMsg );
      break;
#if defined ( ZCL_DISCOVER )
    case ZCL_CMD_DISCOVER_RSP:
      simplemeter_ProcessInDiscRspCmd( pInMsg );
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
 * 函数名称：simplemeter_ProcessInReadRspCmd
 * 功    能：处理配置文件读响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 simplemeter_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：simplemeter_ProcessInWriteRspCmd
 * 功    能：处理配置文件写响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 simplemeter_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：simplemeter_ProcessInConfigReportCmd
 * 功    能：处理配置文件配置报告命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TURE            在属性列表中找到属性
 *           FALSE           未找到
 ********************************************************************/
static uint8 simplemeter_ProcessInConfigReportCmd( zclIncomingMsg_t *pInMsg )
{
  zclCfgReportCmd_t *cfgReportCmd;
  zclCfgReportRec_t *reportRec;
  zclCfgReportRspCmd_t *cfgReportRspCmd;
  zclAttrRec_t attrRec;
  uint8 status;
  uint8 i, j = 0;

  cfgReportCmd = (zclCfgReportCmd_t *)pInMsg->attrCmd;

  /* 为响应命令分配空间 */
  cfgReportRspCmd = (zclCfgReportRspCmd_t *)osal_mem_alloc( sizeof ( zclCfgReportRspCmd_t ) + \
                                        sizeof ( zclCfgReportStatus_t) * cfgReportCmd->numAttr );
  if ( cfgReportRspCmd == NULL )
    return FALSE; // EMBEDDED RETURN

  /* 处理每个报告配置记录属性 */
  for ( i = 0; i < cfgReportCmd->numAttr; i++ )
  {
    reportRec = &(cfgReportCmd->attrList[i]);

    status = ZCL_STATUS_SUCCESS;

    if ( zclFindAttrRec( SIMPLEMETER_ENDPOINT, pInMsg->clusterId, reportRec->attrID, &attrRec ) )
    {
      if ( reportRec->direction == ZCL_SEND_ATTR_REPORTS )
      {
        if ( reportRec->dataType == attrRec.attr.dataType )
        {
          /* 将被报告的属性 */ 
          if ( zcl_MandatoryReportableAttribute( &attrRec ) == TRUE )
          {
            if ( reportRec->minReportInt < SIMPLEMETER_MIN_REPORTING_INTERVAL ||
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
              status = ZCL_STATUS_UNREPORTABLE_ATTRIBUTE; // for now
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
          status = ZCL_STATUS_UNSUPPORTED_ATTRIBUTE; // for now
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
  zcl_SendConfigReportRspCmd( SIMPLEMETER_ENDPOINT, &(pInMsg->srcAddr),
                              pInMsg->clusterId, cfgReportRspCmd, 
                              ZCL_FRAME_SERVER_CLIENT_DIR,
                              true, pInMsg->zclHdr.transSeqNum );

  osal_mem_free( cfgReportRspCmd );

  return TRUE ;
}


/*********************************************************************
 * 函数名称：simplemeter_ProcessInConfigReportRspCmd
 * 功    能：处理配置文件配置报告响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TURE            成功
 *           FALSE           未成功
 ********************************************************************/
static uint8 simplemeter_ProcessInConfigReportRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclCfgReportRspCmd_t *cfgReportRspCmd;
  zclAttrRec_t attrRec;
  uint8 i;

  cfgReportRspCmd = (zclCfgReportRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < cfgReportRspCmd->numAttr; i++)
  {
    if ( zclFindAttrRec( SIMPLEMETER_ENDPOINT, pInMsg->clusterId,
                         cfgReportRspCmd->attrList[i].attrID, &attrRec ) )
    {
      // 在此添加代码
    }
  }

  return TRUE;
}


/*********************************************************************
 * 函数名称：simplemeter_ProcessInReadReportCfgCmd
 * 功    能：处理配置文件读取报告配置命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 simplemeter_ProcessInReadReportCfgCmd( zclIncomingMsg_t *pInMsg )
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
    if ( zclFindAttrRec( SIMPLEMETER_ENDPOINT, pInMsg->clusterId,
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

    if ( zclFindAttrRec( SIMPLEMETER_ENDPOINT, pInMsg->clusterId,
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
  zcl_SendReadReportCfgRspCmd( SIMPLEMETER_ENDPOINT, &(pInMsg->srcAddr),
                               pInMsg->clusterId, readReportCfgRspCmd, 
                               ZCL_FRAME_SERVER_CLIENT_DIR,
                               true, pInMsg->zclHdr.transSeqNum );

  osal_mem_free( readReportCfgRspCmd );

  return TRUE;
}


/*********************************************************************
 * 函数名称：simplemeter_ProcessInReadReportCfgRspCmd
 * 功    能：处理配置文件读取报告配置响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 simplemeter_ProcessInReadReportCfgRspCmd( zclIncomingMsg_t *pInMsg )
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
 * 函数名称：simplemeter_ProcessInReportCmd
 * 功    能：处理配置文件报告命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static uint8 simplemeter_ProcessInReportCmd( zclIncomingMsg_t *pInMsg )
{
  zclReportCmd_t *reportCmd;
  uint8 i;

  reportCmd = (zclReportCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < reportCmd->numAttr; i++)
  {
    // 用户在此添加代码
  }

  return TRUE;
}
#endif


/*********************************************************************
 * 函数名称：simplemeter_ProcessInDefaultRspCmd
 * 功    能：处理配置文件默认响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 simplemeter_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  return TRUE;
}


#if defined ( ZCL_DISCOVER )
/*********************************************************************
 * 函数名称：simplemeter_ProcessInDiscRspCmd
 * 功    能：处理配置文件发现响应命令函数
 * 入口参数：inMsg           待处理的输入消息
 * 出口参数：无
 * 返 回 值：TRUE
 ********************************************************************/
static uint8 simplemeter_ProcessInDiscRspCmd( zclIncomingMsg_t *pInMsg )
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
