/*******************************************************************************
 * 文件名称：TransmitApp.c
 * 功    能：TransmitApp实验。
 * 实验内容：一个ZigBee网络中的某个设备尽可能快地向另一个设备发送数据包（发送设
 *           备将一个数据包OTA发送，无论是否使用了APS ACK，只要收到确认就立刻发
 *           送下一个数据包）。
 *           发送设备同时会计算：
 *               每秒发送的平均字节数量
 *               累计发送的字节数量
 *           接收设备在接收到数据包后会计算：
 *               每秒接收到的平均字节数量
 *               累计接收到的字节数量
 * 
 *           若用户使能了LCD显示，在LCD上将按如下方式显示以上4个数据：
 *               aaaa   bbbb
 *               cccc   dddd
 *           其中：
 *                 aaaa 表示每秒接收到的平均字节数量
 *                 bbbb 表示累计接收到的字节数量
 *                 cccc 表示每秒发送的平均字节数量
 *                 dddd 表示累计发送的字节数量
 *
 * 注    意：本实验可被用来进行一个协调器与一个路由器之间的最大吞吐量测试。默认
 *           不使用APS ACK机制，若使用该机制，吞吐量将大大下降。 
 *           本实验也可被用来进行一个协调器（或路由器）与一个终端设备之间的最大
 *           吞吐量测试。由于终端设备的特性（存在RESPONSE_POLL_RATE，该值在
 *           f8wConfig.cfg文件中定义），每次数据发送必须要进行延时。延时时间要远
 *           大于RESPONSE_POLL_RATE。本实验中默认为2倍的RESPONSE_POLL_RATE时间。
 *           
 * 实验设备：SK-SmartRF05EB（装配有SK-CC2530EM）
 *           UP键：       切换发送/等待状态
 *           RIGHT键：    发送“终端设备绑定请求”
 *           DOWN键：     清零显示信息
 *           LEFT键：     发送“匹配描述符请求”
 *           LED_Y(黄色)：当成功建立网络或加入网络后此LED点亮
 *           LED_B(蓝色)：当与另一设备成功建立应用层的逻辑连接后此LED点亮
 *           
 ******************************************************************************/



/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "AF.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "TransmitApp.h"
#include "OnBoard.h"

#include "DebugTrace.h"

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
/********************************************************************/



/* 常量 */
/********************************************************************/

/*
   本应用的状态
 */
#define TRANSMITAPP_STATE_WAITING   0  // 等待
#define TRANSMITAPP_STATE_SENDING   1  // 发送

/*
   终端设备必须使用的延时（发送数据包之间的延时），以毫秒为单位
 */
#if !defined ( RTR_NWK )
  #define TRANSMITAPP_DELAY_SEND
  #define TRANSMITAPP_SEND_DELAY    (RESPONSE_POLL_RATE * 2)
#endif

/*
   数据包发送选项
 */
/* 需要APS ACK */
//#define TRANSMITAPP_TX_OPTIONS    (AF_DISCV_ROUTE | AF_ACK_REQUEST)  

/* 不需要APS ACK */
#define TRANSMITAPP_TX_OPTIONS    AF_DISCV_ROUTE  

/*
   进行一次数据发送需要连续发送的数据包数量
 */
#define TRANSMITAPP_INITIAL_MSG_COUNT  2

/*
   接收显示事件的超时触发时间
 */
#define TRANSMITAPP_DISPLAY_TIMER   (2 * 1000)

/*
   最大数据包长度
   若允许分片，则最大数据包长度为225字节；否则最大数据包长度为102字节
 */
#if defined ( TRANSMITAPP_FRAGMENTED )
#define TRANSMITAPP_MAX_DATA_LEN    225
#else
#define TRANSMITAPP_MAX_DATA_LEN    102
#endif

/* 
   簇ID列表 
   该列表应该用本应用专用的簇ID来填充
*/
const cId_t TransmitApp_ClusterList[TRANSMITAPP_MAX_CLUSTERS] =
{
  TRANSMITAPP_CLUSTERID_TESTMSG
};

/* 
   设备的简单描述符 
 */
const SimpleDescriptionFormat_t TransmitApp_SimpleDesc =
{
  TRANSMITAPP_ENDPOINT,              //  端点号
  TRANSMITAPP_PROFID,                //  Profile ID
  TRANSMITAPP_DEVICEID,              //  设备ID
  TRANSMITAPP_DEVICE_VERSION,        //  设备版本
  TRANSMITAPP_FLAGS,                 //  标识
  TRANSMITAPP_MAX_CLUSTERS,          //  输入簇的数量
  (cId_t *)TransmitApp_ClusterList,  //  输入簇列表
  TRANSMITAPP_MAX_CLUSTERS,          //  输出簇的数量
  (cId_t *)TransmitApp_ClusterList   //  输出簇列表
};

/* 
   端点描述符 
*/
const endPointDesc_t TransmitApp_epDesc =
{
  TRANSMITAPP_ENDPOINT,
  &TransmitApp_TaskID,
  (SimpleDescriptionFormat_t *)&TransmitApp_SimpleDesc,
  noLatencyReqs
};
/********************************************************************/



/* 变量 */
/********************************************************************/

/*
   本应用的任务ID变量。当TransmitApp_Init()函数被调用时，   该变量可以
   获得任务ID值。
*/
uint8 TransmitApp_TaskID;

/*
   发送数据缓冲区
 */
byte TransmitApp_Msg[ TRANSMITAPP_MAX_DATA_LEN ];

/*
   设备的网络状态变量：协调器/路由器/终端设备/设备不在网络中
*/
devStates_t TransmitApp_NwkState;

/*
   传输序号
 */
static byte TransmitApp_TransID;  

/*
   目的地址
 */
afAddrType_t TransmitApp_DstAddr;

/*
   本应用的状态
 */
byte TransmitApp_State;

/*
   OSAL系统时钟的影子，用来计算实际到期时间
 */
static uint32 clkShdw;

/*
   从开始当前运行到现在接收/发送测试数据的总数量
 */
static uint32 rxTotal, txTotal;

/*
   从上次显示/更新到现在接收/发送数据的数量
 */
static uint32 rxAccum, txAccum;

/*
   用来触发接收显示事件的超时计时器的打开标志
 */
static byte timerOn;

/*
   进行一次数据发送需要连续发送数据包的计数
 */
static byte timesToSend;

/*
   数据发送次数计数
 */
uint16 pktCounter;

/*
   被发送数据包的最大数据长度
 */
uint16 TransmitApp_MaxDataLength;
/********************************************************************/



/* 本地函数 */
/********************************************************************/
void TransmitApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
void TransmitApp_HandleKeys( byte shift, byte keys );
void TransmitApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void TransmitApp_SendTheMessage( void );
void TransmitApp_ChangeState( void );
/********************************************************************/



/* 公有函数 */
/********************************************************************/
void TransmitApp_DisplayResults( void );
/********************************************************************/



/*********************************************************************
 * 函数名称：SerialApp_Init
 * 功    能：SerialApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_Init( byte task_id )
{
#if !defined ( TRANSMITAPP_FRAGMENTED )
  afDataReqMTU_t mtu;
#endif
  uint16 i;

  TransmitApp_TaskID = task_id;
  TransmitApp_NwkState = DEV_INIT;
  TransmitApp_TransID = 0;

  pktCounter = 0;

  TransmitApp_State = TRANSMITAPP_STATE_WAITING;

  /* 初始化目的地址 */
  TransmitApp_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  TransmitApp_DstAddr.endPoint = 0;
  TransmitApp_DstAddr.addr.shortAddr = 0;

  /* 注册端点描述符 */
  afRegister( (endPointDesc_t *)&TransmitApp_epDesc );

  /* 注册按键事件，将所有按键事件发送给本应用任务TransmitApp_TaskID */
  RegisterForKeys( TransmitApp_TaskID );

  /* 若包含了LCD_SUPPORTED编译选项，则在LCD上进行相应的显示 */
#if defined ( LCD_SUPPORTED )
#if defined ( ZIGBEEPRO )
  HalLcdWriteString( "TransmitApp(ZB_PRO)", HAL_LCD_LINE_2 );
#else
  HalLcdWriteString( "TransmitApp(ZB_2007)", HAL_LCD_LINE_2 );
#endif
#endif

/* 若采用数据分片 */
#if defined ( TRANSMITAPP_FRAGMENTED )
  TransmitApp_MaxDataLength = TRANSMITAPP_MAX_DATA_LEN;
/* 若不采用数据分片 */
#else
  mtu.kvp        = FALSE;
  mtu.aps.secure = FALSE;
  TransmitApp_MaxDataLength = afDataReqMTU( &mtu );
#endif

  /* 产生发送数据 */
  for (i=0; i<TransmitApp_MaxDataLength; i++)
  {
    TransmitApp_Msg[i] = (uint8) i;
  }

  /* ZDO信息注册 */
  /* 注册ZDO的簇End_Device_Bind_rsp，将收到的End_Device_Bind_rsp事件
     发送给本应用任务SerialApp_TaskID
   */      
  ZDO_RegisterForZDOMsg( TransmitApp_TaskID, End_Device_Bind_rsp );
   
  /* ZDO信息注册 */
  /* 注册ZDO的簇Match_Desc_rsp，将收到的Match_Desc_rsp事件发送给本应
     用任务SerialApp_TaskID
   */    
  ZDO_RegisterForZDOMsg( TransmitApp_TaskID, Match_Desc_rsp );
}


/*********************************************************************
 * 函数名称：TransmitApp_ProcessEvent
 * 功    能：TransmitApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
UINT16 TransmitApp_ProcessEvent( byte task_id, UINT16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  afDataConfirm_t *afDataConfirm;

  /* 定义用于保存AF数据确认信息各字段值的变量 */
  ZStatus_t sentStatus;
  byte sentEP;

  /* 系统消息事件 */
  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( TransmitApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      { /* ZDO信息输入事件 */
        case ZDO_CB_MSG:
          // 调用ZDO信息输入事件处理函数
          TransmitApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;
         
        /* 按键事件 */  
        case KEY_CHANGE:
          // 调用按键事件处理函数
          TransmitApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, 
			          ((keyChange_t *)MSGpkt)->keys );
          break;

        /* AF数据确认事件 */
        case AF_DATA_CONFIRM_CMD:
          // 读取AF数据确认信息并做相应处理
          afDataConfirm = (afDataConfirm_t *)MSGpkt;
          sentEP = afDataConfirm->endpoint;
          sentStatus = afDataConfirm->hdr.status;

          /* 若数据被成功进行了OTA发送 */
          if ( (ZSuccess == sentStatus) && (TransmitApp_epDesc.endPoint == sentEP))
          {
/* 若数据长度不为随机长度 */
#if !defined ( TRANSMITAPP_RANDOM_LEN )
            txAccum += TransmitApp_MaxDataLength;  // 累计发送数据的数量
#endif
            /* 若用来触发接收显示事件的超时计时器未被打开 */
            if ( !timerOn )
            {
              /* 
                 在指定的TRANSMITAPP_DISPLAY_TIMER时间到期后触发接收显示
                 事件TRANSMITAPP_RCVTIMER_EVT
               */
              osal_start_timerEx( TransmitApp_TaskID,TRANSMITAPP_RCVTIMER_EVT,
                                                     TRANSMITAPP_DISPLAY_TIMER);
              
              clkShdw = osal_GetSystemClock();  // 获取当前系统时钟计数
              
              /* 
                 设置用来触发接收显示事件的超时计时器状态变量为TURE
                 即用来触发接收显示事件的超时计时器已被打开
               */
              timerOn = TRUE;
            }
          }

          TransmitApp_SetSendEvt();  // 当收到确认后发送下一个数据包
          break;

        /* AF输入信息事件 */
        case AF_INCOMING_MSG_CMD:
          // 调用输入信息事件处理函数
          TransmitApp_MessageMSGCB( MSGpkt );
          break;

        /* ZDO状态改变事件 */
        case ZDO_STATE_CHANGE:
          // 读取设备状态
          TransmitApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );  // 释放存储器

      // 获取下一个系统消息事件
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( TransmitApp_TaskID );
    }

    return (events ^ SYS_EVENT_MSG);  // 返回未处理的事件
  }

  /* 数据发送事件 */
  if ( events & TRANSMITAPP_SEND_MSG_EVT )
  {
    /* 若为发送状态 */
    if ( TransmitApp_State == TRANSMITAPP_STATE_SENDING )
    { 
      /* 调用数据发送事件处理函数 */
      TransmitApp_SendTheMessage();
    }

    return (events ^ TRANSMITAPP_SEND_MSG_EVT);
  }

  /* 发送错误事件 */
  if ( events & TRANSMITAPP_SEND_ERR_EVT )
  {
    TransmitApp_SetSendEvt();  // 设置数据发送事件标志

    return (events ^ TRANSMITAPP_SEND_ERR_EVT);
  }

  /* 接收显示事件 */
  if ( events & TRANSMITAPP_RCVTIMER_EVT )
  {
    /* 
       在指定的TRANSMITAPP_DISPLAY_TIMER时间到期后触发接收显示
       事件TRANSMITAPP_RCVTIMER_EVT
     */
    osal_start_timerEx( TransmitApp_TaskID, TRANSMITAPP_RCVTIMER_EVT,
                                            TRANSMITAPP_DISPLAY_TIMER );
    TransmitApp_DisplayResults();  // 更新显示结果

    return (events ^ TRANSMITAPP_RCVTIMER_EVT);
  }
  
  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：TransmitApp_ProcessZDOMsgs
 * 功    能：TransmitApp的ZDO信息输入事件处理函数。
 * 入口参数：inMsg  指向输入的ZDO信息的指针
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  /* ZDO信息中的簇ID */
  switch ( inMsg->clusterID )
  { /* ZDO的簇End_Device_Bind_rsp */
    case End_Device_Bind_rsp:
      /* 若绑定成功 */
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
        /* 点亮LED4 */
        /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
        HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
      }
/* 若定义了BLINK_LEDS宏 */       
#if defined(BLINK_LEDS)
      /* 若绑定失败 */
      else
      {
        /* 闪烁LED4 */
        /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
      }
#endif
      break;
      
    /* ZDO的簇Match_Desc_rsp */  
    case Match_Desc_rsp:
      {  /* 获取指向响应信息的指针 */
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        /* 若指向响应信息的指针变量pRsp的值不为空(NULL) */
        if ( pRsp )
        { /* 指向响应信息的指针变量pRsp的值有效 */
          if ( pRsp->status == ZSuccess && pRsp->cnt )
          {
            /* 设置目的地址 */
            TransmitApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            TransmitApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            // 采用第一个端点，用户可以修改此处代码以搜索所有端点
            TransmitApp_DstAddr.endPoint = pRsp->epList[0];
            
            /* 点亮LED4 */
            /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
          }
          osal_mem_free( pRsp );  // 释放指针变量pRsp所占用的存储器
        }
      }
      break;
  }
}


/*********************************************************************
 * 函数名称：TransmitApp_HandleKeys
 * 功    能：TransmitApp的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的项目为：
 *                  HAL_KEY_SW_1     SK-SmartRF05EB的UP键
 *                  HAL_KEY_SW_2     SK-SmartRF05EB的RIGHT键
 *                  HAL_KEY_SW_3     SK-SmartRF05EB的DOWN键
 *                  HAL_KEY_SW_4     SK-SmartRF05EB的LEFT键
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_HandleKeys( byte shift, byte keys )
{
  zAddrType_t dstAddr;

  /* HAL_KEY_SW_1被按下，切换本应用状态（发送/等待） */
  /* HAL_KEY_SW_1对应SK-SmartRF05EB的UP键 */  
  if ( keys & HAL_KEY_SW_1 )
  {
    TransmitApp_ChangeState();
  }

  /* HAL_KEY_SW_2被按下，发送“终端设备绑定请求” */
  /* HAL_KEY_SW_2对应SK-SmartRF05EB的RIGHT键 */
  if ( keys & HAL_KEY_SW_2 )
  {
    /* 熄灭LED4 */
    /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );

    /* 为强制端点发送一个“终端设备绑定请求”（单播）*/
    dstAddr.addrMode = Addr16Bit;
    dstAddr.addr.shortAddr = 0x0000; // 目的地址为网络中协调器的16位短地址0x0000
    ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(),
                            TransmitApp_epDesc.endPoint,
                            TRANSMITAPP_PROFID,
                            TRANSMITAPP_MAX_CLUSTERS, (cId_t *)TransmitApp_ClusterList,
                            TRANSMITAPP_MAX_CLUSTERS, (cId_t *)TransmitApp_ClusterList,
                            FALSE );
  }

  /* HAL_KEY_SW_3被按下，清零显示信息 */
  /* HAL_KEY_SW_3对应SK-SmartRF05EB的DOWN键 */  
  if ( keys & HAL_KEY_SW_3 )
  {
    rxTotal = txTotal = 0;
    rxAccum = txAccum = 0;
    TransmitApp_DisplayResults();
  }

  /* HAL_KEY_SW_4被按下，发送“匹配描述符请求” */
  /* HAL_KEY_SW_4对应SK-SmartRF05EB的LEFT键 */  
  if ( keys & HAL_KEY_SW_4 )
  {
    /* 熄灭LED4 */
    /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */    
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );

    /* 发送一个“匹配描述符请求”（广播）*/
    dstAddr.addrMode = AddrBroadcast;
    dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
    ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        TRANSMITAPP_PROFID,
                        TRANSMITAPP_MAX_CLUSTERS, (cId_t *)TransmitApp_ClusterList,
                        TRANSMITAPP_MAX_CLUSTERS, (cId_t *)TransmitApp_ClusterList,
                        FALSE );
  }
}


/*********************************************************************
 * 函数名称：TransmitApp_MessageMSGCB
 * 功    能：TransmitApp的输入信息事件处理函数。本函数处理来自其他设备的
 *           任何输入数据。因此，可基于簇ID执行预定的动作。
 * 入口参数：pkt  指向输入信息的指针
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  int16 i;
  uint8 error = FALSE;

  /* AF输入信息中的簇 */
  switch ( pkt->clusterId )
  { /* 簇TRANSMITAPP_CLUSTERID_TESTMSG */
    case TRANSMITAPP_CLUSTERID_TESTMSG:
#if !defined ( TRANSMITAPP_RANDOM_LEN )      
      /* 若接收数据包长度不正确 */
      if (pkt->cmd.DataLength != TransmitApp_MaxDataLength)
      {
        error = TRUE;  // 置错误标志为TRUE
      }
#endif
      
      /* 判断数据包中的数据是否产生了误码 */
      for (i=4; i<pkt->cmd.DataLength; i++)
      {
        if (pkt->cmd.Data[i] != i%256)
          error = TRUE;
      }

      /* 若产生了传输错误 */
      if (error)
      {
        /* 点亮LED2，用于指示产生了传输错误 */
        /* 对于SK-SmartRF05EB而言，LED2是LED_R（红色） */
        HalLedSet(HAL_LED_2, HAL_LED_MODE_ON);
      }
      /* 若没有产生传输错误 */
      else
      { /* 若用来触发接收显示事件的超时计时器未被打开 */
        if ( !timerOn )
        {
          /* 
             在指定的TRANSMITAPP_DISPLAY_TIMER时间到期后触发接收显示
             事件TRANSMITAPP_RCVTIMER_EVT
           */          
          osal_start_timerEx( TransmitApp_TaskID, TRANSMITAPP_RCVTIMER_EVT,
                                                  TRANSMITAPP_DISPLAY_TIMER );
         
          clkShdw = osal_GetSystemClock();  // 获取当前系统时钟计数
              
          /* 
             设置用来触发接收显示事件的超时计时器状态变量为TURE
             即用来触发接收显示事件的超时计时器已被打开
           */
          timerOn = TRUE;
        }
        rxAccum += pkt->cmd.DataLength;  // 累计接收数据的数量
      }
      break;

    default:
      break;
  }
}


/*********************************************************************
 * 函数名称：TransmitApp_SendTheMessage
 * 功    能：TransmitApp的数据发送事件处理函数
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_SendTheMessage( void )
{
  uint16 len;
  uint8 tmp;

  /* 进行一次数据发送：连续发送timesToSend次数据包 */
  /* 若在连续发送数据包次数未达到timesToSend之前出现发送错误，则跳出循环 */
  do
  {  
    /* 将发送序号TransmitApp_TransID存入发送数据缓冲区的第2、3字节 */
    tmp = HI_UINT8( TransmitApp_TransID );
    tmp += (tmp <= 9) ? ('0') : ('A' - 0x0A);
    TransmitApp_Msg[2] = tmp;
    tmp = LO_UINT8( TransmitApp_TransID );
    tmp += (tmp <= 9) ? ('0') : ('A' - 0x0A);
    TransmitApp_Msg[3] = tmp;
    
    len = TransmitApp_MaxDataLength;  // 获取被发送数据包的最大长度

  /* 若被发送数据包的长度使用随机长度 */
#if defined ( TRANSMITAPP_RANDOM_LEN )
    len = (uint8)(osal_rand() & 0x7F);  // 获取被发送数据包的随机长度
    /* 若获取到的被发送数据包的随机长度大于被发送数据包的最大长度或等于0 */
    if( len > TransmitApp_MaxDataLength || len == 0 )
      // 用被发送数据包的最大长度作为被发送数据包的长度
      len = TransmitApp_MaxDataLength;  
    /* 若获取到的被发送数据包的随机长度小于4 */
    else if ( len < 4 )
      // 被发送数据包的长度设置为4
      len = 4;    
#endif

    /* 进行数据包的OTA发送 */
    tmp = AF_DataRequest( &TransmitApp_DstAddr, 
                          (endPointDesc_t *)&TransmitApp_epDesc,
                           TRANSMITAPP_CLUSTERID_TESTMSG,
                           len, TransmitApp_Msg,
                          &TransmitApp_TransID,
                           TRANSMITAPP_TX_OPTIONS,
                           AF_DEFAULT_RADIUS );

/* 若被发送数据包使用随机长度 */    
#if defined ( TRANSMITAPP_RANDOM_LEN )
    /* 若数据包被成功进行了OTA发送 */
    if ( tmp == afStatus_SUCCESS )
    {
      txAccum += len;  // 累计发送数据的数量
    }
#endif    
    
    /* 更新数据包发送次数计数 */
    if ( timesToSend )
    {
      timesToSend--;  
    }
  } 
  while ( (timesToSend != 0) && (afStatus_SUCCESS == tmp) );

  /* 若一次数据发送成功 */
  if ( afStatus_SUCCESS == tmp )
  {
    pktCounter++;  // 数据发送次数累加
  }
  /* 若一次数据发送不成功 */
  else
  {
    /* 在指定时间10ms到时后触发发送错误事件TRANSMITAPP_SEND_ERR_EVT */
    osal_start_timerEx( TransmitApp_TaskID, TRANSMITAPP_SEND_ERR_EVT, 10 );
  }
}


/*********************************************************************
 * 函数名称：TransmitApp_ChangeState
 * 功    能：切换发送/等待状态标志
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_ChangeState( void )
{
  /* 若为等待状态 */
  if ( TransmitApp_State == TRANSMITAPP_STATE_WAITING )
  {
    /* 切换等待状态为发送状态 */
    TransmitApp_State = TRANSMITAPP_STATE_SENDING; // 更新状态标志为发送
    TransmitApp_SetSendEvt();                      // 设置数据发送事件标志
    timesToSend = TRANSMITAPP_INITIAL_MSG_COUNT;   // 初始化数据包发送次数计数
  }
  /* 若为发送状态 */
  else
  {
    TransmitApp_State = TRANSMITAPP_STATE_WAITING; // 更新状态显示标志为等待
  }
}


/*********************************************************************
 * 函数名称：TransmitApp_SetSendEvt
 * 功    能：设置数据发送事件标志
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_SetSendEvt( void )
{
/* 若定义了延时发送（使用终端设备）*/
#if defined( TRANSMITAPP_DELAY_SEND )
  /* 
     在指定的TRANSMITAPP_SEND_DELAY时间到期后触发
     数据发送事件TRANSMITAPP_SEND_MSG_EVT
  */
  osal_start_timerEx( TransmitApp_TaskID,
                      TRANSMITAPP_SEND_MSG_EVT, TRANSMITAPP_SEND_DELAY );

/* 若未定义延时发送（使用协调器或路由器）*/
#else
  /* 立刻触发数据发送事件TRANSMITAPP_SEND_MSG_EVT */
  osal_set_event( TransmitApp_TaskID, TRANSMITAPP_SEND_MSG_EVT );
#endif
}


/*********************************************************************
 * 函数名称：TransmitApp_DisplayResults
 * 功    能：更新显示结果
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void TransmitApp_DisplayResults( void )
{
#ifdef LCD_SUPPORTED
  #define LCD_W  21
  uint32 rxShdw, txShdw, tmp;
  byte lcd_buf[LCD_W+1];
  byte idx;
#endif
  
  /* 计算实际的统计时间 */
  uint32 msecs = osal_GetSystemClock() - clkShdw;
  clkShdw = osal_GetSystemClock();

  rxTotal += rxAccum;  // 接收数据总量累计
  txTotal += txAccum;  // 发送数据总量累计

/* 若包含了LCD_SUPPORTED编译选项，则在LCD上显示统计信息 */
#if defined ( LCD_SUPPORTED )
  /*
     计算每秒钟接收到的平均数据量（以字节为单位），结果四舍五入
  */
  rxShdw = (rxAccum * 1000 + msecs/2) / msecs;
 
  /*
     计算每秒钟发送出去的平均数据量（以字节为单位），结果四舍五入
  */    
  txShdw = (txAccum * 1000 + msecs/2) / msecs;

  /* 显示缓冲区初始化*/  
  osal_memset( lcd_buf, ' ', LCD_W );
  lcd_buf[LCD_W] = NULL;
  
  /* 显示每秒接收到的数据量以及累计接收到的数据量（以字节为单位） */
  idx = 4;
  tmp = (rxShdw >= 100000) ? 99999 : rxShdw;
  
  /* 每秒接收到的数据量 */
  do
  {
    lcd_buf[idx--] = (uint8) ('0' + (tmp % 10));
    tmp /= 10;
  } while ( tmp );

  idx = LCD_W-1;
  tmp = rxTotal;
  
  /* 累计接收到的数据量 */
  do
  {
    lcd_buf[idx--] = (uint8) ('0' + (tmp % 10));
    tmp /= 10;
  } while ( tmp );
  
  /* 在LCD上显示 */
  HalLcdWriteString( (char*)lcd_buf, HAL_LCD_LINE_4 );
  osal_memset( lcd_buf, ' ', LCD_W ); 

 /* 显示每秒发送出去的数据量以及累计发送出去的数据量（以字节为单位） */ 
  idx = 4;
  tmp = (txShdw >= 100000) ? 99999 : txShdw;
  
  /* 每秒发送出去的数据量 */
  do
  {
    lcd_buf[idx--] = (uint8) ('0' + (tmp % 10));
    tmp /= 10;
  } while ( tmp );

  idx = LCD_W-1;
  tmp = txTotal;
  
  /* 累计发送出去的数据量*/
  do
  {
    lcd_buf[idx--] = (uint8) ('0' + (tmp % 10));
    tmp /= 10;
  } while ( tmp );
  /* 在LCD上显示 */
  HalLcdWriteString( (char*)lcd_buf, HAL_LCD_LINE_5 );

/* 若未包含了LCD_SUPPORTED编译选项，则从串口输出统计信息 */
#else
  /* 注意：显示项目与在LCD上显示时有区别 */  
  DEBUG_INFO( COMPID_APP, SEVERITY_INFORMATION, 3, rxAccum,
                                              (uint16)msecs, (uint16)rxTotal );
#endif

  /* 若从上次显示/更新到现在接收和发送数据的数量都为0 */
  if ( (rxAccum == 0) && (txAccum == 0) )
  {
    /* 停止接收显示事件的触发 */
    osal_stop_timerEx( TransmitApp_TaskID, TRANSMITAPP_RCVTIMER_EVT );
    timerOn = FALSE;  // 设置用来触发接收显示事件的超时计时器状态变量为FALSE
  }

  /* 清零从上次显示/更新到现在接收/发送数据的数量 */
  rxAccum = txAccum = 0;
}


