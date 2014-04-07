/*----------------------------------------------------------------------------*/
#include "my1jen.h"
#include "my1cons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
static jen_flash_id_t FLASH_DEVS[] = {
	{4,0x05,0x05,"ST M25P05-A"},
	{0,0x10,0x10,"ST M25P10-A"},
	{5,0x11,0x11,"ST M25P20-A"},
	{1,0xBF,0x49,"SST 25VF010A"},
	{2,0x1F,0x60,"Atmel 25F512"},
	{8,0xCC,0xEE,"Internal Flash (JN516x)"},
	{3,0x12,0x12,"ST M25P40"}
};
/*----------------------------------------------------------------------------*/
static int FLASH_DEVS_SIZE = sizeof(FLASH_DEVS)/sizeof(jen_flash_id_t);
/*----------------------------------------------------------------------------*/
static jen_dev_id_t JENNIC_DEVS[] = {
	{0x10804686,"JN5148(MINE)"},
	{0x10002000,"JN5139"},
	{0x00005686,"JN5142"},
	{0x10404686,"JN5148"}
};
/*----------------------------------------------------------------------------*/
static int JENNIC_DEVS_SIZE = sizeof(JENNIC_DEVS)/sizeof(jen_dev_id_t);
/*----------------------------------------------------------------------------*/
int jen_device_init(ADeviceJEN_t* aDevice)
{
	aDevice->man_id = 0xFF;
	aDevice->dev_id = 0xFF;
	aDevice->pflash = 0x0;
	aDevice->pdevice = 0x0;
	aDevice->pfilename = 0x0;
	return 0;
}
/*----------------------------------------------------------------------------*/
int jen_build_sum(jen_msg_t* msg)
{
	int loop;
	msg->checksum = 0;
	msg->checksum ^= msg->length;
	msg->checksum ^= msg->type;
	for(loop=0;loop<msg->length-JEN_MSG_MINSIZE;loop++)
		msg->checksum ^= msg->data[loop];
	msg->data[loop] = msg->checksum;
	return 0;
}
/*----------------------------------------------------------------------------*/
int jen_check_sum(jen_msg_t* msg)
{
	int loop;
	msg->checksum = 0;
	msg->checksum ^= msg->length;
	msg->checksum ^= msg->type;
	for(loop=0;loop<msg->length-JEN_MSG_MINSIZE;loop++)
		msg->checksum ^= msg->data[loop];
	return msg->checksum ^ msg->data[loop]; /** returns 0 if match! */
}
/*----------------------------------------------------------------------------*/
int jen_send_msg(ASerialPort_t* aPort, jen_msg_t* msg)
{
	int loop;
	flush_serial(aPort);
	put_byte_serial(aPort, msg->length);
	put_byte_serial(aPort, msg->type);
	for(loop=0;loop<msg->length-1;loop++)
		put_byte_serial(aPort, msg->data[loop]);
	return loop - msg->length;
}
/*----------------------------------------------------------------------------*/
int jen_read_msg(ASerialPort_t* aPort, jen_msg_t* msg)
{
	my1key_t key;
	int loop = 0, error = JEN_MSG_OK;
	unsigned char *pbuff = (unsigned char*) msg;
	while(!check_incoming(aPort));
	pbuff[loop++] = get_byte_serial(aPort); /** get length! */
	while(loop<=msg->length)
	{
		if(check_incoming(aPort))
			pbuff[loop++] = get_byte_serial(aPort);
		key = get_keyhit();
		if(key==KEY_ESCAPE)
		{
			error = JEN_MSG_EWAIT_ABORT;
			break;
		}
	}
	purge_serial(aPort);
	return error;
}
/*----------------------------------------------------------------------------*/
#define FLASH_ID_REQ_LEN JEN_MSG_MINSIZE+0
#define FLASH_ID_RES_LEN JEN_MSG_MINSIZE+3
/*----------------------------------------------------------------------------*/
int jen_flash_info(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK, loop;
	/* build request */
	aDevice->message.length = FLASH_ID_REQ_LEN;
	aDevice->message.type = JEN_MSG_FLASH_ID_REQ;
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_FLASH_ID_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	aDevice->man_id = aDevice->message.data[1];
	aDevice->dev_id = aDevice->message.data[2];
	aDevice->pflash = 0x0;
	/* also get jennic flash id info */
	for(loop=0;loop<FLASH_DEVS_SIZE;loop++)
	{
		if(FLASH_DEVS[loop].man_id==aDevice->man_id&&
			FLASH_DEVS[loop].dev_id==aDevice->dev_id)
		{
			aDevice->pflash = &FLASH_DEVS[loop];
			break;
		}
	}
	return error;
}
/*----------------------------------------------------------------------------*/
#define DEVICE_ID_REQ_LEN JEN_MSG_MINSIZE+0
#define DEVICE_ID_RES_LEN JEN_MSG_MINSIZE+5
/*----------------------------------------------------------------------------*/
int jen_jennic_info(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK, loop;
	unsigned char *pchip;
	/* build request */
	aDevice->message.length = DEVICE_ID_REQ_LEN;
	aDevice->message.type = JEN_MSG_DEVICE_ID_REQ;
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_DEVICE_ID_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	/** chip id is MSB first! */
	pchip = (unsigned char*) &aDevice->jen_id;
	pchip[3] = aDevice->message.data[1];
	pchip[2] = aDevice->message.data[2];
	pchip[1] = aDevice->message.data[3];
	pchip[0] = aDevice->message.data[4];
	aDevice->pdevice = 0x0;
	/* get jennic id info */
	for(loop=0;loop<JENNIC_DEVS_SIZE;loop++)
	{
		if(JENNIC_DEVS[loop].jen_id==aDevice->jen_id)
		{
			aDevice->pdevice = &JENNIC_DEVS[loop];
			break;
		}
	}
	return error;
}
/*----------------------------------------------------------------------------*/
int jen_device_info(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	/* get jennic device info */
	error = jen_jennic_info(aPort,aDevice);
	if(error) return error;
	/* get flash device info */
	error = jen_flash_info(aPort,aDevice);
	return error;
}
/*----------------------------------------------------------------------------*/
#define FLASH_SELECT_REQ_LEN JEN_MSG_MINSIZE+5
#define FLASH_SELECT_RES_LEN JEN_MSG_MINSIZE+1
/*----------------------------------------------------------------------------*/
int jen_select_flash(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	unsigned int *paddr;
	/* build request */
	aDevice->message.length = FLASH_SELECT_REQ_LEN;
	aDevice->message.type = JEN_MSG_FLASH_SELECT_REQ;
	aDevice->message.data[0] = aDevice->pflash->jen_id;
	paddr = (unsigned int*) &aDevice->message.data[1];
	*paddr = 0x00000000; /** set to zero */
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_FLASH_SELECT_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	return error;
}
/*----------------------------------------------------------------------------*/
#define WRITE_SR_REQ_LEN JEN_MSG_MINSIZE+1
#define WRITE_SR_RES_LEN JEN_MSG_MINSIZE+1
/*----------------------------------------------------------------------------*/
int jen_write_status(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	/* build request */
	aDevice->message.length = WRITE_SR_REQ_LEN;
	aDevice->message.type = JEN_MSG_WRITE_SR_REQ;
	aDevice->message.data[0] = 0x00;
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_WRITE_SR_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	return error;
}
/*----------------------------------------------------------------------------*/
#define FLASH_ERASE_REQ_LEN JEN_MSG_MINSIZE+0
#define FLASH_ERASE_RES_LEN JEN_MSG_MINSIZE+1
/*----------------------------------------------------------------------------*/
int jen_erase_flash(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	/* build request */
	aDevice->message.length = FLASH_ERASE_REQ_LEN;
	aDevice->message.type = JEN_MSG_FLASH_ERASE_REQ;
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_FLASH_ERASE_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	return error;
}
/*----------------------------------------------------------------------------*/
#define FLASH_WRITE_ADDR_SIZE 4
#define FLASH_WRITE_REQ_LEN JEN_MSG_MINSIZE+FLASH_WRITE_ADDR_SIZE
#define FLASH_WRITE_RES_LEN JEN_MSG_MINSIZE+1
/*----------------------------------------------------------------------------*/
int jen_write_flash(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	/* build request - length and data set externally! */
	aDevice->message.type = JEN_MSG_FLASH_WRITE_REQ;
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_FLASH_WRITE_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	return error;
}
/*----------------------------------------------------------------------------*/
#define MAX_PROG_BUFFER 128
/*----------------------------------------------------------------------------*/
int jen_device_flash(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	void *pbuff;
	FILE *pbinfile;
	int error = JEN_MSG_OK;
	unsigned int tellsize, tellthis = 0, buffsize, *paddr;
	/* get id */
	error = jen_device_info(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_DEVICE_INFO;
	/* select flash */
	error = jen_select_flash(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_FLASH_SELECT;
	/* set status reg? */
	error = jen_write_status(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_WRITE_SR;
	/* erase flash */
	error = jen_erase_flash(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_FLASH_ERASE;
	/* program flash */
	pbinfile = fopen(aDevice->pfilename,"rb");
	if(!pbinfile) return JEN_MSG_EFOPEN;
	/* get actual binary size */
	fseek(pbinfile, 0L, SEEK_END);
	tellsize = ftell(pbinfile);
	fseek(pbinfile, 0L, SEEK_SET);
	/** check device size */
	/*buffsize = aDevice->pflash->dev_size;
	if(tellsize>buffsize)
	{
		fclose(pbinfile);
		return JEN_MSG_EPSIZE;
	}*/
	/* prepare to write */
	pbuff = (void*) &aDevice->message.data[FLASH_WRITE_ADDR_SIZE];
	paddr = (unsigned int*) aDevice->message.data;
	while((buffsize=fread(pbuff,1,MAX_PROG_BUFFER,pbinfile))>0)
	{
		/* prepare message length here */
		aDevice->message.length = FLASH_WRITE_REQ_LEN + buffsize;
		/* prepare write address */
		*paddr = JEN_DEV_CODE_ADDR + tellthis;
		/* write flash */
		printf("%3d%%\b\b\b\b",tellthis*100/tellsize);
		error = jen_write_flash(aPort,aDevice);
		if(error) return error+JEN_MSG_OFF_FLASH_WRITE;
		if(feof(pbinfile)) break;
		tellthis += buffsize;
	}
	fclose(pbinfile);
	return error;
}
/*----------------------------------------------------------------------------*/
#define FLASH_READ_ADDR_SIZE 4
#define FLASH_READ_REQ_LEN JEN_MSG_MINSIZE+6
#define FLASH_READ_RES_LEN JEN_MSG_MINSIZE+1
/*----------------------------------------------------------------------------*/
int jen_read_flash(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	/* build request */
	aDevice->message.type = JEN_MSG_FLASH_READ_REQ;
	aDevice->message.length = FLASH_READ_REQ_LEN;
	jen_build_sum(&aDevice->message);
	/* send request */
	jen_send_msg(aPort,&aDevice->message);
	/* read response */
	error = jen_read_msg(aPort,&aDevice->message);
	if(error) return error;
	/* check response */
	error = jen_check_sum(&aDevice->message);
	if(error) return JEN_MSG_ECHKSUM;
	if(aDevice->message.type!=JEN_MSG_FLASH_READ_RES)
		return JEN_MSG_ETYPE;
	if(aDevice->message.data[JEN_MSG_STATUS_BYTE]!=JEN_MSG_RESP_OK)
		return JEN_MSG_ERESPONSE;
	return error;
}
/*----------------------------------------------------------------------------*/
#define MAX_READ_BUFFER 128
/*----------------------------------------------------------------------------*/
int jen_device_verify(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	void *pbuff;
	FILE *pbinfile;
	int error = JEN_MSG_OK, loop;
	unsigned int tellsize, tellthis = 0, buffsize, *paddr;
	unsigned short *pleng;
	/* buffer for read flash data? */
	unsigned char buff[MAX_READ_BUFFER];
	/* get id */
	error = jen_device_info(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_DEVICE_INFO;
	/* select flash */
	error = jen_select_flash(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_FLASH_SELECT;
	/* read flash */
	pbinfile = fopen(aDevice->pfilename,"rb");
	if(!pbinfile) return JEN_MSG_EFOPEN;
	/* get actual binary size */
	fseek(pbinfile, 0L, SEEK_END);
	tellsize = ftell(pbinfile);
	fseek(pbinfile, 0L, SEEK_SET);
	/** check device size */
	/*buffsize = aDevice->pflash->dev_size;
	if(tellsize>buffsize)
	{
		fclose(pbinfile);
		return JEN_MSG_EPSIZE;
	}*/
	/* prepare to read & verify */
	pbuff = (void*) buff;
	paddr = (unsigned int*) aDevice->message.data;
	pleng = (unsigned short*) &aDevice->message.data[FLASH_READ_ADDR_SIZE];
	while((buffsize=fread(pbuff,1,MAX_READ_BUFFER,pbinfile))>0)
	{
		/* prepare message address & length here */
		*paddr = JEN_DEV_CODE_ADDR + tellthis;
		*pleng = (unsigned short) buffsize;
		/* read flash */
		printf("%3d%%\b\b\b\b",tellthis*100/tellsize);
		error = jen_read_flash(aPort,aDevice);
		if(error) return error+JEN_MSG_OFF_FLASH_READ;
		if(feof(pbinfile)) break;
		/* verify data */
		for(loop=0;loop<(int)buffsize;loop++)
		{
			if(buff[loop]!=aDevice->message.data[loop+1])
				return error+JEN_MSG_OFF_FLASH_READ;
		}
		/* prepare next read address */
		tellthis += buffsize;
	}
	fclose(pbinfile);
	return error;
}
/*----------------------------------------------------------------------------*/
int jen_device_read_maclic(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK, loop;
	unsigned int *paddr;
	unsigned short *pleng;
	/* get id */
	error = jen_device_info(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_DEVICE_INFO;
	/* select flash */
	error = jen_select_flash(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_FLASH_SELECT;
	/* prepare pointers */
	paddr = (unsigned int*) aDevice->message.data;
	pleng = (unsigned short*) &aDevice->message.data[FLASH_READ_ADDR_SIZE];
	/* prepare message address & length here */
	*paddr = JEN_DEV_MAC_OSET;
	*pleng = JEN_DEV_MAC_SIZE;
	/* read mac */
	error = jen_read_flash(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_FLASH_READ;
	/* save mac */
	for(loop=0;loop<JEN_DEV_MAC_SIZE;loop++)
		aDevice->mac_id[loop] = aDevice->message.data[loop+1];
	/* prepare message address & length here */
	*paddr = JEN_DEV_LIC_OSET;
	*pleng = JEN_DEV_LIC_SIZE;
	/* read lic */
	error = jen_read_flash(aPort,aDevice);
	if(error) return error+JEN_MSG_OFF_FLASH_READ;
	/* save lic */
	for(loop=0;loop<JEN_DEV_LIC_SIZE;loop++)
		aDevice->lic_id[loop] = aDevice->message.data[loop+1];
	/* done */
	return error;
}
/*----------------------------------------------------------------------------*/
