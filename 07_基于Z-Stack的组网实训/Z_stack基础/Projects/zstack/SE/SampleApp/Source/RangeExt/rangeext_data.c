/*******************************************************************************
 * 文件名称：rangeext_data.c
 * 功    能：增距器数据文件。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ZDConfig.h"
#include "se.h"
#include "rangeext.h"
#include "zcl_general.h"
#include "zcl_key_establish.h"
/********************************************************************/


/* 常量定义 */
/********************************************************************/
#define RANGEEXT_DEVICE_VERSION      0
#define RANGEEXT_FLAGS               0

#define RANGEEXT_HWVERSION           1
#define RANGEEXT_ZCLVERSION          1

/*
  基础簇
 */
const uint8 rangeExtZCLVersion = RANGEEXT_ZCLVERSION;
const uint8 rangeExtHWVersion = RANGEEXT_HWVERSION;
const uint8 rangeExtManufacturerName[] = { 16, 'S','I','K','A','I',' ','E','L','E','C','T','R','O','N','I','C' };
const uint8 rangeExtModelId[] = { 16, 'S','K','0','0','0','1',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 rangeExtDateCode[] = { 16, '2','0','1','0','0','3','0','8',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 rangeExtPowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 rangeExtLocationDescription[] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 rangeExtPhysicalEnvironment = PHY_UNSPECIFIED_ENV;
uint8 rangeExtDeviceEnabled = DEVICE_ENABLED;

/*
  识别簇属性
 */
uint16 rangeExtIdentifyTime = 0;
uint32 rangeExtTime = 0;
uint8 rangeExtTimeStatus = 0x01;

/*
  密钥建立
 */
uint8 zclRangeExt_KeyEstablishmentSuite = CERTIFICATE_BASED_KEY_ESTABLISHMENT;

/*
  簇定义 - 用户簇ID
 */
CONST zclAttrRec_t rangeExtAttrs[RANGEEXT_MAX_ATTRIBUTES] =
{
  /* 通用基础簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // 簇ID - 在建立时被定义(比如：zcl.h)
    {  // 属性记录
      ATTRID_BASIC_ZCL_VERSION,           // 属性ID - 在簇库头文件中(比如：zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // 数据类型 - 在 zcl.h中
      ACCESS_CONTROL_READ,                // 存储控制变量 - 在 zcl.h中
      (void *)&rangeExtZCLVersion         // 指向属性变量指针
    }
  },
  {                                       // 下同
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_HW_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&rangeExtHWVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)rangeExtManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)rangeExtModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)rangeExtDateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {   // 属性记录
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&rangeExtPowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)rangeExtLocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&rangeExtPhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&rangeExtDeviceEnabled
    }
  },

  /* 识别簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    {  // 属性记录
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&rangeExtIdentifyTime
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
      (void *)&rangeExtTime
    }
  },
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_STATUS,
      ZCL_DATATYPE_BITMAP8,
      ACCESS_CONTROL_READ,
      (void *)&rangeExtTimeStatus
    }
  },

  // SE属性
  {
    ZCL_CLUSTER_ID_GEN_KEY_ESTABLISHMENT,
    {  // 属性记录
      ATTRID_KEY_ESTABLISH_SUITE,
      ZCL_DATATYPE_BITMAP16,
      ACCESS_CONTROL_READ,
      (void *)&zclRangeExt_KeyEstablishmentSuite
    }
  },
};

/*
  簇选项定义
 */
zclOptionRec_t rangeExtOptions[RANGEEXT_MAX_OPTIONS] =
{
  /* 通用簇选项 */
  {
    ZCL_CLUSTER_ID_GEN_TIME,                 // 簇ID - 在基础层中定义(比如：zcl.h)
    ( AF_EN_SECURITY /*| AF_ACK_REQUEST*/ ), // 选项 - 在 AF头文件中定义(比如：AF.h)
  },
};

/* 
  增距器最大输入簇数量
 */
#define RANGEEXT_MAX_INCLUSTERS       2
const cId_t rangeExtInClusterList[RANGEEXT_MAX_INCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY
};

/* 
  增距器最大输出簇数量
 */
#define RANGEEXT_MAX_OUTCLUSTERS      2
const cId_t rangeExtOutClusterList[RANGEEXT_MAX_OUTCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY
};

SimpleDescriptionFormat_t rangeExtSimpleDesc =
{
  RANGEEXT_ENDPOINT,                    //  uint8 Endpoint;
  ZCL_SE_PROFILE_ID,                    //  uint16 AppProfId[2];
  ZCL_SE_DEVICEID_RANGE_EXTENDER,       //  uint16 AppDeviceId[2];
  RANGEEXT_DEVICE_VERSION,              //  int   AppDevVer:4;
  RANGEEXT_FLAGS,                       //  int   AppFlags:4;
  RANGEEXT_MAX_INCLUSTERS,              //  uint8  AppNumInClusters;
  (cId_t *)rangeExtInClusterList,       //  cId_t *pAppInClusterList;
  RANGEEXT_MAX_OUTCLUSTERS,             //  uint8  AppNumInClusters;
  (cId_t *)rangeExtOutClusterList       //  cId_t *pAppInClusterList;
};


