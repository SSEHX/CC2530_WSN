/*******************************************************************************
 * 文件名称：pct_data.c
 * 功    能：PCT可编程通信温控器数据文件。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "ZDConfig.h"

#include "se.h"
#include "pct.h"
#include "zcl_general.h"
#include "zcl_se.h"
#include "zcl_key_establish.h"
/********************************************************************/


/* 常量定义 */
/********************************************************************/
#define PCT_DEVICE_VERSION      0
#define PCT_FLAGS               0

#define PCT_HWVERSION           1
#define PCT_ZCLVERSION          1

/*
  基础簇
 */
const uint8 pctZCLVersion = PCT_ZCLVERSION;
const uint8 pctHWVersion = PCT_HWVERSION;
const uint8 pctManufacturerName[] = { 16, 'S','I','K','A','I',' ','E','L','E','C','T','R','O','N','I','C' };
const uint8 pctModelId[] = { 16, 'S','K','0','0','0','1',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 pctDateCode[] = { 16, '2','0','1','0','0','3','0','8',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 pctPowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 pctLocationDescription[] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 pctPhysicalEnvironment = PHY_UNSPECIFIED_ENV;
uint8 pctDeviceEnabled = DEVICE_ENABLED;

/*
  识别簇属性
 */
uint16 pctIdentifyTime = 0;
uint32 pctTime = 0;
uint8 pctTimeStatus = 0x01;

/*
  响应需求及负载控制
 */
uint8 pctUtilityDefinedGroup = 0x00;
uint8 pctStartRandomizeMinutes = 0x00;
uint8 pctStopRandomizeMinutes = 0x00;
uint8 pctSignature[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

/*
  密钥建立
 */
uint16 pctKeyEstablishmentSuite = CERTIFICATE_BASED_KEY_ESTABLISHMENT;

/*
  簇定义 - 用户簇ID
 */
CONST zclAttrRec_t pctAttrs[PCT_MAX_ATTRIBUTES] =
{
  /* 通用基础簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // 簇ID - 在建立时被定义(比如：zcl.h)
    {  // 属性记录
      ATTRID_BASIC_ZCL_VERSION,           // 属性ID - 在簇库头文件中(比如：zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // 数据类型 - 在 zcl.h中
      ACCESS_CONTROL_READ,                // 存储控制变量 - 在 zcl.h中
      (void *)&pctZCLVersion              // 指向属性变量指针
    }
  },
  {                                       // 下同
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_HW_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&pctHWVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)pctManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)pctModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)pctDateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {   // 属性记录
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&pctPowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)pctLocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&pctPhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&pctDeviceEnabled
    }
  },

  /* 识别簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    {  // 属性记录
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&pctIdentifyTime
    }
  },

  /* 时间簇属性 */
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_TIME,
      ZCL_DATATYPE_UTC,
      ACCESS_CONTROL_READ,
      (void *)&pctTime
    }
  },
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_STATUS,
      ZCL_DATATYPE_BITMAP8,
      ACCESS_CONTROL_READ,
      (void *)&pctTimeStatus
    }
  },


  // SE属性
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    { // 属性记录
      ATTRID_SE_UTILITY_DEFINED_GROUP,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&pctUtilityDefinedGroup
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    { // 属性记录
      ATTRID_SE_START_RANDOMIZE_MINUTES,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&pctStartRandomizeMinutes
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_STOP_RANDOMIZE_MINUTES,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&pctStopRandomizeMinutes
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_KEY_ESTABLISHMENT,
    {  // 属性记录
      ATTRID_KEY_ESTABLISH_SUITE,
      ZCL_DATATYPE_BITMAP16,
      ACCESS_CONTROL_READ,
      (void *)&pctKeyEstablishmentSuite
    }
  },
};


/*
  簇选项定义
 */
zclOptionRec_t pctOptions[PCT_MAX_OPTIONS] =
{
  /* 通用簇选项 */
  {
    ZCL_CLUSTER_ID_GEN_TIME,                 // 簇ID - 在基础层中定义(比如：zcl.h)
    ( AF_EN_SECURITY /*| AF_ACK_REQUEST*/ ), // 选项 - 在 AF头文件中定义(比如：AF.h)
  },

  /* 智能能源簇选项 */
  // *** Smart Energy Cluster Options ***
  {
    ZCL_CLUSTER_ID_SE_PRICING,
    ( AF_EN_SECURITY ),
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    ( AF_EN_SECURITY ),
  },
};


/* 
  PCT可编程温控器最大输入簇数量
 */
#define PCT_MAX_INCLUSTERS       3
const cId_t pctInClusterList[PCT_MAX_INCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL
};

/* 
  PCT可编程温控器最大输出簇数量
 */
#define PCT_MAX_OUTCLUSTERS       3
const cId_t pctOutClusterList[PCT_MAX_OUTCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL
};

SimpleDescriptionFormat_t pctSimpleDesc =
{
  PCT_ENDPOINT,                   //  uint8 Endpoint;
  ZCL_SE_PROFILE_ID,              //  uint16 AppProfId[2];
  ZCL_SE_DEVICEID_PCT,            //  uint16 AppDeviceId[2];
  PCT_DEVICE_VERSION,             //  int   AppDevVer:4;
  PCT_FLAGS,                      //  int   AppFlags:4;
  PCT_MAX_INCLUSTERS,             //  uint8  AppNumInClusters;
  (cId_t *)pctInClusterList,      //  cId_t *pAppInClusterList;
  PCT_MAX_OUTCLUSTERS,            //  uint8  AppNumInClusters;
  (cId_t *)pctOutClusterList      //  cId_t *pAppInClusterList;
};

