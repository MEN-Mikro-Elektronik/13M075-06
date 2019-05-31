/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         \file m75_test.c
 *
 *       \author Christian.Schuster@men.de
 *
 *  Description: several tests for M75 hardware and driver
 *
 *     Required: -
 *     Switches: -
 *
 *---------------------------------------------------------------------------
 * Copyright (c) 2004-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/mdis_err.h>

#include <MEN/m75_drv.h>

/* #include <MEN/dbg.h> */

/*-----------------------------------------+
|  TYPEDEFS                                |
+------------------------------------------*/
/* none */

/*-----------------------------------------+
|  DEFINES & CONST                         |
+------------------------------------------*/

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

#define TEST_MAXFRAMES	500		/* Maximum number of packets for M75_test_q */
#define EXT_FIFO_SIZE	2048	/* Size of external FIFO on M75 HW */
#define DEFAULT_FILE	"m75_data.txt"
#define DEFAULT_BRGEN_TCONST 0x01 /* default timeconstant for BR Gen */
static const int32 OFF = 0;
static const int32 ON  = 1;

static const int32 T_OK = 0;
static const int32 T_ERROR = 1;

/* SCC register defaults */
#define WR01_TEST_DEFAULT		0x18	/* SCC Interrupt, Wait/Request Modes */
#define WR02_TEST_DEFAULT		0x00	/* Interrupt Vector */
#define WR03_TEST_DEFAULT		0xC8	/* D8 Rx Control and Parameters */
#define WR04_TEST_DEFAULT		0x20	/* Rx and Tx Control */
#define WR05_TEST_DEFAULT		0x61	/* Tx Control */
#define WR06_TEST_DEFAULT		0x00	/* SDLC Address */
#define WR07_TEST_DEFAULT		0x7E	/* WR7: SDLC Flag */
#define WR7P_TEST_DEFAULT		0x5E	/* WR7': Extended Feature & FIFO Control */
#define WR09_TEST_DEFAULT		0x02	/* Master Interrupt Control and Reset */
#define WR10_TEST_DEFAULT		0x80	/* Rx and Tx miscellaneous Control Bits*/
#define WR11_TEST_DEFAULT		0x16	/* Clock Mode */
#define WR12_TEST_DEFAULT		0x71	/* BR Generator Time Constant, Lower Byte */
#define WR13_TEST_DEFAULT		0x00	/* BR Generator Time Constant, Upper Byte */
#define WR14_TEST_DEFAULT		0x07	/* Miscellaneous Control */
#define WR15_TEST_DEFAULT		0x05	/* External Status/Source Control */

/*-----------------------------------------+
|  GLOBALS                                 |
+------------------------------------------*/
static int32 G_sigUosCnt[2];	/* signal counters */
static int8 G_endMe;
/*-----------------------------------------+
|  STATICS                                 |
+------------------------------------------*/

/*-----------------------------------------+
|  PROTOTYPES                              |
+------------------------------------------*/
static void errShow( void );
static void usage(void);

static int32 m75_openclose( char *devName );
static int32 m75_test_setgetstat(MDIS_PATH fdHdl, u_int8 verbose);
static int32 m75_test_TxQ(MDIS_PATH fdHdl, int32 numpak);
static int32 m75_test_Tx(MDIS_PATH fdHdl, int32 numpak,
						 int32 tconst, int32 tout, u_int8 ch,
						 char *filename, u_int8 verbose);
static int32 m75_test_Rx(MDIS_PATH fdHdl,
						 int32  tconst, int32 tout, u_int8 useSig,
						 u_int8 ch, char *filename, u_int8 verbose);
static int32 m75_getFrame(MDIS_PATH fdHdl, FILE *fileH,
						  int32 *nbrOfBytesReceived, u_int8 verbose);




/******************************* errShow ************************************
 *
 *  Description:  Show MDIS or OS error message.
 *
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  -
 *  Globals....:  errno
 *
 ****************************************************************************/
static void errShow( void )
{
   u_int32 error;

   error = UOS_ErrnoGet();

   printf("*** %s ***\n",M_errstring( error ) );
}

/********************************* usage ************************************
 *
 *  Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("\n\nUsage: m75_test [<opts>] <device> [<opts>]\n");
	printf("Function: M75 hardware access\n");
	printf("Options:\n");
	printf("    device          device name (M75)               [M75_1]\n");
	printf("    -n=<n>(hex)     nr of frames for test           [%d]\n",
																TEST_MAXFRAMES);
	printf("    -c=<ch>         channel (1=A,2=B)               [1] \n");
	printf("    -s              flag, test set/getstats         [no]\n");
	printf("    -q              flag, test TxQ                  [no]\n");
	printf("    -b=<tc>(hex)    BR Gen timeconstant             [0]\n");
	printf("    -t              flag, test Tx                   [no]\n");
	printf("    -r              flag, test Rx                   [no]\n");
	printf("                    if file .\\conf.rx exists,\n"
		   "                    the parameters from this file are used\n");
	printf("    -m              flag, use Signals for Rx        [no]\n");
	printf("                    once started, press any key to abort\n");
	printf("    -w=<tout>       SET-, GETBLOCK_TOUT(-1 or msec) [0] \n");
	printf("    -f[=<filename>] optional, file with/for Data    [no]\n");
	printf("    -o              flag, extra open/close\n"              );
	printf("                    and Revision & ID tests         [no]\n");
	printf("    -v              verbose                         [no]\n");
	printf("\n");
	printf("Copyright (c) 2004-2019, MEN Mikro Elektronik GmbH\n%s\n\n",IdentString);
}

/******************************** main **************************************
 *
 *  Description:  main() - function
 *
 *---------------------------------------------------------------------------
 *  Input......:  argc      number of arguments
 *                *argv     pointer to arguments
 *      arguments:          description                 default
 *     -----------------------------------------------------------
 *      device          device name (M75)               [M75_1]
 *	    -n=<n>(hex)     nr of frames for test           [TEST_MAXFRAMES]
 *	    -c=<ch>         channel (1=A,2=B)               [1]
  *	    -s              flag, test set/getstats         [no]
 *	    -q              flag, test TxQ                  [no]
 *	    -b=<tc>(hex)    BR Gen timeconstant             [0]
 *	    -t              flag, test Tx                   [no]
 *	    -r              flag, test Rx                   [no]
 *	                    if file .\conf.rx exists,
 *	                    the parameters from this file are used
 *	    -m              flag, use Signals for Rx        [no]
 *	                    once started, press any key to abort
 *	    -w=<tout>       SET-, GETBLOCK_TOUT(-1 or msec) [0]
 *	    -f[=<filename>] optional, file with/for Data    [no]
 *                      if -f is specified without filename,
 *                      m75_data.txt is assumed
 *	    -o              flag, extra open/close\n"
 *	                    and Revision & ID tests         [no]
 *	    -v              verbose                         [no]
 *
 *  Output.....:  return    0   if no error
 *                          1   if error
 *
 *  Globals....:  -
 ****************************************************************************/
 int main( int argc, char *argv[ ] )
 {
	char *device, *str;
	int32	n, numframes, wait, tconst;
	u_int8	verbose,
			openclose, setget,
			txq, tx, rx,
			init, ch,
			useSig, useFile;
    MDIS_PATH fd = 0;
	int32	open=0, error=0, tested=0;
	char	*filename, *errstr;
/*	int32  path, ch, val;
	int32  retCode, loop;
	int32  timeStart, timeStop, timeDiff;
	u_int8 dataBuf[100];
*/
    if( argc < 2){
        usage();
        return 1;
    }

    printf("=========================\n");
    printf("%s", IdentString );
    printf("=========================\n");
    printf("welcome to the world of magic\n");

	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		device = "M75_1";
		/* usage();
		 * return(1);*/
	}

	numframes = ((str = UTL_TSTOPT("n=")) ?
						strtol(str, &errstr, 16) : 0);
	if(numframes > TEST_MAXFRAMES) {
		usage();
		return(1);
	}

	if( (useFile = (UTL_TSTOPT("f") ? 1 : 0)) )
		filename = (str = UTL_TSTOPT("f=")) ? str : DEFAULT_FILE;
	else
		filename = NULL;

	ch = ((str = UTL_TSTOPT("c=")) ? atoi(str) : 1);
	if( (ch > 3) || (ch < 1) ){
		usage();
		return(1);
	}
	wait   = ((str = UTL_TSTOPT("w=")) ? atoi(str) : 0);
	if( wait < -1 ) {
		usage();
		return(1);
	}

	tconst = ((str = UTL_TSTOPT("b=")) ? strtol(str, &errstr, 16) : 0);
	if( tconst < 0 ) {
		usage();
		return(1);
	}

	openclose = (UTL_TSTOPT("o") ? 1 : 0);
	setget    = (UTL_TSTOPT("s") ? 1 : 0);
	txq       = (UTL_TSTOPT("q") ? 1 : 0);
	tx        = (UTL_TSTOPT("t") ? 1 : 0);
	rx        = (UTL_TSTOPT("r") ? 1 : 0);
	useSig    = (UTL_TSTOPT("m") ? 1 : 0);
	init      = (UTL_TSTOPT("i") ? 1 : 0);
	verbose	  = (UTL_TSTOPT("v") ? 1 : 0);

    printf("\nm75_test channel=%s \tTimeConst=0x%04X \tframes=%d \twait=%d\n"
		   "         openclose=%s \tgetsetstat=%s    \ttxq=%s \n"
		   "         tx=%s        \trx=%s            \tverbose=%s\n\n\n",
		ch==1 ? "A" : ch==2 ? "B" : "AB",
		tconst, numframes, wait,
		openclose ? "yes" : "no",
		setget ? "yes" : "no",
		txq ? "yes" : "no",
		tx  ? "yes" : "no",
		rx  ? "yes" : "no",
		verbose ? "yes" : "no" );

	if( !(openclose | setget| txq | tx | rx) ) {
		printf("please specify a test\n");
		usage();
		return( 1 );
	}

	/* perform open/close and Revision & ID test */
	if(openclose){
		if( verbose )
			printf("=========================\n");
		m75_openclose( device );
		tested++;
	}

	/* open device for further testing */
	if( setget | txq | tx | rx ){
		if( verbose )
			printf("    M_open(%s)\n", device);
		if( (fd = M_open( device )) < 0 )
			goto M75_ERR;
		open = 1;
	}


	/* test all set/getstats */
	if(setget){
		if( verbose )
			printf("=========================\n");
		if(	(error = m75_test_setgetstat( fd, verbose )) )
			printf("\n    ==> ERROR m75_test_setgetstat\n");
		else
			printf("\n    M75_TEST_SET/GETSTAT ==> OK\n");
	}

	/* test txq */
	if(txq){
		if( verbose )
			printf("=========================\n");
		if(	(error = m75_test_TxQ( fd, numframes )) )
			printf("    ==> ERROR m75_test_TxQ: %d\n", error);
	}

	/* test Tx */
	if( tx && (ch & 0x1) ){
		if( verbose )
			printf("=========================\n");
		if(	(error = m75_test_Tx( fd, numframes, tconst, wait,
								 0, filename, verbose )) )
			printf("    ==> ERROR m75_test_Tx channel A\n");
	}

	if( tx && (ch & 0x2) ){
		if( verbose )
			printf("=========================\n");
		if(	(error = m75_test_Tx( fd, numframes, tconst, wait,
								 1, filename, verbose )) )
			printf("    ==> ERROR m75_test_Tx channel B\n");
	}

	/* test Rx */
	if( rx && (ch & 0x1) ){
		if( verbose )
			printf("=========================\n");
		if(	(error = m75_test_Rx( fd, tconst, wait,
								 useSig, 0, filename, verbose )) )
			printf("    ==> ERROR m75_test_Rx channel A\n");
	}

	if( rx && (ch & 0x2) ){
		if( verbose )
			printf("=========================\n");
		if(	(error = m75_test_Rx( fd, tconst, wait,
								 useSig, 1, filename, verbose )) )
			printf("    ==> ERROR m75_test_Rx channel B\n");
	}

	/* close device */
	if( open ){
		if( verbose )
			printf("\n=========================\n");
		if( M_close( fd ) ) goto M75_ERR;
		if( verbose )
			printf("    M_close\n");
	}
    return 0;

M75_ERR:
    errShow();
    printf("    => Error\n");
    M_close( fd );
    return( 1 );
}

static void __MAPILIB SigHandler( u_int32 sigCode )
{
	switch( sigCode ){
	case UOS_SIG_USR1:
		G_sigUosCnt[0]++;
		break;
	case UOS_SIG_USR2:
		G_sigUosCnt[1]++;
		break;
	default:
		G_endMe = TRUE;
	}
}

/*************************** m75_test_setgetstat ****************************
 *
 *  Description:  m75_test_setgetstat() - function
 *
 *---------------------------------------------------------------------------
 *  Input......:  fhHdl     Handle
 *                verbose   flag, perform detail outputs
 *
 *  Output.....:  return    0   if no error
 *                          1   if error
 *
 *  Globals....:  -
 ****************************************************************************/
int32 m75_test_setgetstat(MDIS_PATH fdHdl, u_int8 verbose)
{
	int32 val, val1, ch;
	u_int32 sig, error=0;
	int32	i;

	printf("\nm75_test_setgetstat\n");
	for( ch=0; ch<2; ch++ )
    {
		if( verbose )
			printf("\n      check channel %d\n", ch);
	   	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */

		/* --- check WR15 --- */
		M_setstat(fdHdl, M75_SCC_REG_15, WR15_TEST_DEFAULT);
		M_getstat(fdHdl, M75_SCC_REG_15, &val);
		if(val == WR15_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR15 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 15\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR7P_TEST_DEFAULT);
			error++;
		}
	/* --- check WR7P (enable extended read) --- */
/* not correct: extended read/normal read */
		M_setstat(fdHdl, M75_SCC_REG_15, WR15_TEST_DEFAULT | 0x04); /* set extended */
/* it should be */
/*		M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT | 0x40);*/

		M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT);

		M_getstat(fdHdl, M75_SCC_REG_14, &val); /* only extended read */
		if(val == WR7P_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR7P OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 7P\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR7P_TEST_DEFAULT);
			error++;
		}
	/* --- check WR03 --- */
		M_setstat(fdHdl, M75_SCC_REG_03, WR03_TEST_DEFAULT);
		M_getstat(fdHdl, M75_SCC_REG_09, &val); /* only extended read */
		if(val == WR03_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR03 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 03\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR03_TEST_DEFAULT);
			error++;
		}
	/* --- check WR04 --- */
		M_setstat(fdHdl, M75_SCC_REG_04, WR04_TEST_DEFAULT);
		/*M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT & 0xBF);*/ /* disable extended read */
		M_getstat(fdHdl, M75_SCC_REG_04, &val); /* only extended read else RR00*/
		if(val == WR04_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR04 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 04\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR04_TEST_DEFAULT);
			error++;
		}
	/* --- check WR05 --- */
		M_setstat(fdHdl, M75_SCC_REG_05, WR05_TEST_DEFAULT);
		/*M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT & 0xBF);*/ /* disable extended read */
		M_getstat(fdHdl, M75_SCC_REG_05, &val); /* only extended read else RR01*/
		if(val == WR05_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR05 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 05\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR05_TEST_DEFAULT);
			error++;
		}
	/* --- check WR10 --- */
		M_setstat(fdHdl, M75_SCC_REG_10, WR10_TEST_DEFAULT);
		/*M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT & 0xBF);*/ /* disable extended read */
		M_getstat(fdHdl, M75_SCC_REG_11, &val); /* only extended read else WR15*/
		if(val == WR10_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR10 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 10\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR10_TEST_DEFAULT);
			error++;
		}
	/* --- check WR12 --- */
		M_setstat(fdHdl, M75_SCC_REG_12, WR12_TEST_DEFAULT);
		M_getstat(fdHdl, M75_SCC_REG_12, &val); /* only extended read */
		if(val == WR12_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR12 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 12\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR12_TEST_DEFAULT);
			error++;
		}
	/* --- check WR13 --- */
		M_setstat(fdHdl, M75_SCC_REG_13, WR13_TEST_DEFAULT);
#if 0   /* 1: normal read with RR09, 0: extended read with RR13 */
		M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT & 0xBF); /* disable extended read */
		M_getstat(fdHdl, M75_SCC_REG_09, &val); /* normal read */
		M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT);        /* enable extended read again */
#else
		M_getstat(fdHdl, M75_SCC_REG_13, &val); /* only extended read */
#endif
		if(val == WR13_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR13 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 13\n\
				read:     %04X\n				required: %04X\n",
				ch, val, WR13_TEST_DEFAULT);
			error++;
		}

	/* --- M75_BRGEN_TCONST --- */
		M_setstat(fdHdl, M75_BRGEN_TCONST, 0x315467);
		M_getstat(fdHdl, M75_SCC_REG_12, &val); /* only extended read */
		M_getstat(fdHdl, M75_SCC_REG_13, &val1); /* only extended read */
		if( (val == 0x67) && (val1 == 0x54) ){
			if( verbose )
				printf("        ==> M75_BRGEN_TCONST OK\n");
		} else {
			printf("Error: RegCheck channel %d, M75_BRGEN_TCONST\n"\
				"\t\tread WR12:     %04X\n\t\trequired WR12: %04X\n"\
				"\t\tread WR13:     %04X\n\t\trequired WR13: %04X\n",
				ch, val, 0x67, val1, 0x54);
			error++;
		}
	/* --- check WR14 --- */
		M_setstat(fdHdl, M75_SCC_REG_14, WR14_TEST_DEFAULT);
/* not correct extended/normal read switch */
		M_setstat(fdHdl, M75_SCC_REG_15, WR15_TEST_DEFAULT & 0xFB);
/* it should be */
/*		M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_TEST_DEFAULT & 0xBF);*/
		M_getstat(fdHdl, M75_SCC_REG_14, &val); /* normal read */
		if(val == WR14_TEST_DEFAULT){
			if( verbose )
				printf("        ==> WR14 OK\n");
		} else {
			printf("Error: RegCheck channel %d, reg 14\n", ch);
			printf("       read:     %04X\n        required: %04X\n",
				    val, WR14_TEST_DEFAULT);
			error++;
		}

		/* --- check FIFO  --- */
		if( verbose ) {
			printf("        -------------\n");
			printf("      check FIFO, FIFO reset and FIFO S&C Reg channel %d\n",
					ch);
		}
		/* reset Module */
		M_setstat(fdHdl, M75_SCC_REG_09, 0xC0);
		M_getstat(fdHdl, M75_SCC_REG_00, &val); /* dummy read after reset */
		/* enable FIFO *//* FIFO Status register not needed here */
		/*M_getstat(fdHdl, M75_SCC_REG_15, &val);*/	/* get register  RR15 */
		/*M_setstat(fdHdl, M75_SCC_REG_15, val|0x04);*/
		/* reset Tx FIFO */
		M_setstat(fdHdl, M75_FIFO_RESET, 0x11 );
		/* disable transmitting data of FIFO */
		M_setstat(fdHdl, M75_TXEN, 0x00 );

		/* fill Tx FIFO */
		if( verbose ) {
			printf("        FIFO status channel");
			M_getstat(fdHdl, M75_FIFO_STATUS, &val);
			printf(" %d = 0x%02X\n", ch, val);
			printf("        FIFO half full:");
		}
		for(i=0; i<=0x400; i++){
			M_setstat( fdHdl, M75_FIFO_DATA, i );
		}
		M_getstat(fdHdl, M75_FIFO_STATUS, &val);
		if((val & 0x70) == 0x20) {
			if( verbose )
				printf("        ==> OK\n");
		}else{
			printf("\nError: FIFO half check, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x20);
			error++;
		}

		/* fill up to the top */
		if( verbose )
			printf("        FIFO full:");
		for(i=0x400; i<0x800; i++){
			M_setstat( fdHdl, M75_FIFO_DATA, i );
		}
		UOS_Delay(200);
		M_getstat(fdHdl, M75_FIFO_STATUS, &val);
		if((val & 0x70) == 0x60) {
			if( verbose )
				printf("        ==> OK\n");
		}else{
			printf("\nError: FIFO full check, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x60);
			error++;
		}

		/* reset Tx FIFO */
		if( verbose )
			printf("        FIFO reset:");
		M_setstat(fdHdl, M75_FIFO_RESET, 0x11 );
		UOS_Delay(100);
		M_getstat(fdHdl, M75_FIFO_STATUS, &val);
		if((val & 0x70) == 0x10) {
			if( verbose )
				printf("        ==> OK\n");
		}else{
			printf("\nError: FIFO reset check, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: 0x1x\n",
				ch, val);
			error++;
		}

		/* --- check other get_setstats --- */
		/* M75_GETBLOCK_TOUT */
		if( verbose ) {
			printf("        -------------\n");
			printf("      check other Set/Getstats %d\n", ch);
		}
		val1 = 0x3B6E + ch;
		if( M_setstat(fdHdl, M75_GETBLOCK_TOUT, val1) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_GETBLOCK_TOUT, &val) )
			goto M75_ERR;
		if(val == val1) {
			if( verbose )
				printf("        M75_GETBLOCK_TOUT ==> OK\n");
		}else{
			printf("\nError: M75_GETBLOCK_TOUT, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, val1);
			error++;
		}

		/* M75_SETBLOCK_TOUT */
		val1 = 0xC642 + ch;
		if( M_setstat(fdHdl, M75_SETBLOCK_TOUT, val1) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_SETBLOCK_TOUT, &val) )
			goto M75_ERR;
		if(val == val1) {
			if( verbose )
				printf("        M75_SETBLOCK_TOUT ==> OK\n");
		}else{
			printf("\nError: M75_SETBLOCK_TOUT, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, val1);
			error++;
		}
		/* M75_MAX_RXFRAME_NUM larger*/
		val1 = 0x200 + ch;
		if( M_setstat(fdHdl, M75_MAX_RXFRAME_NUM, val1) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_MAX_RXFRAME_NUM, &val) )
			goto M75_ERR;
		if(val == val1) {
			if( verbose )
				printf("        M75_MAX_RXFRAME_NUM larger==> OK\n");
		}else{
			printf("\nError: M75_MAX_RXFRAME_NUM larger, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, val1);
			error++;
		}
		/* M75_MAX_RXFRAME_NUM smaller*/
		if( M_setstat(fdHdl, M75_MAX_RXFRAME_NUM, 0x80+ch) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_MAX_RXFRAME_NUM, &val) )
			goto M75_ERR;
		if(val == 0x80+ch) {
			if( verbose )
				printf("        M75_MAX_RXFRAME_NUM smaller ==> OK\n");
		}else{
			printf("\nError: M75_MAX_RXFRAME_NUM smaller, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x80+ch);
			error++;
		}
		/* M75_MAX_TXFRAME_NUM larger*/
		if( M_setstat(fdHdl, M75_MAX_TXFRAME_NUM, 0x10F+ch) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_MAX_TXFRAME_NUM, &val) )
			goto M75_ERR;
		if(val == 0x10F+ch) {
			if( verbose )
				printf("        M75_MAX_TXFRAME_NUM larger ==> OK\n");
		}else{
			printf("\nError: M75_MAX_TXFRAME_NUM larger, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x10F+ch);
			error++;
		}
		/* M75_MAX_TXFRAME_NUM smaller*/
		if( M_setstat(fdHdl, M75_MAX_TXFRAME_NUM, 0x40+ch) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_MAX_TXFRAME_NUM, &val) )
			goto M75_ERR;
		if(val == 0x40+ch) {
			if( verbose )
				printf("        M75_MAX_TXFRAME_NUM smaller ==> OK\n");
		}else{
			printf("\nError: M75_MAX_TXFRAME_NUM smaller, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x40+ch);
			error++;
		}
		/* M75_MAX_RXFRAME_SIZE */
		if( M_setstat(fdHdl, M75_MAX_RXFRAME_SIZE, 0x400+ch) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_MAX_RXFRAME_SIZE, &val) )
			goto M75_ERR;
		if(val == 0x400+ch) {
			if( verbose )
				printf("        M75_MAX_RXFRAME_SIZE ==> OK\n");
		}else{
			printf("\nError: M75_MAX_RXFRAME_SIZE, channel %d, Rx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x400+ch);
			error++;
		}
		/* M75_MAX_TXFRAME_SIZE */
		if( M_setstat(fdHdl, M75_MAX_TXFRAME_SIZE, 0x600+ch) )
			goto M75_ERR;
		if( M_getstat(fdHdl, M75_MAX_TXFRAME_SIZE, &val) )
			goto M75_ERR;
		if(val == 0x600+ch) {
			if( verbose )
				printf("        M75_MAX_TXFRAME_SIZE ==> OK\n");
		}else{
			printf("\nError: M75_MAX_TXFRAME_SIZE, channel %d, Tx FIFO\n\
				read status:     %04X\n				required: %04X\n",
				ch, val, 0x600+ch);
			error++;
		}

		/* M75_SETRXSIG / M75_SETRXSIG */
		sig = (ch ? UOS_SIG_USR2: UOS_SIG_USR1);
		UOS_SigInit( SigHandler );
		if( UOS_SigInstall( sig ) )
			errShow();

		if( M_setstat(fdHdl, M75_SETRXSIG, sig) )
			goto M75_ERR;
		M_setstat(fdHdl, M75_SETRXSIG, sig);
		if( UOS_ErrnoGet() == M75_ERR_SIGBUSY ) {
			if( verbose )
				printf("        M75_SETRXSIG ==> OK\n");
		} else {
			printf("\nError: M75_SETRXSIG, channel ch\n");
			error++;
		}

		if( M_setstat(fdHdl, M75_CLRRXSIG, 0x00) )
			goto M75_ERR;
		if( M_setstat(fdHdl, M75_SETRXSIG, sig) )
			goto M75_ERR;
		if( M_setstat(fdHdl, M75_CLRRXSIG, 0x00) )
			goto M75_ERR;

		if( UOS_SigRemove( sig ) )
			goto M75_ERR;

		/* M75_SCC_REGS  */
		{
			M75_SCC_REGS_PB sccregs;
			M_SG_BLOCK blk;

			/* restore M75_WRxx_DEFAULTs */
			M_setstat(fdHdl, M75_SCC_REG_01, WR01_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_02, WR02_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_03, WR03_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_04, WR04_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_05, WR05_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_06, WR06_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_07, WR07_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_7P, WR7P_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_09, WR09_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_10, WR10_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_11, WR11_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_12, WR12_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_13, WR13_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_14, WR14_DEFAULT);
			M_setstat(fdHdl, M75_SCC_REG_15, WR15_DEFAULT);

			blk.size = sizeof(M75_SCC_REGS_PB);
			blk.data = (void *)&sccregs;

			if( M_getstat(fdHdl, M75_SCC_REGS, (int32 *)&blk) )
				goto M75_ERR;
			if( verbose )
				printf( "        Current default configuration:\n"
				    "        WR01 = 0x%02X"
					"        WR02 = 0x%02X"
					"        WR03 = 0x%02X\n"
					"        WR04 = 0x%02X"
					"        WR05 = 0x%02X"
					"        WR06 = 0x%02X\n"
					"        WR07 = 0x%02X"
					"        WR7p = 0x%02X"
					"        WR09 = 0x%02X\n"
					"        WR10 = 0x%02X"
					"        WR11 = 0x%02X"
					"        WR12 = 0x%02X\n"
					"        WR13 = 0x%02X"
					"        WR14 = 0x%02X"
					"        WR15 = 0x%02X\n\n\n",
					sccregs.wr01, sccregs.wr02, sccregs.wr03, sccregs.wr04,
					sccregs.wr05, sccregs.wr06, sccregs.wr07, sccregs.wr7p,
					sccregs.wr09, sccregs.wr10, sccregs.wr11, sccregs.wr12,
					sccregs.wr13, sccregs.wr14, sccregs.wr15 );
		}

		if( verbose )
			printf("    ====================\n");
	}

	return( error );

M75_ERR:
    errShow();
    printf("    => Error\n");
    return( 1 );
}

/**************************** m75_openclose *********************************
 *
 *  Description:  Open and close the device. Read the SW-Revision ID's
 *                and the M-Modul ID.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  devName device name in the system e.g. "/m75/0"
 *
 *  Output.....:  return  T_OK or T_ERROR
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
int32 m75_openclose( char *devName )
{
   int    ret = T_OK;
   MDIS_PATH  fd;
   int32  size;
   char   *buf;
   M_SETGETSTAT_BLOCK blkStruct;
   u_int16 *dataP;
   u_int32 i;
   u_int32 maxWords;

   printf("m75 - open close \n");
   printf("    open\n");
   fd = M_open( devName );
   if( fd >= 0 )
   {
       printf("    => OK\n");

       printf("    revision id's\n");
       size = 4096;
       buf = (char*) malloc( size );
       if( buf )
       {
           *buf = 0;
           blkStruct.size = size;
           blkStruct.data = buf;

           if( M_getstat( fd, M_MK_BLK_REV_ID, (int32*) &blkStruct ) )
           {
               errShow();
               printf("    => Error\n");
               ret = T_ERROR;
           }
           else
           {
               printf("    => OK\n");
           }/*if*/

           printf( buf );

           printf("    module id\n");
           maxWords = 128;
           blkStruct.size = maxWords*2;

           if( M_getstat( fd, M_LL_BLK_ID_DATA, (int32*) &blkStruct ) )
           {
               errShow();
               printf("    => Error\n");
               ret = T_ERROR;
           }
           else
           {
               printf("    => OK\n");
               dataP = (u_int16*) blkStruct.data;
               for( i=1; i<=maxWords; i++ )
               {
                   printf( " 0x%04x", (int)*dataP );
                   if( !(i%8) )
                       printf("\n");
                   dataP++;
               }/*for*/
           }/*if*/

           free(buf);
       }
       else
           printf(" can't allocate user buffer\n");





       printf("    close\n");
       if( M_close( fd ) == 0 )
       {
           printf("    => OK\n");

       }
       else
       {
           errShow();
           printf("    => Error\n");
           ret = T_ERROR;
       }/*if*/
   }
   else
   {
       errShow();
       printf("    => Error\n");
       ret = T_ERROR;
   }/*if*/

   return( ret );
}/**/


/**************************** m75_test_TxQ *********************************
 *
 *  Description:  Write .
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  devName device name in the system e.g. "/m75/0"
 *                numpak  number of frames to be sent to Tx queue
 *
 *  Output.....:  return  T_OK or T_ERROR
 *
 *  Globals....:  ---
 *
 *  works only if function call M75_SendFrame in M75_WriteBlock
 *  is commented out, otherwise queue is emtied before getting full
 ****************************************************************************/
int32 m75_test_TxQ(MDIS_PATH fdHdl, int32 numpak)
{
	int32 val,ch,pak;
	int32	i;
	u_int32 error=0, syserror=0;
	u_int8 m75_buf[TEST_MAXFRAMES+2], *bufp = m75_buf;
	int32 nbrOfBytesSent;

	printf("\nm75_test_TxQ\n");
	if( numpak > TEST_MAXFRAMES ) return ( 123 );

	/* reset Tx FIFO */
	M_setstat(fdHdl, M75_FIFO_RESET, 0x11 );
	/* enable FIFO */
	M_getstat(fdHdl, M75_SCC_REG_15, &val);	/* get register  RR15 */
	M_setstat(fdHdl, M75_SCC_REG_15, val|0x04);

	/* init buffer */
	for(i=0; i<numpak+2; i++)
		*bufp++ = (u_int8)i;

	for( ch=0; ch<2; ch++ )
    {
	   	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */
		M_setstat(fdHdl, M75_SETBLOCK_TOUT, 0x0000FF);
		M_setstat(fdHdl, M75_MAX_TXFRAME_NUM, numpak);
		M_setstat(fdHdl, M75_MAX_TXFRAME_SIZE, numpak+1);

		M_getstat(fdHdl, M75_MAX_TXFRAME_NUM, &val);
		printf("    M_getstat(M75_MAX_TXFRAME_NUM):  0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_TXFRAME_SIZE, &val);
		printf("    M_getstat(M75_MAX_TXFRAME_SIZE): 0x%04X\n", val);

		/* send some packets (fill queue) */
		for(pak = 1; pak <= numpak; pak++){

			nbrOfBytesSent = M_setblock(fdHdl,m75_buf,pak);
			printf("    M_setblock(%d) \tnbrOfBytesSent=%d\n", pak, nbrOfBytesSent);

			if(nbrOfBytesSent != pak) {
				errShow();
				printf("\nError: M75_BlockWrite, channel %d, \n\
				transmitted:     0x%04X\n \t\t\t\trequired: 0x%04X\n\
				if this error returns no buffer available, ==> OK\n",
				ch, nbrOfBytesSent, pak);
				error++;
			}
		}
		if(!error) printf("    => OK, %d packets written to queue\n", numpak);

		/* send one more than queue can take */
		nbrOfBytesSent = M_setblock(fdHdl,m75_buf,pak);
		printf("    M_setblock(%d) \tnbrOfBytesSent=%d\n", pak, nbrOfBytesSent);

		if( (nbrOfBytesSent != pak) &&
			((syserror = UOS_ErrnoGet()) == ERR_OSS_TIMEOUT) ){
			printf("    => OK, ERR_OSS_TIMEOUT (expected)\n");
		} else {
			errShow();
		}


		/* send one bigger than queue can take */
		pak++;
		nbrOfBytesSent = M_setblock(fdHdl,m75_buf,pak);
		printf("    M_setblock(%d) \tnbrOfBytesSent=%d\n", pak, nbrOfBytesSent);

		if( (nbrOfBytesSent != pak) &&
			((syserror = UOS_ErrnoGet()) == M75_ERR_FRAMETOOLARGE) ){
			printf("    => OK, detected M75_ERR_FRAMETOOLARGE (expected)\n");
		} else {
			errShow();
		}

		printf("    --------------------\n");
	}
	if(!error)
		printf("  TEST_TxQ ==> OK\n");
	printf("    ====================\n");

	return( error );
/*
M75_ERR:
    errShow();
    printf("    => Error\n");
    return( 1 );
*/
}


/**************************** M75_Test_Tx **********************************
 *
 *  Description:  Write .
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  fdHdl    handle to opened hardware
 *                numpak   number of frames to send
 *                tconst   BR Gen timeconstant
 *                tout     M75_setblock timeout when Tx queue is full
 *                ch       channel,
 *                filename when provided, parameters and data are read
 *                         from this file
 *                         (parameters from command line have priority)
 *                verbose  perform detailed outputs
 *
 *  Output.....:  return  T_OK or T_ERROR
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
int32 m75_test_Tx(MDIS_PATH fdHdl, int32 numpak,
				  int32 tconst, int32 tout, u_int8 ch,
				  char *filename, u_int8 verbose ){

	int32 val, pak;
	int32	i;
	u_int32 error=0;
	u_int8 *m75_TxBuf=NULL,
		   *bufp,
		   inframe=0;
	int32 nbrOfBytesSent;
	FILE *fileH=NULL;
	int32	brGen_TConst=DEFAULT_BRGEN_TCONST,
			setBlockTout=0,
			txFrameSize=EXT_FIFO_SIZE,
			txFrameNum =0x10,
			numpacks=1,
			fsize=0,
			fnum=0;
	char line[50], *errstr;

	printf("\nm75_test_Tx channel %d\n", ch);

	if( filename ) {
		if( (fileH = fopen( filename, "r" )) == NULL ){
			printf("\n*** cannot open file %s\n",filename);
			goto M75_ERR;
		}
	}
	/* read Tx options (optional) */
	if( fileH ){
		while( !inframe && !error){
			if( (fgets( line, 50, fileH ) == NULL) ||
				ferror(fileH) ||
				feof(fileH) ) {
				error++;
				goto M75_ERR;
			} else {
				if( !(val = strncmp( line, "#M75_BRGEN_TCONST", 17 )) ){
					/* look for hex num */
					for(i=1; i<50; i++) {
						if( (line[i] == '0') && (line[i+1] == 'x') ) {
							brGen_TConst = strtol(&line[i], &errstr, 16);
							break;
						}
					}
				} else if( !strncmp( line, "#M75_SETBLOCK_TOUT", 18 ) ){
					/* look for hex num */
					for(i=1; i<50; i++) {
						if( (line[i] == '0') && (line[i+1] == 'x') ) {
							setBlockTout = strtol(&line[i], &errstr, 16);
							break;
						}
					}
				} else if( !strncmp( line, "#M75_MAX_TXFRAME_NUM", 18 ) ){
					/* look for hex num */
					for(i=1; i<50; i++) {
						if( (line[i] == '0') && (line[i+1] == 'x') ) {
							txFrameNum = strtol(&line[i], &errstr, 16);
							break;
						}
					}
				} else if( !strncmp( line, "#M75_MAX_TXFRAME_SIZE", 21 ) ){
					/* look for hex num */
					for(i=1; i<50; i++) {
						if( (line[i] == '0') && (line[i+1] == 'x') ) {
							txFrameSize = strtol(&line[i], &errstr, 16);
							break;
						}
					}
				} else if( !strncmp( line, "#NUM_PACKS", 10 ) ){
					/* look for hex num */
					for(i=1; i<50; i++) {
						if( (line[i] == '0') && (line[i+1] == 'x') ) {
							numpacks = strtol(&line[i], &errstr, 16);
							break;
						}
					}
				} else if( !strncmp( line, "#$" , 2 ) ){
					/* beginning of frame */
					inframe++;
					break;
				}


			}
		}
	}

	/* command line arguments have priority */
	if( tconst )
		brGen_TConst = tconst;
	if( tout )
		setBlockTout = tout;
	if( numpak )
		numpacks = numpak;

	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */

	/* BR Gen Timeconstant */
	M_setstat( fdHdl, M75_BRGEN_TCONST, brGen_TConst );

	M_setstat(fdHdl, M75_SETBLOCK_TOUT, setBlockTout);
	while( M_setstat(fdHdl, M75_MAX_TXFRAME_NUM, txFrameNum) ){
		printf( "*** Error *** set M75_MAX_TXFRAME_NUM:"
				" not enough memory for 0x%04X frames\n",
				txFrameNum);
		txFrameNum /= 2;
		printf( "              trying to allocate for 0x%04X frames\n",
				txFrameNum);
		/* goto M75_ERR; */
	}
	if( M_setstat(fdHdl, M75_MAX_TXFRAME_SIZE, txFrameSize) ) {
		printf( "*** Error *** set M75_MAX_TXFRAME_SIZE:"
				" not enough memory for %04X frames of %04X bytes, aborting\n",
				txFrameNum, txFrameSize);
		goto M75_ERR;
	}
	if( verbose ){
		M75_SCC_REGS_PB sccregs;
		M_SG_BLOCK msg_blk;
		printf("  Tx Parameters:\n");
		M_getstat(fdHdl, M75_SETBLOCK_TOUT, &val);
		printf("    M75_SETBLOCK_TOUT:    0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_TXFRAME_NUM, &val);
		printf("    M75_MAX_TXFRAME_NUM:  0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_TXFRAME_SIZE, &val);
		printf("    M75_MAX_TXFRAME_SIZE: 0x%04X\n", val);
		msg_blk.size = sizeof(M75_SCC_REGS_PB);
		msg_blk.data = (void *)&sccregs;
		M_getstat(fdHdl, M75_SCC_REGS, (int32 *)&msg_blk);
		printf( "    Current configuration:\n"
			    "        WR01 = 0x%02X    WR02 = 0x%02X    WR03 = 0x%02X\n"
				"        WR04 = 0x%02X    WR05 = 0x%02X    WR06 = 0x%02X\n"
				"        WR07 = 0x%02X    WR7p = 0x%02X    WR09 = 0x%02X\n"
				"        WR10 = 0x%02X    WR11 = 0x%02X    WR12 = 0x%02X\n"
				"        WR13 = 0x%02X    WR14 = 0x%02X    WR15 = 0x%02X\n\n\n",
				sccregs.wr01, sccregs.wr02, sccregs.wr03,
				sccregs.wr04, sccregs.wr05, sccregs.wr06,
				sccregs.wr07, sccregs.wr7p, sccregs.wr09,
				sccregs.wr10, sccregs.wr11, sccregs.wr12,
				sccregs.wr13, sccregs.wr14, sccregs.wr15 );
	}

	m75_TxBuf = (u_int8*)malloc(EXT_FIFO_SIZE);
	bufp = m75_TxBuf;

	/* enable IRQs */
	M_setstat(fdHdl, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(fdHdl, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */


	for(pak=1; pak<=numpacks; pak++) {

		/* init buffer */
		if( fileH ) {
			bufp = m75_TxBuf;
			fsize = 0;
			if( !inframe ) {
				/*look for begin of frame */
				while( !inframe && !error ){
					if( (fgets( line, 50, fileH ) == NULL) ||
						ferror(fileH) ||
						feof(fileH) ) {
						error++;
						goto M75_ERR;
					} else if( !strncmp( line, "#$" , 2 ) ){
						/* beginning of frame */
						inframe++;
						break;
					}
				}
			}
			if( inframe ){
				/* found begin of frame, read frame data from file */
				while( inframe && !error ){
					if( (fgets( line, 50, fileH ) == NULL) ||
						ferror(fileH) ||
						feof(fileH) ) {
						error++;
						goto M75_ERR;
					} else if( !strncmp( line, "#$" , 2 ) ){
						inframe--;
					} else {
						if( fsize > txFrameSize ) {
							error++;
							printf("*** ERROR   frame %d too big (maxFrameSize = %d)\n",
								fnum+1, txFrameSize);
							goto M75_ERR;
						}
						*bufp++ = (u_int8)strtol(line, &errstr, 16);
						fsize++;
					}
				}
			}
		} else {
			bufp = m75_TxBuf;
			if( fsize <= (txFrameSize - 7) )
				fsize += 7;
			for(i=1; i<=fsize; i++)
				*bufp++ = (u_int8)i;
		}

		/* send frame */
		if( (nbrOfBytesSent = M_setblock(fdHdl,m75_TxBuf,fsize)) == fsize ) {
			fnum++;
			if( tconst > 0x20 ) {
				if( nbrOfBytesSent < 0x100 )
					UOS_Delay(20); /* give frame the chance to be transmitted */
				else if( nbrOfBytesSent < 0x300 )
					UOS_Delay(60); /* give frame more chance to be transmitted */
				else if( nbrOfBytesSent < 0x400 )
					UOS_Delay(100); /* give frame even more chance to be transmitted */
				else
					UOS_Delay(100); /* give frame a lot more chance to be transmitted */
			} else {
				UOS_Delay(20); /* give frame the chance to be transmitted */
			}

		} else {
			errShow();
			error++;
			break;
		}

		if( verbose )
			printf("    M_setblock(%d) \tnbrOfBytesSent=%d\n",
					fnum, nbrOfBytesSent);

	}
	free(m75_TxBuf);

	if( fileH )
		fclose(fileH);
	if(!error)
		printf("  TEST_Tx ==> sent %d frames\n", fnum);
	printf("    ====================\n");
	/* give driver the chance to empty Tx Buffer */
	UOS_Delay(5000);
	return( error );

M75_ERR:

    errShow();
    printf("    => Error\n");
	free(m75_TxBuf);
	if( fileH )
		fclose(fileH);
    return( 1 );
}

/**************************** m75_test_Rx **********************************
 *
 *  Description:  Read frame(s).
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  fdHdl    handle to opened hardware
 *                tconst   BR Gen timeconstant
 *                tout     M75_setblock timeout when Tx queue is full
 *                ch       channel,
 *                filename when provided, received frames are written
 *                         to this file
 *                verbose  perform detailed outputs
 *
 *  When available the Rx parameters are read from the file .\conf.rx
 *  (parameters from command line have priority)
 *
 *  Output.....:  return  T_OK or T_ERROR
 *
 *  Globals....:  G_sigUosCnt[2], G_endMe
 *
 ****************************************************************************/
int32 m75_test_Rx(MDIS_PATH fdHdl,
				  int32 tconst, int32 tout, u_int8 useSig,
				  u_int8 ch, char *filename, u_int8 verbose ){

	int32	val, i, oldSigCnt=0, bytesRx=0;
	u_int32 error=0, numerrors=0, recFrames=0;
	FILE	*fileH;
	int32	brGen_TConst=DEFAULT_BRGEN_TCONST,
			getBlockTout=0,
			rxFrameSize=EXT_FIFO_SIZE,
			rxFrameNum =0x10;
	char	line[50], *errstr;


	printf("\nm75_test_Rx channel %d\n", ch);

	/* try to get configuration settings from conf.rx file */
	if( (fileH = fopen( "./conf.rx", "r" )) == NULL ){
		printf("\n*** file .\\conf.rx not found\n");
	} else {
		printf("    found ./conf.rx file\n");
		while( fgets( line, 50, fileH ) &&
				!ferror(fileH) &&
				!feof(fileH) ) {
			if( !strncmp( line, "#M75_BRGEN_TCONST", 17 ) ){
				/* look for hex num */
				for(i=1; i<50; i++) {
					if( (line[i] == '0') && (line[i+1] == 'x') ) {
						brGen_TConst = strtol(&line[i], &errstr, 16);
						break;
					}
				}
			} else if( !strncmp( line, "#M75_GETBLOCK_TOUT", 18 ) ){
				/* look for hex num */
				for(i=1; i<50; i++) {
					if( (line[i] == '0') && (line[i+1] == 'x') ) {
						getBlockTout = strtol(&line[i], &errstr, 16);
						break;
					}
				}
			} else if( !strncmp( line, "#M75_MAX_RXFRAME_NUM", 18 ) ){
				/* look for hex num */
				for(i=1; i<50; i++) {
					if( (line[i] == '0') && (line[i+1] == 'x') ) {
						rxFrameNum = strtol(&line[i], &errstr, 16);
						break;
					}
				}
			} else if( !strncmp( line, "#M75_MAX_RXFRAME_SIZE", 21 ) ){
				/* look for hex num */
				for(i=1; i<50; i++) {
					if( (line[i] == '0') && (line[i+1] == 'x') ) {
						rxFrameSize = strtol(&line[i], &errstr, 16);
						break;
					}
				}
			}
		}
		fclose(fileH);
		fileH=NULL;
		if(verbose)
			printf( "    Rx parameters read from file:\n"
					"                   M75_BRGEN_TCONST     %04X\n"
					"                   M75_GETBLOCK_TOUT    %04X\n"
					"                   M75_MAX_RXFRAME_NUM  %04X\n"
					"                   M75_MAX_RXFRAME_SIZE %04X\n",
					brGen_TConst, getBlockTout, rxFrameNum, rxFrameSize);


	}
	/* command line arguments have priority */
	if( tconst )
		brGen_TConst = tconst;
	if( useSig )
		getBlockTout = 0x00;
	else if( tout )
		getBlockTout = tout;

	if( filename )
		if( (fileH = fopen( filename, "w" )) == NULL ){
			printf("\n*** cannot open file %s\n",filename);
			fprintf(fileH, "This is a test");
			goto M75_ERR;
		}
	if( fileH )
		fprintf(fileH, "#Received Frames\n");
#if 0
	for(i=0;i<200;i++){ /*200frames*/
		fprintf(fileH,"#$\n0x%02X\n",i); /* frame begin */
		for(j=0;j<0x7FA;j++)
			fprintf(fileH, "0x%02X\n",j);
		fprintf(fileH,"#$\n"); /* frame end */
	}
#endif
	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */

	/* BR Gen Timeconstant */
	/* irrelevant since data is synchronized through the transmit clock (RTxC)
	M_setstat( fdHdl, M75_BRGEN_TCONST, brGen_TConst );
	*/

	M_setstat(fdHdl, M75_GETBLOCK_TOUT, getBlockTout);
	while( M_setstat(fdHdl, M75_MAX_RXFRAME_NUM, rxFrameNum) ){
		printf( "*** Error *** set M75_MAX_RXFRAME_NUM: not enough memory for %04X frames",
				rxFrameNum);
		rxFrameNum /= 2;
		printf( "              trying to allocate for %04X frames\n",
				rxFrameNum);
		/* goto M75_ERR; */
	}
	if( M_setstat(fdHdl, M75_MAX_RXFRAME_SIZE, rxFrameSize) ) {
		printf( "*** Error *** set M75_MAX_RXFRAME_SIZE:"
				" not enough memory for %04X frames of %04X bytes, aborting\n",
				rxFrameNum, rxFrameSize);
		goto M75_ERR;
	}


	/* enable IRQs */
	M_setstat(fdHdl, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(fdHdl, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */

	M_setstat( fdHdl, M75_RXEN, 1 );

	if( verbose ){
		M75_SCC_REGS_PB sccregs;
		M_SG_BLOCK msg_blk;
		printf("  Rx Parameters:\n");
		M_getstat(fdHdl, M75_GETBLOCK_TOUT, &val);
		printf("    M75_GETBLOCK_TOUT:    0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_RXFRAME_NUM, &val);
		printf("    M75_MAX_RXFRAME_NUM:  0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_RXFRAME_SIZE, &val);
		printf("    M75_MAX_RXFRAME_SIZE: 0x%04X\n", val);

		msg_blk.size = sizeof(M75_SCC_REGS_PB);
		msg_blk.data = (void *)&sccregs;
		M_getstat(fdHdl, M75_SCC_REGS, (int32 *)&msg_blk);
		printf( "    Current configuration:\n"
			    "        WR01 = 0x%02X    WR02 = 0x%02X    WR03 = 0x%02X\n"
				"        WR04 = 0x%02X    WR05 = 0x%02X    WR06 = 0x%02X\n"
				"        WR07 = 0x%02X    WR7p = 0x%02X    WR09 = 0x%02X\n"
				"        WR10 = 0x%02X    WR11 = 0x%02X    WR12 = 0x%02X\n"
				"        WR13 = 0x%02X    WR14 = 0x%02X    WR15 = 0x%02X\n\n\n",
				sccregs.wr01, sccregs.wr02, sccregs.wr03,
				sccregs.wr04, sccregs.wr05, sccregs.wr06,
				sccregs.wr07, sccregs.wr7p, sccregs.wr09,
				sccregs.wr10, sccregs.wr11, sccregs.wr12,
				sccregs.wr13, sccregs.wr14, sccregs.wr15 );
	}

	if( useSig ) {

		UOS_SigInit( SigHandler );
		if( !ch && UOS_SigInstall( UOS_SIG_USR1 ) )
			errShow();
		else if( UOS_SigInstall( UOS_SIG_USR2 )   )
			errShow();

		if( !ch )
			M_setstat(fdHdl, M75_SETRXSIG, UOS_SIG_USR1);
		else
			M_setstat(fdHdl, M75_SETRXSIG, UOS_SIG_USR2);

		G_endMe = FALSE;
		oldSigCnt = G_sigUosCnt[ch];

		while( !error && !G_endMe && (UOS_KeyPressed() == -1) ) {
			/* receive frame(s) */

			if( oldSigCnt < G_sigUosCnt[ch] ) { /* new signal arrived */
				oldSigCnt++;
				while( !error ) {
					if( !(error = m75_getFrame(fdHdl, fileH,
												&bytesRx, verbose)) ) {
						recFrames++;
						if(verbose)
							printf("   ==> OK: Frame %4d, length 0x%04X;\n\n",
						       recFrames, bytesRx);
					} else {
						if( (error == M75_ERR_RX_QEMPTY) ||
							(error == ERR_OSS_TIMEOUT) ) {
							error = 0;
							break;
						} else if( (error == M75_ERR_RX_QFULL) ||
								   (error == M75_ERR_RX_OVERFLOW)) {
							printf("*** %s ***\n",M_errstring( error ) );
							error = 0;
							numerrors++;
							break;
						} else if( (error == M75_ERR_RX_BREAKABORT) ) {
							printf("*** %s ***\n",M_errstring( error ) );
							error = 0;
							numerrors++;
						} else {
							printf("*** %s ***\n",M_errstring( error ) );
							numerrors++;
						}
					}
				}
			}
		}
		M_setstat(fdHdl, M75_CLRRXSIG, 0x00);
		if( !ch && UOS_SigRemove( UOS_SIG_USR1 ) )
			errShow();
		else if( UOS_SigRemove( UOS_SIG_USR2 ) )
			errShow();


	} else {
		/* continously receive frame(s) */
		while( !error && (UOS_KeyPressed() == -1) ) {
			if( !(error = m75_getFrame(fdHdl, fileH, &bytesRx, verbose)) ) {
				recFrames++;
				if(verbose)
					printf("   ==> OK: Frame %4d, length 0x%04X;\n\n",
					        recFrames, bytesRx);
			} else if(error == ERR_OSS_TIMEOUT) {
				error = 0;
			} else if( (error == M75_ERR_RX_BREAKABORT) ||
					   (error == M75_ERR_RX_QFULL) ||
					   (error == M75_ERR_RX_OVERFLOW)) {
				printf("*** %s ***\n",M_errstring( error ) );
				error = 0;
				numerrors++;
			} else {
				printf("*** %s ***\n",M_errstring( error ) );
				numerrors++;
			}
		}
	}

	if( fileH )
		fclose(fileH);
	if(!numerrors)
		printf("  TEST_Tx ==> OK, received %d frames;\n", recFrames);
	else
		printf( "  TEST_Tx ==> completed with errors\n"
				"              received %d frames;\n"
				"              detected %d errors;\n", recFrames, numerrors);
	printf("    ====================\n");
	return( error );

M75_ERR:
    errShow();
    printf("    => Error\n");
	if( fileH )
		fclose(fileH);
    return( 1 );
}

/**************************** m75_getFrame *********************************
 *
 *  Description:  Get frame(s) from driver.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  fdHdl   device handle
 *                fileH   file handle for writing frames
 *                verbose flag, detailed output to stdout
 *
 *  Output.....:  return  number of bytes received or -1
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
int32 m75_getFrame(MDIS_PATH fdHdl, FILE *fileH, int32 *nbrOfBytesReceived, u_int8 verbose){

	int32	i;
	u_int8 *m75_RxBuf;
	int32  syserror;

	m75_RxBuf = (u_int8*) malloc( EXT_FIFO_SIZE );
	if( (*nbrOfBytesReceived =
			M_getblock(fdHdl, m75_RxBuf, EXT_FIFO_SIZE)) < 0 ) {
		syserror = UOS_ErrnoGet();
		return( syserror );
	} else {
		if( fileH ) {
			/* print frame to file */
			fprintf(fileH, "#$\n"); /* opening flag */
			for(i=0; i<*nbrOfBytesReceived; i++)
				fprintf(fileH, "0x%02X\n", m75_RxBuf[i]);
			fprintf(fileH, "#$\n"); /* closing flag */
		}

		if( verbose ) {
			printf("==> Frame receive: size = 0x%04X;\n", *nbrOfBytesReceived);
			for(i=0; i < *nbrOfBytesReceived; i++){
				printf("%s\t0x%02X",
					( !(i%8) && (i!=0) ) ? "\n" : "", *(m75_RxBuf+i));
			}
			printf("\n");
		}
	}
	free(m75_RxBuf);
	return( ERR_SUCCESS );

}

