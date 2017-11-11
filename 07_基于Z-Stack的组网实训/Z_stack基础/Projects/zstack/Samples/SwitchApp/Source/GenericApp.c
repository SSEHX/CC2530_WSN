/*******************************************************************************
 * 文件名称：GenericApp.c
 * 功    能：GenericApp实验。
 * 实验内容：一个ZigBee网络中的某两个设备通过“终端设备绑定请求”或“匹配描述符
 *           请求”建立应用层的逻辑链接后，以5秒为周期互发"HelloZIGBEE"信息。
 * 实验设备：
 *           SW2： 发送“匹配描述符请求”
 *           SW1：发送“终端设备绑定请求”         
 *           LED_R(红色 D6)：当成功建立网络或加入网络后此LED点亮
 *           LEB_G(绿色 D5)：当与另一设备成功建立应用层的逻辑连接后此LED点亮
*           LED_B(蓝色 D4): 当发送"Hello ZIGBEE"信息时此LED取反
 *          LED_B(蓝色 D3)：：当接收到"Hello World"信息时此LED闪烁
 *           
 *           我们对两个设备在应用层建立逻辑链接做个具体说明，假设A和B是网络中的
 *           两个设备，我们希望在它们的应用层建立逻辑链接，可以采用下面两种方法：
 *           （1）“匹配描述符请求”
 *           按下设备A上的SW2键，开始在应用层建立逻辑链接，如果成功则设备A上的
 *           LEB_G(绿色 D5)点亮，这时我们就建立了单向的A->B的逻辑链接。此后，设备A
 *           以5秒为周期，向设备B发送"Hello ZIGBEE"信息。
 *           按下设备B上的SW2键，开始在应用层建立逻辑链接，如果成功则设备B上的
 *           LED_B(蓝色)点亮，这时我们就建立了单向的B->A的逻辑链接。此后，设备B
 *           以5秒为周期，向设备A发送"Hello ZIGBEE"信息。
 *　　　　　 经过前面两步的操作，设备A与设备B之间就建立好了应用层的逻辑链接，之
 *           后它们以5秒为周期互发"Hello ZIGBEE"信息。
 *           （2）“终端设备绑定请求”
 *            在APS_DEFAULT_MAXBINDING_TIME规定时间内（默认为16秒）分别按下设备
 *            A和设备B上的SW1键，将建立设备A与设备B之间的绑定，若成功则设备A和
 *            设备B上的LEB_G(绿色 D5)点亮。此时设备A与设备B之间就建立好了应用层的逻
 *            辑链接，之后它们以5秒为周期互发"Hello ZIGBEE"信息。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "GenericApp.h"
#include "DebugTrace.h"

#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
/********************************************************************/


/* 簇ID列表 */
/* 该列表应该用本应用专用的簇ID来填充 */
const cId_t GenericApp_ClusterList[GENERICAPP_MAX_CLUSTERS] =
{
  GENERICAPP_CLUSTERID
};


/* 设备的简单描述符 */
const SimpleDescriptionFormat_t GenericApp_SimpleDesc =
{
  GENERICAPP_ENDPOINT,              //  端点号
  GENERICAPP_PROFID,                //  Profile ID
  GENERICAPP_DEVICEID,              //  设备ID
  GENERICAPP_DEVICE_VERSION,        //  设备版本
  GENERICAPP_FLAGS,                 //  标识
  GENERICAPP_MAX_CLUSTERS,          //  输入簇的数量
  (cId_t *)GenericApp_ClusterList,  //  输入簇列表
  GENERICAPP_MAX_CLUSTERS,          //  输出簇的数量
  (cId_t *)GenericApp_ClusterList   //  输出簇列表
};


/* 设备的端点描述符 */
/* 我们在此处定义了设备的端点描述符，但是我们在GenericApp_Init()函数
   中对该描述符进行填充。
   我们也可以在此处用"const"关键字来定义并填充一个端点描述符的结构体
   常量。
   很明显，我们此处采用第一种方式，换句话说，我们在RAM里定义了端点描
   述符。
*/
endPointDesc_t GenericApp_epDesc;


/* 本地变量 */
/********************************************************************/

/*
   本应用的任务ID变量。当SampleApp_Init()函数被调用时，
   该变量可以获得任务ID值。
*/
byte GenericApp_TaskID;   


/*
   设备的网络状态变量：协调器/路由器/终端设备/设备不在网络中
*/
devStates_t GenericApp_NwkState;


/*
   传输ID（计数器）变量
*/
byte GenericApp_TransID;  


/*
   "Hello World"信息的目的地址结构体变量
*/
afAddrType_t GenericApp_DstAddr;
/********************************************************************/


/* 本地函数 */
/********************************************************************/
void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
void GenericApp_HandleKeys( byte shift, byte keys );
void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void GenericApp_SendTheMessage( void );
/********************************************************************/


/*********************************************************************
 * 函数名称：GenericApp_Init
 * 功    能：GenericApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_Init( byte task_id )
{
  GenericApp_TaskID = task_id;
  GenericApp_NwkState = DEV_INIT;
  GenericApp_TransID = 0;

  /* 
     可以在此处添加对设备硬件的初始化代码。 
     若硬件是本应用专用的，可以将对该硬件的初始化代码添加在此处。 
     若硬件不是本应用专用的，可以将初始化代码添加在Zmain.c文件中的main()函数。
  */

  /* 初始化"Hello World"信息的目的地址 */
  GenericApp_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  GenericApp_DstAddr.endPoint = 0;
  GenericApp_DstAddr.addr.shortAddr = 0;

  /* 填充端点描述符 */
  GenericApp_epDesc.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_epDesc.task_id = &GenericApp_TaskID;
  GenericApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&GenericApp_SimpleDesc;
  GenericApp_epDesc.latencyReq = noLatencyReqs;

  /* 注册端点描述符 */
  afRegister( &GenericApp_epDesc );

  /* 注册按键事件，将所有按键事件发送给本应用任务GenericApp_TaskID */
  RegisterForKeys( GenericApp_TaskID );

  /* 若包含了LCD_SUPPORTED编译选项，则在LCD上进行相应的显示 */
#if defined ( LCD_SUPPORTED )
#if defined ( ZIGBEEPRO )
    HalLcdWriteString( "GenericApp(ZigBeePRO)", HAL_LCD_LINE_2 );
#else
    HalLcdWriteString( "GenericApp(ZigBee07)", HAL_LCD_LINE_2 );
#endif
#endif
  
  /* ZDO信息注册 */
  /* 注册ZDO的簇End_Device_Bind_rsp，将收到的End_Device_Bind_rsp事件
     发送给本应用任务GenericApp_TaskID
   */
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, End_Device_Bind_rsp );
  
  /* ZDO信息注册 */
  /* 注册ZDO的簇Match_Desc_rsp，将收到的Match_Desc_rsp事件发送给本应
     用任务GenericApp_TaskID
   */  
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, Match_Desc_rsp );
}


/*********************************************************************
 * 函数名称：GenericApp_ProcessEvent
 * 功    能：GenericApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
UINT16 GenericApp_ProcessEvent( byte task_id, UINT16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  afDataConfirm_t *afDataConfirm;

  /* 定义用于保存AF数据确认信息各字段值的变量 */
  byte sentEP;
  ZStatus_t sentStatus;
  byte sentTransID;       
  (void)task_id;  // 防止编译器报错

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        /* ZDO信息输入事件 */
        case ZDO_CB_MSG:
          // 调用ZDO信息输入事件处理函数
          GenericApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;
        
        /* 按键事件 */
        case KEY_CHANGE:
          GenericApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, 
                                 ((keyChange_t *)MSGpkt)->keys );
          break;

        /* AF数据确认事件 */
        case AF_DATA_CONFIRM_CMD:
          // 读取AF数据确认信息并做相应处理
          afDataConfirm = (afDataConfirm_t *)MSGpkt;
          sentEP = afDataConfirm->endpoint;
          sentStatus = afDataConfirm->hdr.status;
          sentTransID = afDataConfirm->transID;
          (void)sentEP;
          (void)sentTransID;

          // 若AF数据确认信息表明刚才发送的数据不成功则做相应处理
          if ( sentStatus != ZSuccess )
          {
            // 用户可在此处添加自己的处理代码
          }
          break;

        /* AF输入信息事件 */ 
        case AF_INCOMING_MSG_CMD:
          GenericApp_MessageMSGCB( MSGpkt );  // 调用输入信息事件处理函数
          break;

        /* ZDO状态改变事件 */  
        case ZDO_STATE_CHANGE:
          GenericApp_NwkState = (devStates_t)(MSGpkt->hdr.status);// 读取设备状态
          /* 若设备是协调器或路由器或终端  */
          if ( (GenericApp_NwkState == DEV_ZB_COORD)
              || (GenericApp_NwkState == DEV_ROUTER)
              || (GenericApp_NwkState == DEV_END_DEVICE) )
          {
            /* 触发发送"Hello World"信息的事件GENERICAPP_SEND_MSG_EVT */
            osal_start_timerEx( GenericApp_TaskID,
                                GENERICAPP_SEND_MSG_EVT,
                                GENERICAPP_SEND_MSG_TIMEOUT );
          }
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );  // 释放存储器

      // 获取下一个系统消息事件
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    }

    return (events ^ SYS_EVENT_MSG);  // 返回未处理的事件
  }

  /* 发送"Hello World"信息的事件GENERICAPP_SEND_MSG_EVT */
  /* 该事件在前面的"case ZDO_STATE_CHANGE:"代码部分可能被触发 */
  if ( events & GENERICAPP_SEND_MSG_EVT )
  {
#if (LOGIL_TYPE!=0)
      GenericApp_SendTheMessage();  // 发送"Hello World"信息
#endif
    /* 再次触发发送"Hello World"信息的事件GENERICAPP_SEND_MSG_EVT */
    osal_start_timerEx( GenericApp_TaskID,
                        GENERICAPP_SEND_MSG_EVT,
                        GENERICAPP_SEND_MSG_TIMEOUT );

    return (events ^ GENERICAPP_SEND_MSG_EVT);  // 返回未处理的事件
  }
  
  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：GenericApp_ProcessZDOMsgs
 * 功    能：GenericApp的ZDO信息输入事件处理函数。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  /* ZDO信息中的簇ID */
  switch ( inMsg->clusterID )
  {
    /* ZDO的簇End_Device_Bind_rsp */
    case End_Device_Bind_rsp:
      /* 若绑定成功 */
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
      
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
      {
        /* 获取指向响应信息的指针 */
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        /* 若指向响应信息的指针变量pRsp的值不为空(NULL) */
        if ( pRsp )
        { /* 指向响应信息的指针变量pRsp的值有效 */
          if ( pRsp->status == ZSuccess && pRsp->cnt )
          {
            /* 设置"Hello World"信息的目的地址 */
            GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            GenericApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            
            // 采用第一个端点，用户可以修改此处代码以搜索所有端点
            GenericApp_DstAddr.endPoint = pRsp->epList[0];

            /* 点亮LED4 */
            
            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
          }
          osal_mem_free( pRsp );  // 释放指针变量pRsp所占用的存储器
        }
      }
      break;
  }
}


/*********************************************************************
 * 函数名称：GenericApp_HandleKeys
 * 功    能：GenericApp的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的项目为：
 *                  HAL_KEY_SW_2     SK-SmartRF05EB的RIGHT键
 *                  HAL_KEY_SW_4     SK-SmartRF05EB的LEFT键
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_HandleKeys( byte shift, byte keys )
{
  zAddrType_t dstAddr;
  (void)shift;
  
  /* HAL_KEY_SW_1被按下，发送“终端设备绑定请求” */
  
  if ( keys & HAL_KEY_SW_6)
  {
#if defined (KEY_TO_UART)
      HalUARTWrite(HAL_UART_PORT_0,"KEY1 PRESS!\r\n",13);
#endif          
      
#if (LOGIL_TYPE==0)    
    HalLedSet ( HAL_LED_1, HAL_LED_MODE_TOGGLE );
#else
    /* 为强制端点发送一个“终端设备绑定请求”（单播）*/
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
    dstAddr.addrMode = Addr16Bit;
    dstAddr.addr.shortAddr = 0x0000; // 目的地址为网络中协调器的16位短地址0x0000
    ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(), 
                            GenericApp_epDesc.endPoint,
                            GENERICAPP_PROFID,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            FALSE );
#endif    
  }

  /* HAL_KEY_SW_2被按下，发送“匹配描述符请求” */
   
  if ( keys &HAL_KEY_SW_7 )
  {
#if defined  (KEY_TO_UART)
      HalUARTWrite(HAL_UART_PORT_0,"KEY2 PRESS!\r\n",13);
#endif   
     
      
  #if (LOGIL_TYPE==0)    
    HalLedSet ( HAL_LED_2, HAL_LED_MODE_TOGGLE );
#else

    /* 发送一个“匹配描述符请求”（广播）*/
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
    dstAddr.addrMode = AddrBroadcast;
    dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
    ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
#endif     
  }
}


/*********************************************************************
 * 函数名称：GenericApp_MessageMSGCB
 * 功    能：GenericApp的输入信息事件处理函数。本函数处理来自其他设备的
 *           任何输入数据。因此，可基于簇ID执行预定的动作。
 * 入口参数：pkt  指向输入信息的指针
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  /* AF输入信息中的簇 */
  switch ( pkt->clusterId )
  {
    /* 簇GENERICAPP_CLUSTERID */
    case GENERICAPP_CLUSTERID:
   
      /* 熄灭LED1 */
      /* 对于SK-SmartRF05EB而言，LED1是LED_G（绿色） */
      HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );      
      
      /* 闪烁LED1 */
      HalLedBlink( HAL_LED_1, 2, 50, 250 ); 
   /* 则UART上显示接收到的信息 */    
      HalUARTWrite(HAL_UART_PORT_0,pkt->cmd.Data, osal_strlen(pkt->cmd.Data));
    /* 若包含了LCD_SUPPORTED编译选项，则在LCD上显示接收到的信息 */      
#if defined( LCD_SUPPORTED )
      HalLcdWriteString( (char*)pkt->cmd.Data, HAL_LCD_LINE_4 );
      HalLcdWriteString( "rcvd", HAL_LCD_LINE_5 );
/* 若包含了WIN32编译选项，则在IAR调试环境下打印出接收到的信息 */
#elif defined( WIN32 )
      WPRINTSTR( pkt->cmd.Data );
#endif
    break;
  }
}


/*********************************************************************
 * 函数名称：GenericApp_SendTheMessage
 * 功    能：发送"Hello World"信息。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_SendTheMessage( void )
{
  /* 要发送的应用数据"Hello World"*/
  char theMessageData[] = "Hello Zigbee!\r\n";

  /* 发送"Hello World"信息 */
  if ( AF_DataRequest( &GenericApp_DstAddr, &GenericApp_epDesc,
                       GENERICAPP_CLUSTERID,
                       (byte)osal_strlen( theMessageData ) + 1,
                       (byte *)&theMessageData,
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE, 
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
    // 发送请求执行成功，用户可添加自己相应的处理代码
    
    /* 熄灭LED2 */
    
    HalLedSet ( HAL_LED_2, HAL_LED_MODE_TOGGLE );      
   
   // HalLedBlink( HAL_LED_2, 2, 50, 250 );   
  }
  else
  {
        HalLedBlink( HAL_LED_2, 0, 50, 250 );
      // 发送请求执行失败，用户可添加自己相应的处理代码
  }
}

