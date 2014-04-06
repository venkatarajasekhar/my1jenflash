/*----------------------------------------------------------------------------*/
#ifndef __MY1JENH__
#define __MY1JENH__
/*----------------------------------------------------------------------------*/
#include "my1comlib.h"
/*----------------------------------------------------------------------------*/
#define JEN_MSG_MINSIZE 2
#define JEN_MSG_MAXSIZE 255
#define JEN_MSG_MAXDATA JEN_MSG_MAXSIZE-1
/*----------------------------------------------------------------------------*/
/** 0x00 - 0x06 are reserved */
#define JEN_MSG_FLASH_ERASE_REQ 0x07
#define JEN_MSG_FLASH_ERASE_RES 0x08
#define JEN_MSG_FLASH_WRITE_REQ 0x09
#define JEN_MSG_FLASH_WRITE_RES 0x0A
#define JEN_MSG_FLASH_READ_REQ 0x0B
#define JEN_MSG_FLASH_READ_RES 0x0C
#define JEN_MSG_WRITE_SR_REQ 0x0F
#define JEN_MSG_WRITE_SR_RES 0x10
#define JEN_MSG_FLASH_ID_REQ 0x25
#define JEN_MSG_FLASH_ID_RES 0x26
#define JEN_MSG_FLASH_SELECT_REQ 0x2C
#define JEN_MSG_FLASH_SELECT_RES 0x2D
#define JEN_MSG_DEVICE_ID_REQ 0x32
#define JEN_MSG_DEVICE_ID_RES 0x33
/*----------------------------------------------------------------------------*/
#define JEN_MSG_STATUS_BYTE 0
/*----------------------------------------------------------------------------*/
/** response status code */
#define JEN_MSG_RESP_OK 0x00
#define JEN_MSG_RESP_NO_SUPPORT 0xFF
#define JEN_MSG_RESP_WRITE_FAIL 0xFE
#define JEN_MSG_RESP_INVALID_RES 0xFD
#define JEN_MSG_RESP_CRC_ERROR 0xFC
#define JEN_MSG_RESP_ASSERT_FAIL 0xFB
#define JEN_MSG_RESP_USER_INTR 0xFA
#define JEN_MSG_RESP_READ_FAIL 0xF9
#define JEN_MSG_RESP_TST_ERROR 0xF8
#define JEN_MSG_RESP_AUTH_ERROR 0xF7
#define JEN_MSG_RESP_NO_RESPONSE 0xF6
/*----------------------------------------------------------------------------*/
/** return codes */
#define JEN_MSG_OK 0
#define JEN_MSG_EWAIT_ABORT 1
#define JEN_MSG_ECHKSUM 2
#define JEN_MSG_ETYPE 3
#define JEN_MSG_ERESPONSE 4
#define JEN_MSG_EINVALID 5
#define JEN_MSG_EFOPEN 6
#define JEN_MSG_EPSIZE 7
#define JEN_MSG_OFF_DEVICE_INFO 20
#define JEN_MSG_OFF_FLASH_SELECT 40
#define JEN_MSG_OFF_WRITE_SR 60
#define JEN_MSG_OFF_FLASH_ERASE 80
#define JEN_MSG_OFF_FLASH_WRITE 100
#define JEN_MSG_OFF_FLASH_READ 120
#define JEN_MSG_OFF_FLASH_VERIFY 140
/*----------------------------------------------------------------------------*/
#define JEN_DEV_ID_CHARSIZE 32
#define JEN_DEV_ID_UNKOWN 0xFF
/*----------------------------------------------------------------------------*/
typedef struct _jen_flash_id_t
{
	unsigned char jen_id, man_id, dev_id;
	unsigned int dev_size;
	char name[JEN_DEV_ID_CHARSIZE];
}
jen_flash_id_t;
/*----------------------------------------------------------------------------*/
typedef struct _jen_dev_id_t
{
	unsigned int jen_id, jen_code_start;
	char name[JEN_DEV_ID_CHARSIZE];
}
jen_dev_id_t;
/*----------------------------------------------------------------------------*/
typedef struct _jen_msg_t
{
	unsigned char length; /* minimum is two? type + checksum */
	unsigned char type;
	unsigned char data[JEN_MSG_MAXSIZE-1];
	unsigned char checksum;
	/* checksum should be @ data[length-JEN_MSG_MINSIZE] */
}
jen_msg_t;
/*----------------------------------------------------------------------------*/
typedef struct _jen_dev_t
{
	unsigned char man_id, dev_id;
	unsigned int jen_id;
	char *pfilename;
	jen_flash_id_t *pflash;
	jen_dev_id_t *pdevice;
	jen_msg_t message;
}
ADeviceJEN_t;
/*----------------------------------------------------------------------------*/
int jen_device_init(ADeviceJEN_t* aDevice);
int jen_device_info(ASerialPort_t* aPort, ADeviceJEN_t* aDevice);
int jen_device_flash(ASerialPort_t* aPort, ADeviceJEN_t* aDevice);
int jen_device_verify(ASerialPort_t* aPort, ADeviceJEN_t* aDevice);
/*----------------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------------*/
