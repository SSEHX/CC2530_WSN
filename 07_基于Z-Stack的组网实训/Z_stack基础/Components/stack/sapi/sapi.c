/*******************************************************************************
 * 文件名称：sapi.c
 * 功    能：SAPI函数     
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

#include "osal_nv.h"
#include "NLMEDE.h"
#include "AF.h"
#include "OnBoard.h"
#include "nwk_util.h"
#include "ZDProfile.h"
#include "ZDObject.h"
#include "hal_led.h"
#include "hal_key.h"
#include "sapi.h"
#include "MT_SAPI.h"
/********************************************************************/


/* 外部变量 */
/********************************************************************/
extern uint8 zgStartDelay;
extern uint8 zgSapiEndpoint;
/********************************************************************/


/* 宏定义 */
/* 设置值范围：0xE0-0xEF */
/********************************************************************/
/*
  用户信息
 */
#define ZB_USER_MSG                       0xE0

/*
  发送数据确认
 */
#define SAPICB_DATA_CNF                   0xE0

/*
  绑定确认
 */
#define SAPICB_BIND_CNF                   0xE1

/*
  设备启动确认
 */
#define SAPICB_START_CNF                  0xE2
/********************************************************************/


/* 全局变量 */
/********************************************************************/
/*
   任务数组：存放每个任务的相应的处理函数指针
   注意：下表中各项的顺序必须与下面的osalInitTasks()函数
         中任务初始化调用的顺序一致
*/
const pTaskEventHandlerFn tasksArr[] = {
  macEventLoop,               // MAC层任务处理函数
  nwk_event_loop,             // 网络层任务处理函数
  Hal_ProcessEvent,           // 硬件抽象层任务处理函数
#if defined( MT_TASK )
  MT_ProcessEvent,            // 监控测试任务处理函数
#endif
  APS_event_loop,             // 应用支持子层任务处理函数
  ZDApp_event_loop,           // ZigBee设备应用任务处理函数
  SAPI_ProcessEvent           // 用户应用层任务处理函数，由用户自己编写
};


/* 本地变量 */
/********************************************************************/
const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );
uint16 *tasksEvents;
endPointDesc_t sapi_epDesc;
uint8 sapi_TaskID;
static uint16 sapi_bindInProgress;
/********************************************************************/


/* 本地函数 */
/********************************************************************/
void SAPI_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
static void SAPI_SendCback( uint8 event, uint8 status, uint16 data );
static void SAPI_StartConfirm( uint8 status );
static void SAPI_SendDataConfirm( uint8 handle, uint8 status );
static void SAPI_BindConfirm( uint16 commandId, uint8 status );
static void SAPI_FindDeviceConfirm( uint8 searchType,
                                        uint8 *searchKey, uint8 *result );
static void SAPI_ReceiveDataIndication( uint16 source,
                              uint16 command, uint16 len, uint8 *pData  );
static void SAPI_AllowBindConfirm( uint16 source );
/********************************************************************/



/*********************************************************************
 * 函数名称：zb_SystemReset
 * 功    能：系统复位
 * 入口参数：无
 * 出口参数：无 
 * 返 回 值：无
 ********************************************************************/
void zb_SystemReset ( void )
{
  SystemReset();
}


/*********************************************************************
 * 函数名称：zb_StartRequest
 * 功    能：开始ZigBee协议栈。当协议栈启动后，首先从NV中读取配置参数；
 *           然后设备加入该网络。
 * 入口参数：无
 * 出口参数：无 
 * 返 回 值：无
 ********************************************************************/
void zb_StartRequest()
{
  uint8 logicalType;

  /* 启动设备 */ 
  if ( zgStartDelay < NWK_START_DELAY )
    zgStartDelay = 0;
  else
    zgStartDelay -= NWK_START_DELAY;

  /* 检查设备类型与编译定义组合 */
  zb_ReadConfiguration( ZCD_NV_LOGICAL_TYPE, sizeof(uint8), &logicalType );
  if (  ( logicalType > ZG_DEVICETYPE_ENDDEVICE )        ||
#if defined( RTR_NWK )
  #if defined( ZDO_COORDINATOR )
          // 仅路由器或协调器有效 
         ( logicalType == ZG_DEVICETYPE_ENDDEVICE )   ||
  #else
          // 仅路由器有效
          ( logicalType != ZG_DEVICETYPE_ROUTER )     ||
  #endif
#else
  #if defined( ZDO_COORDINATOR )
          // 错误
          ( 1 )                                     ||
  #else
          // 仅终端设备有效
          ( logicalType != ZG_DEVICETYPE_ENDDEVICE )  ||
  #endif
#endif
          ( 0 ) )
   {
     /* 错误配置 */
     SAPI_SendCback( SAPICB_START_CNF, ZInvalidParameter, 0 );
   }
   else
   {
     ZDOInitDevice(zgStartDelay);
   }

  return;
}


/*********************************************************************
 * 函数名称：zb_BindDevice
 * 功    能：控制2个设备间建立或者解除绑定
 * 入口参数：create       TRUE表示创建绑定，FLASH 表示解除绑定 
 *           commandId    命令ID，绑定标识符，基于某命令的绑定
 *           pDestination 设备64位IEEE地址
 * 出口参数：绑定操作的状态   
 * 返 回 值：无
 ********************************************************************/
void zb_BindDevice ( uint8 create, uint16 commandId, uint8 *pDestination )
{
  zAddrType_t destination;
  uint8 ret = ZB_ALREADY_IN_PROGRESS;

  if ( create )
  {
    if (sapi_bindInProgress == 0xffff) 
    {
      if ( pDestination ) // 已知扩展地址绑定
      {
        destination.addrMode = Addr64Bit; 
        osal_cpyExtAddr( destination.addr.extAddr, pDestination );
        
        /* 调用APS绑定请求函数 */
        ret = APSME_BindRequest( sapi_epDesc.endPoint, commandId,
                                            &destination, sapi_epDesc.endPoint );

        if ( ret == ZSuccess )
        {
          /* 查找网络地址 */
          ZDP_NwkAddrReq(pDestination, ZDP_ADDR_REQTYPE_SINGLE, 0, 0 );
          osal_start_timerEx( ZDAppTaskID, ZDO_NWK_UPDATE_NV, 250 );
        }
      }
      else // 未知扩展地址绑定
      {
        ret = ZB_INVALID_PARAMETER;
        destination.addrMode = Addr16Bit;
        destination.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
        if ( ZDO_AnyClusterMatches( 1, &commandId, sapi_epDesc.simpleDesc->AppNumOutClusters,
                                                sapi_epDesc.simpleDesc->pAppOutClusterList ) )
        {
          /* 在允许绑定模式下，尝试匹配设备 */
          ret = ZDP_MatchDescReq( &destination, NWK_BROADCAST_SHORTADDR,
              sapi_epDesc.simpleDesc->AppProfId, 1, &commandId, 0, (cId_t *)NULL, 0 );
        }
        else if ( ZDO_AnyClusterMatches( 1, &commandId, sapi_epDesc.simpleDesc->AppNumInClusters,
                                                sapi_epDesc.simpleDesc->pAppInClusterList ) )
        {
          ret = ZDP_MatchDescReq( &destination, NWK_BROADCAST_SHORTADDR,
              sapi_epDesc.simpleDesc->AppProfId, 0, (cId_t *)NULL, 1, &commandId, 0 );
        }

        if ( ret == ZB_SUCCESS )
        {
          /* 设置定时器确保绑定完成 */
#if ( ZG_BUILD_RTR_TYPE )
          osal_start_timerEx(sapi_TaskID, ZB_BIND_TIMER, AIB_MaxBindingTime);
#else
          /* AIB_MaxBindingTime 未被设置 */
          osal_start_timerEx(sapi_TaskID, ZB_BIND_TIMER, zgApsDefaultMaxBindingTime);
#endif
          sapi_bindInProgress = commandId;
          return;
        }
      }
    }

    SAPI_SendCback( SAPICB_BIND_CNF, ret, commandId );
  }
  else
  {
    /* 解除本地绑定 */
    BindingEntry_t *pBind;

    /* 解除绑定 */
    while ( pBind = bindFind( sapi_epDesc.simpleDesc->EndPoint, commandId, 0 ) )
    {
      bindRemoveEntry(pBind);
    }
    osal_start_timerEx( ZDAppTaskID, ZDO_NWK_UPDATE_NV, 250 );
  }
  return;
}


/*********************************************************************
 * 函数名称：zb_PermitJoiningRequest
 * 功    能：控制设备允许加入网络，或者不允许加入网络
 * 入口参数：destination  设备的地址，通常为本地设备地址或表示所有
 *                        协调器或者路由器的特殊广播地址【0xFFFC】
 *                        通过这种方式可以控制单个设备或者整个网络。
 *           timeout      允许加入的规定时间（单位为秒）
 *                        如果是0x00,表示不允许加入网络
 *                        如果是0xff,表示一直允许加入网络
 * 出口参数：无   
 * 返 回 值：ZB_SUCCESS   允许加入网络
 *           其他错误代码 不允许加入
 ********************************************************************/
uint8 zb_PermitJoiningRequest ( uint16 destination, uint8 timeout )
{
#if defined( ZDO_MGMT_PERMIT_JOIN_REQUEST )
  zAddrType_t dstAddr;

  dstAddr.addrMode = Addr16Bit;
  dstAddr.addr.shortAddr = destination;

  return( (uint8) ZDP_MgmtPermitJoinReq( &dstAddr, timeout, 0, 0 ) );
#else
  (void)destination;
  (void)timeout;
  return ZUnsupportedMode;
#endif
}


/*********************************************************************
 * 函数名称：zb_AllowBind
 * 功    能：使设备在规定的时间内处于允许绑定模式。
 * 入口参数：timeout    允许绑定的规定时间（单位为秒）
 * 出口参数：ZB_SUCCESS 设备进入允许绑定模式，否则报错
 * 返 回 值：无
 ********************************************************************/
void zb_AllowBind ( uint8 timeout )
{
  osal_stop_timerEx(sapi_TaskID, ZB_ALLOW_BIND_TIMER);

  if ( timeout == 0 )
  {
    afSetMatch(sapi_epDesc.simpleDesc->EndPoint, FALSE);
  }
  else
  {
    afSetMatch(sapi_epDesc.simpleDesc->EndPoint, TRUE);
    if ( timeout != 0xFF )
    {
      if ( timeout > 64 )
      {
        timeout = 64;
      }
      osal_start_timerEx(sapi_TaskID, ZB_ALLOW_BIND_TIMER, timeout*1000);
    }
  }
  return;
}


/*********************************************************************
 * 函数名称：zb_SendDataRequest
 * 功    能：发送数据请求函数
 * 入口参数：destination    被发送数据的目的地址。该地址为：
 ×　　　　                 - 设备16位短地址【0-0xFFFD】
 *                          - ZB_BROADCAST_ADDR 向网络中的所有设备发送数据。
 *                          - ZB_BINDING_ADDR 向先前绑定的设备发送数据。
 *           commandId      随数据发送的命令ID。如果使用ZB_BINDING_ADDR
 *                          该命令同样被用来指示绑定被使用。
 *           len            pData数据长度
 *           pData          发送的数据指针
 *　         handle         指示发送数据请求
 ×          txOptions      如果为TRUE，则要求从目的地址返回应答
 ×          radius        最大跳数量
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_SendDataRequest ( uint16 destination, uint16 commandId, uint8 len,
                          uint8 *pData, uint8 handle, uint8 txOptions, uint8 radius )
{
  afStatus_t status;
  afAddrType_t dstAddr;

  txOptions |= AF_DISCV_ROUTE;

  /* 设置目的地址 */
  if (destination == ZB_BINDING_ADDR)
  {
    // 绑定
    dstAddr.addrMode = afAddrNotPresent;
  }
  else
  {
    // 使用短地址
    dstAddr.addr.shortAddr = destination;
    dstAddr.addrMode = afAddr16Bit;

    if ( ADDR_NOT_BCAST != NLME_IsAddressBroadcast( destination ) )
    {
      txOptions &= ~AF_ACK_REQUEST;
    }
  }

  /* 设置端点 */
  dstAddr.endPoint = sapi_epDesc.simpleDesc->EndPoint;

  /* 发送消息 */
  status = AF_DataRequest(&dstAddr, &sapi_epDesc, commandId, len,
                          pData, &handle, txOptions, radius);

  if (status != afStatus_SUCCESS)
  {
    SAPI_SendCback( SAPICB_DATA_CNF, status, handle );
  }
}


/*********************************************************************
 * 函数名称：zb_ReadConfiguration
 * 功    能：从NV读取配置信息。
 * 入口参数：configId    配置信息标识符。 
 *           len         配置信息数据长度
 *           pValue      存储配置信息缓冲区
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
uint8 zb_ReadConfiguration( uint8 configId, uint8 len, void *pValue )
{
  uint8 size;

  size = (uint8)osal_nv_item_len( configId );
  if ( size > len )
  {
    return ZFailure;
  }
  else
  {
    return( osal_nv_read(configId, 0, size, pValue) );
  }
}


/*********************************************************************
 * 函数名称：zb_WriteConfiguration
 * 功    能：向NV写入配置信息。
 * 入口参数：configId    配置信息标识符。 
 *           len         配置信息数据长度
 *           pValue      存储配置信息缓冲区
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
uint8 zb_WriteConfiguration( uint8 configId, uint8 len, void *pValue )
{
  return( osal_nv_write(configId, 0, len, pValue) );
}


/*********************************************************************
 * 函数名称：zb_GetDeviceInfo
 * 功    能：获取设备信息。
 * 入口参数：param    设备信息标识符。 
 *           pValue   存储设备信息缓冲区
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_GetDeviceInfo ( uint8 param, void *pValue )
{
  switch(param)
  {
    case ZB_INFO_DEV_STATE:
      osal_memcpy(pValue, &devState, sizeof(uint8));
      break;
    case ZB_INFO_IEEE_ADDR:
      osal_memcpy(pValue, &aExtendedAddress, Z_EXTADDR_LEN);
      break;
    case ZB_INFO_SHORT_ADDR:
      osal_memcpy(pValue, &_NIB.nwkDevAddress, sizeof(uint16));
      break;
    case ZB_INFO_PARENT_SHORT_ADDR:
      osal_memcpy(pValue, &_NIB.nwkCoordAddress, sizeof(uint16));
      break;
    case ZB_INFO_PARENT_IEEE_ADDR:
      osal_memcpy(pValue, &_NIB.nwkCoordExtAddress, Z_EXTADDR_LEN);
      break;
    case ZB_INFO_CHANNEL:
      osal_memcpy(pValue, &_NIB.nwkLogicalChannel, sizeof(uint8));
      break;
    case ZB_INFO_PAN_ID:
      osal_memcpy(pValue, &_NIB.nwkPanId, sizeof(uint16));
      break;
    case ZB_INFO_EXT_PAN_ID:
      osal_memcpy(pValue, &_NIB.extendedPANID, Z_EXTADDR_LEN);
      break;
  }
}


/*********************************************************************
 * 函数名称：zb_FindDeviceRequest
 * 功    能：确定网络中设备的短地址。设备通过调用本函数来初始化并且
 *           被在相同网络中的成员搜索到。当搜索完成后，
 *           zb_FindDeviceConfirm回调函数将被调用。
 * 入口参数：searchType  查找类型。 
 *           searchKey   查找值
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_FindDeviceRequest( uint8 searchType, void *searchKey )
{
  if (searchType == ZB_IEEE_SEARCH)
  {
    ZDP_NwkAddrReq((uint8*) searchKey, ZDP_ADDR_REQTYPE_SINGLE, 0, 0 );
  }
}


/*********************************************************************
 * 函数名称：SAPI_StartConfirm
 * 功    能：启动确认函数。本函数在开启一个设备操作请求后被ZigBee
 *           协议栈所调用。
 * 入口参数：status  启动操作的状态值。如果值为ZB_SUCCESS，指示启动
 *           设备操作成功完成。如果不是，则延时myStartRetryDelay后
 *           向操作系统发起MY_START_EVT事件。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_StartConfirm( uint8 status )
{
   /*是否定义了串口监控任务 */
#if defined ( MT_SAPI_CB_FUNC )
  if ( SAPICB_CHECK( SPI_CB_SAPI_START_CNF ) )
  {
    zb_MTCallbackStartConfirm( status );
  }
  else
#endif
  {
    zb_StartConfirm( status );
  }
}


/*********************************************************************
 * 函数名称：SAPI_SendDataConfirm
 * 功    能：发送数据确认函数。本函数在发送数据操作请求后被ZigBee协议栈
 *           所调用。
 * 入口参数：handle  数据传输识别句柄。
 *           status  操作状态。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_SendDataConfirm( uint8 handle, uint8 status )
{
  /*是否定义了串口监控任务 */  
#if defined ( MT_SAPI_CB_FUNC )
  if ( SAPICB_CHECK( SPI_CB_SAPI_SEND_DATA_CNF ) )
  {
    zb_MTCallbackSendDataConfirm( handle, status );
  }
  else
#endif
  {
    zb_SendDataConfirm( handle, status );
  }
}


/*********************************************************************
 * 函数名称：SAPI_BindConfirm
 * 功    能：绑定确认函数。本函数在绑定操作请求后被ZigBee协议栈所调用。
 * 入口参数：commandId  绑定确定后的命令ID。
 *           status     操作状态。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_BindConfirm( uint16 commandId, uint8 status )
{
  /*是否定义了串口监控任务 */
#if defined ( MT_SAPI_CB_FUNC )
  if ( SAPICB_CHECK( SPI_CB_SAPI_BIND_CNF ) )
  {
    zb_MTCallbackBindConfirm( commandId, status );
  }
  else
#endif
  {
    zb_BindConfirm( commandId, status );
  }
}


/*********************************************************************
 * 函数名称：SAPI_AllowBindConfirm
 * 功    能：允许绑定确认函数。指示另外一个设备试图绑定到该设备。
 * 入口参数：source   绑定源地址。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_AllowBindConfirm( uint16 source )
{
   /*是否定义了串口监控任务 */
#if defined ( MT_SAPI_CB_FUNC )
  if ( SAPICB_CHECK( SPI_CB_SAPI_ALLOW_BIND_CNF ) )
  {
    zb_MTCallbackAllowBindConfirm( source );
  }
  else
#endif
  {
    zb_AllowBindConfirm( source );
  }
}


/*********************************************************************
 * 函数名称：SAPI_FindDeviceConfirm
 * 功    能：查找设备确认函数。本函数在查找设备操作请求后被ZigBee
 *           协议栈所调用。
 * 入口参数：searchType  查找类型。 
 *           searchKey   查找值
 *           result      查找结果
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_FindDeviceConfirm( uint8 searchType, uint8 *searchKey, uint8 *result )
{
  /*是否定义了串口监控任务 */
#if defined ( MT_SAPI_CB_FUNC )
  if ( SAPICB_CHECK( SPI_CB_SAPI_FIND_DEV_CNF ) )
  {
    zb_MTCallbackFindDeviceConfirm( searchType, searchKey, result );
  }
  else
#endif
  {
    zb_FindDeviceConfirm( searchType, searchKey, result );
  }
}


/*********************************************************************
 * 函数名称：SAPI_ReceiveDataIndication
 * 功    能：接收数据处理函数。本函数被ZigBee协议栈调用，用来通知应用
 *           程序数据已经从各个设备中接收到。
 *           协议栈所调用。
 * 入口参数：source   接收源地址。 
 *           command  命令值。
 *           len      数据长度。
 *           pData    设备发送过来的数据串。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_ReceiveDataIndication( uint16 source, uint16 command, uint16 len, uint8 *pData  )
{
  /*是否定义了串口监控任务 */
#if defined ( MT_SAPI_CB_FUNC )
  if ( SAPICB_CHECK( SPI_CB_SAPI_RCV_DATA_IND ) )
  {
    zb_MTCallbackReceiveDataIndication( source, command, len, pData  );
  }
  else
#endif
  {
    zb_ReceiveDataIndication( source, command, len, pData  );
  }
}


/*********************************************************************
 * 函数名称：SAPI_ProcessEvent
 * 功    能：SimpleApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
UINT16 SAPI_ProcessEvent( byte task_id, UINT16 events )
{
  osal_event_hdr_t *pMsg;
  afIncomingMSGPacket_t *pMSGpkt;
  afDataConfirm_t *pDataConfirm;

  if ( events & SYS_EVENT_MSG )
  {
    pMsg = (osal_event_hdr_t *) osal_msg_receive( task_id );
    while ( pMsg )
    {
      switch ( pMsg->event )
      {
        /* ZDO信息输入事件 */
        case ZDO_CB_MSG:
          SAPI_ProcessZDOMsgs( (zdoIncomingMsg_t *)pMsg );
          break;

        /* AF数据确认事件 */
        case AF_DATA_CONFIRM_CMD:
          /* 若数据包发送后那么该信息将会被接收到，其状态为ZStatus_t类型 */
          /* 该消息定义在AF.h文件中 */
          pDataConfirm = (afDataConfirm_t *) pMsg;
          SAPI_SendDataConfirm( pDataConfirm->transID, pDataConfirm->hdr.status );
          break;
       
        /* AF输入信息事件 */ 
        case AF_INCOMING_MSG_CMD:
          pMSGpkt = (afIncomingMSGPacket_t *) pMsg;
          SAPI_ReceiveDataIndication( pMSGpkt->srcAddr.addr.shortAddr, pMSGpkt->clusterId,
                                    pMSGpkt->cmd.DataLength, pMSGpkt->cmd.Data);
          break;

        /* ZDO状态改变事件 */  
        case ZDO_STATE_CHANGE:
          /* 若设备是协调器或路由器或终端 */
          if (pMsg->status == DEV_END_DEVICE ||
              pMsg->status == DEV_ROUTER ||
              pMsg->status == DEV_ZB_COORD )
          {
            SAPI_StartConfirm( ZB_SUCCESS );
          }
          else  if (pMsg->status == DEV_HOLD ||
                  pMsg->status == DEV_INIT)
          {
            SAPI_StartConfirm( ZB_INIT );
          }
          break;

        /* ZDO匹配描述符响应事件 */  
        case ZDO_MATCH_DESC_RSP_SENT:
          SAPI_AllowBindConfirm( ((ZDO_MatchDescRspSent_t *)pMsg)->nwkAddr );
          break;

        /* 按键事件 */
        case KEY_CHANGE:
          zb_HandleKeys( ((keyChange_t *)pMsg)->state, ((keyChange_t *)pMsg)->keys );
          break;
          
        /* SAPI发送数据确认回调事件 */
        case SAPICB_DATA_CNF:
          SAPI_SendDataConfirm( (uint8)((sapi_CbackEvent_t *)pMsg)->data,
                                    ((sapi_CbackEvent_t *)pMsg)->hdr.status );
          break;

        /* SAPI绑定确认回调事件 */
        case SAPICB_BIND_CNF:
          SAPI_BindConfirm( ((sapi_CbackEvent_t *)pMsg)->data,
                              ((sapi_CbackEvent_t *)pMsg)->hdr.status );
          break;

        /* SAPI设备启动确认回调事件 */
        case SAPICB_START_CNF:
          SAPI_StartConfirm( ((sapi_CbackEvent_t *)pMsg)->hdr.status );
          break;

        default:
          if ( pMsg->event >= ZB_USER_MSG )
          {
            // 用户可在此处添加自己的处理代码
          }
          break;
      }

      /* 释放内存 */
      osal_msg_deallocate( (uint8 *) pMsg );
      
      /* 获取下一个系统消息事件 */
      pMsg = (osal_event_hdr_t *) osal_msg_receive( task_id );
    }

    /* 返回未处理的事件 */
    return (events ^ SYS_EVENT_MSG);
  }

  /* 允许绑定定时器事件 */
  if ( events & ZB_ALLOW_BIND_TIMER )
  {
    afSetMatch(sapi_epDesc.simpleDesc->EndPoint, FALSE);
    return (events ^ ZB_ALLOW_BIND_TIMER);
  }

  /* 绑定定时器事件 */
  if ( events & ZB_BIND_TIMER )
  {
    /* 向应用程序发送绑定确认回调函数 */
    SAPI_BindConfirm( sapi_bindInProgress, ZB_TIMEOUT );
    sapi_bindInProgress = 0xffff;

    return (events ^ ZB_BIND_TIMER);
  }

  /* 用户程序入口事件 */
  if ( events & ZB_ENTRY_EVENT )
  {
    uint8 startOptions;

    /* 执行操作系统事件处理函数 */
    zb_HandleOsalEvent( ZB_ENTRY_EVENT );

    /* 关闭LED4 */
    //HalLedSet (HAL_LED_4, HAL_LED_MODE_OFF);

    zb_ReadConfiguration( ZCD_NV_STARTUP_OPTION, sizeof(uint8), &startOptions );
    if ( startOptions & ZCD_STARTOPT_AUTO_START )
    {
      zb_StartRequest(); // 启动设备，建立/加入网络
    }
    else
    {
      /* 闪烁LED2,等待外部配置输入并且重启（等待按键来配置） */
      HalLedBlink(HAL_LED_2, 0, 50, 500);
      //HalLedBlink(HAL_LED_2, 0, 50, 200);  
    }

    return (events ^ ZB_ENTRY_EVENT );
  }

  /* 用户自定义事件 */
  if ( events & ( ZB_USER_EVENTS ) )
  {
    /* 执行操作系统事件处理函数 */
    zb_HandleOsalEvent( events );
  }

  return 0;
}


/*********************************************************************
 * 函数名称：SAPI_ProcessZDOMsgs
 * 功    能：SimpleApp的ZDO信息输入事件处理函数。
 * 入口参数：inMsg  输入信息
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  switch ( inMsg->clusterID )
  {
    case NWK_addr_rsp:
      {
        /* 发送搜索设备到应用程序 */
        ZDO_NwkIEEEAddrResp_t *pNwkAddrRsp = ZDO_ParseAddrRsp( inMsg );
        SAPI_FindDeviceConfirm( ZB_IEEE_SEARCH, (uint8*)&pNwkAddrRsp->nwkAddr, pNwkAddrRsp->extAddr );
      }
      break;

    case Match_Desc_rsp:
      {
        zAddrType_t dstAddr;
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );

        if ( sapi_bindInProgress != 0xffff )
        {
          /* 创建绑定表入口 */
          dstAddr.addrMode = Addr16Bit;
          dstAddr.addr.shortAddr = pRsp->nwkAddr;

          if ( APSME_BindRequest( sapi_epDesc.simpleDesc->EndPoint,
                     sapi_bindInProgress, &dstAddr, pRsp->epList[0] ) == ZSuccess )
          {
            osal_stop_timerEx(sapi_TaskID,  ZB_BIND_TIMER);
            osal_start_timerEx( ZDAppTaskID, ZDO_NWK_UPDATE_NV, 250 );

            /* 查询IEEE地址 */
            ZDP_IEEEAddrReq( pRsp->nwkAddr, ZDP_ADDR_REQTYPE_SINGLE, 0, 0 );

            /* 发送绑定确认到应用程序 */
            zb_BindConfirm( sapi_bindInProgress, ZB_SUCCESS );
            sapi_bindInProgress = 0xffff;
          }
        }
      }
      break;
  }
}


/*********************************************************************
 * 函数名称：SAPI_Init
 * 功    能：SimpleApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_Init( byte task_id )
{
  uint8 startOptions;

  sapi_TaskID = task_id;

  sapi_bindInProgress = 0xffff;

  /* 填充端点描述符 */ 
  sapi_epDesc.endPoint = zb_SimpleDesc.EndPoint;
  sapi_epDesc.task_id = &sapi_TaskID;
  sapi_epDesc.simpleDesc = (SimpleDescriptionFormat_t *)&zb_SimpleDesc;
  sapi_epDesc.latencyReq = noLatencyReqs;

  /* 注册端点描述符 */
  afRegister( &sapi_epDesc );

  /* 默认关闭匹配描述响应 */
  afSetMatch(sapi_epDesc.simpleDesc->EndPoint, FALSE);

  /* 注册回调事件 */
  ZDO_RegisterForZDOMsg( sapi_TaskID, NWK_addr_rsp );
  ZDO_RegisterForZDOMsg( sapi_TaskID, Match_Desc_rsp );

#if (defined HAL_KEY) && (HAL_KEY == TRUE)
  /* 注册硬件抽象层按键事件 */
  RegisterForKeys( sapi_TaskID );

  /* 判断SW5是否被按下 */
  /* SK-SmartRF05EB的SW5对应于CENTER键 */
  //if ( HalKeyRead () == HAL_KEY_SW_5)
  //{
    /* 如果在启动过程中按住SW5不放，那么将强制清除设备状态和配置后重启设备 */
  //  startOptions = ZCD_STARTOPT_CLEAR_STATE | ZCD_STARTOPT_CLEAR_CONFIG;
  //  zb_WriteConfiguration( ZCD_NV_STARTUP_OPTION, sizeof(uint8), &startOptions );
 //   zb_SystemReset();
 // }
#endif

  /* 设置事件启动应用程序 */
  osal_set_event(task_id, ZB_ENTRY_EVENT);
}


/*********************************************************************
 * 函数名称：SAPI_SendCback
 * 功    能：发送信息到SAPI自身任务函数，以便产生回调。
 * 入口参数：event  准备处理的事件。该变量是一个位图，可包含多个事件。
 *           status 状态标志位
 *           data   数据 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SAPI_SendCback( uint8 event, uint8 status, uint16 data )
{
  sapi_CbackEvent_t *pMsg;

  pMsg = (sapi_CbackEvent_t *)osal_msg_allocate( sizeof(sapi_CbackEvent_t) );
  if( pMsg )
  {
    pMsg->hdr.event = event;
    pMsg->hdr.status = status;
    pMsg->data = data;

    osal_msg_send( sapi_TaskID, (uint8 *)pMsg );
  }
}


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
  ZDApp_Init( taskID++ );
  SAPI_Init( taskID );
}


