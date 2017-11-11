/*******************************************************************************
 * 文件名称：loadcontrol_data.c
 * 功    能：负载控制器数据文件。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "ZDConfig.h"

#include "se.h"
#include "loadcontrol.h"
#include "zcl_general.h"
#include "zcl_se.h"
#include "zcl_key_establish.h"
/********************************************************************/


/* 常量定义 */
/********************************************************************/
#define LOADCONTROL_DEVICE_VERSION      0
#define LOADCONTROL_FLAGS               0

#define LOADCONTROL_HWVERSION           1
#define LOADCONTROL_ZCLVERSION          1

/*
  基础簇
 */
const uint8 loadControlZCLVersion = LOADCONTROL_ZCLVERSION;
const uint8 loadControlHWVersion = LOADCONTROL_HWVERSION;
const uint8 loadControlManufacturerName[] = { 16, 'S','I','K','A','I',' ','E','L','E','C','T','R','O','N','I','C' };
const uint8 loadControlModelId[] = { 16, 'S','K','0','0','0','1',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 loadControlDateCode[] = { 16, '2','0','1','0','0','3','0','8',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 loadControlPowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 loadControlLocationDescription[] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 loadControlPhysicalEnvironment = PHY_UNSPECIFIED_ENV;
uint8 loadControlDeviceEnabled = DEVICE_ENABLED;

/*
  识别簇属性
 */
uint16 loadControlIdentifyTime = 0;
uint32 loadControlTime = 0;
uint8 loadControlTimeStatus = 0x01;

/*
  响应需求及负载控制 
*/
uint8 loadControlUtilityDefinedGroup = 0x00;
uint8 loadControlStartRandomizeMinutes = 0x00;
uint8 loadControlStopRandomizeMinutes = 0x00;
uint8 loadControlSignature[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

/*
  密钥建立
 */
uint16 loadControlKeyEstablishmentSuite = CERTIFICATE_BASED_KEY_ESTABLISHMENT;

/*
  簇定义 - 用户簇ID
 */
CONST zclAttrRec_t loadControlAttrs[LOADCONTROL_MAX_ATTRIBUTES] =
{
  /* 通用基础簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // 簇ID - 在建立时被定义(比如：zcl.h)
    {  // 属性记录
      ATTRID_BASIC_ZCL_VERSION,           // 属性ID - 在簇库头文件中(比如：zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // 数据类型 - 在 zcl.h中
      ACCESS_CONTROL_READ,                // 存储控制变量 - 在 zcl.h中
      (void *)&loadControlZCLVersion      // 指向属性变量
    }
  },
  {                                       // 下同
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_HW_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&loadControlHWVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)loadControlManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)loadControlModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)loadControlDateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {   // 属性记录
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&loadControlPowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)loadControlLocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&loadControlPhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&loadControlDeviceEnabled
    }
  },

  /* 识别簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    {  // 属性记录
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&loadControlIdentifyTime
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
      (void *)&loadControlTime
    }
  },
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_STATUS,
      ZCL_DATATYPE_BITMAP8,
      ACCESS_CONTROL_READ,
      (void *)&loadControlTimeStatus
    }
  },


  /* SE 属性 */
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_UTILITY_DEFINED_GROUP,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&loadControlUtilityDefinedGroup
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_START_RANDOMIZE_MINUTES,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&loadControlStartRandomizeMinutes
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_STOP_RANDOMIZE_MINUTES,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&loadControlStopRandomizeMinutes
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_KEY_ESTABLISHMENT,
    {  // 属性记录
      ATTRID_KEY_ESTABLISH_SUITE,
      ZCL_DATATYPE_BITMAP16,
      ACCESS_CONTROL_READ,
      (void *)&loadControlKeyEstablishmentSuite
    }
  },
};


/*
  簇选项定义
 */
zclOptionRec_t loadControlOptions[LOADCONTROL_MAX_OPTIONS] =
{
  /* 通用簇选项 */
  {
    ZCL_CLUSTER_ID_GEN_TIME,                 // 簇ID - 在基础层中定义(比如：zcl.h)
    ( AF_EN_SECURITY /*| AF_ACK_REQUEST*/ ), // 选项 - 在 AF头文件中定义(比如：AF.h)
  },

  /* 智能能源簇选项 */
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    ( AF_EN_SECURITY ),
  },
};


/* 
  负载控制器最大输入簇数量
 */
#define LOADCONTROL_MAX_INCLUSTERS       3
const cId_t loadControlInClusterList[LOADCONTROL_MAX_INCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL
};

/* 
  负载控制器最大输出簇数量
 */
#define LOADCONTROL_MAX_OUTCLUSTERS       3
const cId_t loadControlOutClusterList[LOADCONTROL_MAX_OUTCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL
};

SimpleDescriptionFormat_t loadControlSimpleDesc =
{
  LOADCONTROL_ENDPOINT,                   //  uint8 Endpoint;
  ZCL_SE_PROFILE_ID,                      //  uint16 AppProfId[2];
  ZCL_SE_DEVICEID_LOAD_CTRL_EXTENSION,    //  uint16 AppDeviceId[2];
  LOADCONTROL_DEVICE_VERSION,             //  int   AppDevVer:4;
  LOADCONTROL_FLAGS,                      //  int   AppFlags:4;
  LOADCONTROL_MAX_INCLUSTERS,             //  uint8  AppNumInClusters;
  (cId_t *)loadControlInClusterList,      //  cId_t *pAppInClusterList;
  LOADCONTROL_MAX_OUTCLUSTERS,            //  uint8  AppNumInClusters;
  (cId_t *)loadControlOutClusterList      //  cId_t *pAppInClusterList;
};

