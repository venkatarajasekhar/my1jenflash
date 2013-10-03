/*----------------------------------------------------------------------------*/
#include "my1jen.h"
#include "my1comlib.h"
#include "my1cons.h"
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define PROGNAME "my1jenflash"
/*----------------------------------------------------------------------------*/
#define COMMAND_NONE 0
#define COMMAND_SCAN 1
#define COMMAND_DEVICE 2
#define COMMAND_WRITE 3
#define COMMAND_VERIFY 4
/*----------------------------------------------------------------------------*/
#define ERROR_GENERAL -1
#define ERROR_USER_ABORT -2
#define ERROR_PARAM_PORT -3
#define ERROR_PORT_INIT -4
#define ERROR_PORT_OPEN -5
#define ERROR_PARAM_FILE -6
#define ERROR_MULTI_CMD -7
#define ERROR_NO_COMMAND -8
#define ERROR_NO_PGMFILE -9
#define ERROR_NO_DEVICE -10
/*----------------------------------------------------------------------------*/
#define FILENAME_LEN 80
/*----------------------------------------------------------------------------*/
void about(void)
{
	printf("Use:\n  %s [options] [command]\n",PROGNAME);
	printf("Options are:\n");
	printf("  --help      : show this message. overrides other options.\n");
	printf("  --port []   : specify port number between 1-%d.\n",MAX_COM_PORT);
	printf("  --file []   : specify programming file.\n");
	printf("  --tty []    : specify name for device (useful on Linux)\n");
	printf("  --device [] : specify Jennic device (not used for now).\n");
	printf("Commands are:\n");
	printf("  scan   : Scan for available serial port.\n");
	printf("  info   : Display device information.\n");
	printf("  write  : Write BIN file to device.\n");
	printf("\n");
}
/*----------------------------------------------------------------------------*/
void print_portscan(ASerialPort_t* aPort)
{
	int test, cCount = 0;
	printf("--------------------\n");
	printf("COM Port Scan Result\n");
	printf("--------------------\n");
	for(test=1;test<=MAX_COM_PORT;test++)
	{
		if(check_serial(aPort,test))
		{
			printf("%s%d: ",aPort->mPortName,COM_PORT(test));
			cCount++;
			printf("Ready.\n");
		}
	}
	printf("\nDetected Port(s): %d\n\n",cCount);
}
/*----------------------------------------------------------------------------*/
void show_device(ADeviceJEN_t* aDevice)
{
	printf("--------------------\n");
	printf("|Device Information|\n");
	printf("--------------------\n");
	printf("Jennic Device: ");
	if(!aDevice->pdevice)
		printf("Unknown! (CHIPID:%04X)\n",aDevice->jen_id);
	else
		printf("%s\n",aDevice->pdevice->name);
	printf("Flash Device: ");
	if(!aDevice->pflash)
		printf("Unknown! (MFCID:%02X)(DEVID: %02X)\n",
			aDevice->man_id,aDevice->dev_id);
	else
		printf("%s (%d)\n",aDevice->pflash->name,aDevice->pflash->jen_id);
	printf("------------------\n");
	printf("|Information Ends|\n");
	printf("------------------\n");
}
/*----------------------------------------------------------------------------*/
int info_device(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	printf("Getting device info... (hit <ESC> to cancel)");
	error = jen_device_info(aPort,aDevice);
	printf("\r                                            \r");
	if(error)
		printf("[ERROR] Cannot get device info? (%d)\n",error);
	else
		show_device(aDevice);
	return error;
}
/*----------------------------------------------------------------------------*/
int write_device(ASerialPort_t* aPort, ADeviceJEN_t* aDevice)
{
	int error = JEN_MSG_OK;
	if(!aDevice->pfilename)
	{
		printf("No programming file selected!\n");
		return ERROR_NO_PGMFILE;
	}
	printf("Writing programing file '%s'... ",aDevice->pfilename);
	fflush(stdout);
	error = jen_device_flash(aPort, aDevice);
	if(!error) printf("Done!\n");
	else printf("\nError programming device! (%d)\n",error);
	return error;
}
/*----------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	ASerialPort_t cPort;
	ASerialConf_t cPortConf;
	ADeviceJEN_t cDevice;
	int loop, test, port=1;
	int do_command = COMMAND_NONE;
	char *pfile = 0x0, *pdev = 0x0, *ptty = 0x0;

	/* print tool info */
	printf("\n%s - Jennic Flash Tool (version %s)\n",PROGNAME,PROGVERS);
	printf("  => by azman@my1matrix.net\n\n");

	if(argc>1)
	{
		for(loop=1;loop<argc;loop++)
		{
			if(!strcmp(argv[loop],"--help")||!strcmp(argv[loop],"-h"))
			{
				about();
				return 0;
			}
			else if(!strcmp(argv[loop],"--port"))
			{
				if(get_param_int(argc,argv,&loop,&test)<0)
				{
					printf("Cannot get port number!\n");
					return ERROR_PARAM_PORT;
				}
				else if(test<1||test>MAX_COM_PORT)
				{
					printf("Invalid port number! (%d)\n", test);
					return ERROR_PARAM_PORT;
				}
				port = test;
			}
			else if(!strcmp(argv[loop],"--tty"))
			{
				if(!(ptty=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting tty name!\n");
					continue;
				}
			}
			else if(!strcmp(argv[loop],"--file"))
			{
				if(!(pfile=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting filename!\n");
					return ERROR_PARAM_FILE;
				}
			}
			else if(!strcmp(argv[loop],"--device"))
			{
				if(!(pdev=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting device name!\n");
					continue;
				}
			}
			else if(!strcmp(argv[loop],"scan"))
			{
				if(do_command!=COMMAND_NONE)
				{
					printf("Multiple commands '%s'!(%d)\n",argv[loop],do_command);
					return ERROR_MULTI_CMD;
				}
				do_command = COMMAND_SCAN;
			}
			else if(!strcmp(argv[loop],"info"))
			{
				if(do_command!=COMMAND_NONE)
				{
					printf("Multiple commands '%s'!(%d)\n",argv[loop],do_command);
					return ERROR_MULTI_CMD;
				}
				do_command = COMMAND_DEVICE;
			}
			else if(!strcmp(argv[loop],"write"))
			{
				if(do_command!=COMMAND_NONE)
				{
					printf("Multiple commands '%s'!(%d)\n",argv[loop],do_command);
					return ERROR_MULTI_CMD;
				}
				do_command = COMMAND_WRITE;
			}
			else
			{
				printf("Unknown param '%s'!\n",argv[loop]);
			}
		}
	}

	/** initialize port */
	initialize_serial(&cPort);
#ifndef DO_MINGW
	sprintf(cPort.mPortName,"/dev/ttyUSB"); /* default on linux? */
#endif
	/** check user requested name change */
	if(ptty) sprintf(cPort.mPortName,ptty);
	get_serialconfig(&cPort,&cPortConf);
	cPortConf.mBaudRate = MY1BAUD38400; /* jennic uses this baudrate */
	set_serialconfig(&cPort,&cPortConf);

	/** check if user requested a port scan */
	if(do_command==COMMAND_SCAN)
	{
		print_portscan(&cPort);
		return 0;
	}

	/** try to prepare port with requested terminal */
	if(!port) port = find_serial(&cPort,0x0);
	if(!set_serial(&cPort,port))
	{
		about();
		print_portscan(&cPort);
		printf("Cannot prepare port '%s%d'!\n\n",cPort.mPortName,
			COM_PORT(port));
		return ERROR_PORT_INIT;
	}

	/** try to open port */
	if(!open_serial(&cPort))
	{
		printf("\nCannot open port '%s%d'!\n\n",cPort.mPortName,
			COM_PORT(cPort.mPortIndex));
		return ERROR_PORT_OPEN;
	}

	/** inform user about port in use */
	printf("Using Port: '%s%d'\n\n",cPort.mPortName,
		COM_PORT(cPort.mPortIndex));

	/** try to execute given command */
	switch(do_command)
	{
		case COMMAND_DEVICE:
			/** init device structure? */
			jen_device_init(&cDevice);
			/* get device info */
			info_device(&cPort,&cDevice);
			break;
		case COMMAND_WRITE:
			/** init device structure? */
			jen_device_init(&cDevice);
			/** check filename! */
			if(pfile) cDevice.pfilename = pfile;
			else
			{
				printf("No filename given! Aborting!\n");
				break;
			}
			/* program device */
			write_device(&cPort,&cDevice);
			break;
		case COMMAND_NONE:
			printf("No command given! Exiting!\n");
			break;
		default:
			printf("Invalid command! Aborting!\n");
	}
	printf("\n");

	/** close terminal */
	close_serial(&cPort);

	return 0;
}
/*----------------------------------------------------------------------------*/
