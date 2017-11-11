/*******************************************************************************
 * 文件名称：se.h
 * 功    能：SE的初始化函数头文件。
 ******************************************************************************/


#ifndef SE_H
#define SE_H


#ifdef __cplusplus
extern "C"
{
#endif

/*
  Zigbee SE Profile识别符
 */
#define ZCL_SE_PROFILE_ID                                      0x0109

/*
  通用设备ID
 */
#define ZCL_SE_DEVICEID_RANGE_EXTENDER                         0x0008

/*
  SE设备ID
 */
#define ZCL_SE_DEVICEID_ESP                                    0x0500
#define ZCL_SE_DEVICEID_METER                                  0x0501
#define ZCL_SE_DEVICEID_IN_PREMISE_DISPLAY                     0x0502
#define ZCL_SE_DEVICEID_PCT                                    0x0503
#define ZCL_SE_DEVICEID_LOAD_CTRL_EXTENSION                    0x0504
#define ZCL_SE_DEVICEID_SMART_APPLIANCE                        0x0505
#define ZCL_SE_DEVICEID_PREPAY_TERMINAL                        0x0506


/*
  ZCL SE Profile 初始化函数
 */
extern void zclSE_Init( SimpleDescriptionFormat_t *simpleDesc );


#ifdef __cplusplus
}
#endif

#endif /* ZCL_SE_H */
  

