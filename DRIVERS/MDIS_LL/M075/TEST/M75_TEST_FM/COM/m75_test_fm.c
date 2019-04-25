/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         \file m75_test_fm.c
 *
 *       \author Christian.Schuster@men.de
 *        $Date: 2009/07/14 17:47:27 $
 *    $Revision: 1.2 $
 *
 *  Description: Test for M75 hardware and driver,
 *               Transmitting and Receiving frames in FM mode
 *               Only valid for 04M75_04 hardware
 *               (04M75_03 modified to use DPLL for full clock recovery)
 *
 *     Required: -
 *     Switches: M75_TEST_FM; M75_RETURN_RX;
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m75_test_fm.c,v $
 * Revision 1.2  2009/07/14 17:47:27  cs
 * R:1. Porting to MDIS5
 * M:1.a) use MDIS_PATH for MDIS paths
 *     b) fix SigHandler Prototype (prepend __MAPILIB)
 *
 * Revision 1.1  2004/12/29 19:34:59  cs
 * Initial Revision
 *
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2004 by MEN Mikro Elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static char *RCSid="$Id: m75_test_fm.c,v 1.2 2009/07/14 17:47:27 cs Exp $\n";

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
#define TEST_MAXFRAMES	500		/* Maximum number of packets for M75_test_q */
#define EXT_FIFO_SIZE	2048	/* Size of external FIFO on M75 HW */
#define DEFAULT_FILE	"m75_data.txt"
#define DEFAULT_BRGEN_TCONST 0x00 /* default timeconstant for BR Gen */
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


#define CODE_NRZ  0x00
#define CODE_NRZI 0x20
#define CODE_FM1  0x40
#define CODE_FM0  0x60
#define CODE_CLR  0x60
#define MARK_IDLE 0x08

#define CLK_TRXC_XTAL  0x00
#define CLK_TRXC_TXCLK 0x01
#define CLK_TRXC_BRGEN 0x02
#define CLK_TRXC_DPLL  0x03

#define CLK_TRXC_OUT 0x04
#define CLK_TRXC_IN  0x00

#define CLK_TX_RTXC  0x00
#define CLK_TX_TRXC  0x08
#define CLK_TX_BRGEN 0x10
#define CLK_TX_DPLL  0x18

#define CLK_RX_RTXC  0x00
#define CLK_RX_TRXC  0x20
#define CLK_RX_BRGEN 0x40
#define CLK_RX_DPLL  0x60

#define DPLL_EN_SEARCH 0x20
#define DPLL_DISABLE   0x60
#define DPLL_CLK_BRGEN 0x80
#define DPLL_CLK_RTXC  0xA0
#define DPLL_MODE_FM   0xC0
#define DPLL_MODE_NRZI 0xE0
#define WR14_BRGEN_EN  0x01



#define MSECDIFF(basemsec)  (UOS_MsecTimerGet() - basemsec)

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

static int32 m75_test_Tx(MDIS_PATH fdHdl,
						 int32 numpak,
						 int32 tconst,
						 int32 tout,
						 u_int8 ch,
						 char *filename,
						 u_int8 verbose);
static int32 m75_test_Rx(MDIS_PATH fdHdl,
						 int32  tconst,
						 int32 tout,
						 u_int8 useSig,
						 u_int8 ch,
						 char *filename,
						 u_int8 verbose);
static int32 m75_getFrame(MDIS_PATH fdHdl,
						  FILE *fileH,
						  int32 *nbrOfBytesReceived,
						  u_int8 *m75_RxBuf,
						  u_int8 verbose);
static int32 m75_verifyFrame(u_int8 *frame1,
							 u_int8 *frame2,
							 u_int32 len,
							 u_int8 verbose);

static void m75_printFrame( FILE *fileH,
							u_int8 *frame,
							u_int32 len);




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
	printf(	"\n\nUsage: m75_test [<opts>] <device> [<opts>]\n"
			"Function: M75 hardware access\n"
			"Options:\n"
			"    device          device name (M75)               [M75_1]\n"
			"    -n=<n>(hex)     nr of frames for test           [%d]\n"
			"    -c=<ch>         channel (0=A,1=B)               [1] \n"
			"    -b=<tc>(hex)    BR Gen timeconstant             [0]\n"
			"    -t              flag, test Tx                   [no]\n"
			"    -r              flag, test Rx                   [no]\n"
			"                    if file .\\conf.rx exists,\n"
			"                    the parameters from this file are used\n"
			"    -m              flag, use Signals for Rx        [no]\n"
			"                    once started, press any key to abort\n"
			"    -w=<tout>       SET-, GETBLOCK_TOUT(-1 or msec) [0] \n"
			"    -f[=<filename>] optional, file with/for Data    [no]\n"
			"    -v              verbose                         [no]\n",
			TEST_MAXFRAMES);
	printf("\n(c) 2004 by MEN mikro elektronik GmbH\n%s\n\n",RCSid);
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
 *	    -c=<ch>         channel (0=A,1=B)               [0]
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
			tx, rx, ch,
			useSig, useFile;
    MDIS_PATH fd = 0;
	int32	error=0, open=0;
	char	*filename, *errstr;

    if( argc < 2){
        usage();
        return 1;
    }

    printf("=========================\n");
    printf("%s", RCSid );
    printf("=========================\n");
    printf("welcome to the world of magic\n");

	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		device = "M75_1";
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

	ch = ((str = UTL_TSTOPT("c=")) ? atoi(str) : 0);
	ch+=1;
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

	tx        = (UTL_TSTOPT("t") ? 1 : 0);
	rx        = (UTL_TSTOPT("r") ? 1 : 0);
	useSig    = (UTL_TSTOPT("m") ? 1 : 0);
	verbose	  = (UTL_TSTOPT("v") ? 1 : 0);

    printf("\nm75_test channel=%s \tTimeConst=0x%04X \tframes=%d \twait=%d\n"
		   "         tx=%s        \trx=%s            \tverbose=%s\n\n\n",
		ch==1 ? "A" : ch==2 ? "B" : "AB",
		tconst, numframes, wait,
		tx  ? "yes" : "no",
		rx  ? "yes" : "no",
		verbose ? "yes" : "no" );

	if( !( tx | rx) ) {
		printf("please specify a test\n");
		usage();
		return( 1 );
	}

	/* open device */
	if( verbose )
		printf("    M_open(%s)\n", device);
	if( (fd = M_open( device )) < 0 )
		goto M75_ERR;
	open = 1;

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

/**************************** m75_test_Tx **********************************
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

	int32 val, pak,	i;
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

#ifdef M75_RETURN_RX
	int32	bytesRx=0;
	u_int32 recFrames=0;
	u_int8 *m75_RxBuf = NULL;
	int32	getBlockTout=0x500,
			rxFrameSize=EXT_FIFO_SIZE,
			rxFrameNum =0x10,
			recerror = 0,
			vererror = 0;
#endif

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

#ifdef M75_TEST_FM

	if( verbose ){
		printf("FM1 encoding, using the DPLL\n");
	}

    /* disable DPLL and BRGen */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | DPLL_DISABLE) );

	/* WR10: Coding mode FM1, mark on idle (needed by DPLL?) */
    M_setstat( fdHdl, M75_SCC_REG_10, ((WR10_DEFAULT & ~CODE_CLR) |
										CODE_FM1 ));

	/* WR11: Clock Mode */
    M_setstat( fdHdl, M75_SCC_REG_11, CLK_RX_DPLL |
									  CLK_TX_RTXC |
									  CLK_TRXC_OUT |
									  CLK_TRXC_BRGEN );

	/* Then handle the DPLL */
	/* set DPLL clock source */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | DPLL_CLK_BRGEN) );
	/* set DPLL mode to FM */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | DPLL_MODE_FM) );
	/* enable BRGen */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | WR14_BRGEN_EN) );
	/* put DPLL in search mode */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1F) |
										DPLL_EN_SEARCH |
										WR14_BRGEN_EN) );

#else
	if( verbose ){
		printf("NRZ encoding, Rx external clock\n");
	}
	/* no more settings necessary for this mode */
#endif

	/* Tx parameters */
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

	/* enable IRQs */
	M_setstat(fdHdl, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(fdHdl, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */

#ifdef M75_RETURN_RX
	/* Rx parameters */
	/* wait for returned frame */
	M_setstat(fdHdl, M75_GETBLOCK_TOUT, getBlockTout);

	/* enable receiver */
	M_setstat( fdHdl, M75_SCC_REG_03, 0xd9 );

	while( M_setstat(fdHdl, M75_MAX_RXFRAME_NUM, rxFrameNum) ){
		printf( "*** Error *** set M75_MAX_RXFRAME_NUM:"
				" not enough memory for %04X frames",
				rxFrameNum);
		if( rxFrameNum == 1 )
			goto M75_ERR; /* this shouldn't happen on a normal system */

		rxFrameNum /= 2;
		printf( "              trying to allocate for %04X frames\n",
				rxFrameNum);
	}
	if( M_setstat(fdHdl, M75_MAX_RXFRAME_SIZE, rxFrameSize) ) {
		printf( "*** Error *** set M75_MAX_RXFRAME_SIZE:"
				" not enough memory for %04X frames of %04X bytes, aborting\n",
				rxFrameNum, rxFrameSize);
		goto M75_ERR;
	}
	if( verbose )
		printf("Rx enabled\n");
#endif

#ifdef M75_TEST_FM
	/* enable Transmitter (DPLL needs signal before frames in order to sync) */
	M_setstat( fdHdl, M75_SCC_REG_05, WR05_DEFAULT | 0x08 );
#endif

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

#ifdef M75_RETURN_RX
		printf("  Rx Parameters:\n");
		M_getstat(fdHdl, M75_GETBLOCK_TOUT, &val);
		printf("    M75_GETBLOCK_TOUT:    0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_RXFRAME_NUM, &val);
		printf("    M75_MAX_RXFRAME_NUM:  0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_RXFRAME_SIZE, &val);
		printf("    M75_MAX_RXFRAME_SIZE: 0x%04X\n", val);
#endif
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

#ifdef M75_RETURN_RX
	/* allocate Rx buffer */
	m75_RxBuf = (u_int8*) malloc( EXT_FIFO_SIZE );
#endif


	for(pak=0; pak<numpacks; pak++) {

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
							printf( "*** ERROR   frame %d too big"
									" (maxFrameSize = %d)\n",
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
			*bufp++ = (u_int8)pak;
			for(i=1; i<fsize; i++)
				*bufp++ = (u_int8)i;
		}

		/* send frame */
		if( (nbrOfBytesSent = M_setblock(fdHdl, m75_TxBuf, fsize)) == fsize ) {
			fnum++;
			UOS_Delay(50); /* allow some time for the frame to be transmitted */
		} else {
			errShow();
			error++;
			break;
		}

		if( verbose )
			printf("    M_setblock(%d) \tnbrOfBytesSent=%d pak=0x%02x\n",
					fnum, nbrOfBytesSent, pak);

#ifdef M75_RETURN_RX
		/* receive returned frame */
		if(!(error = m75_getFrame(fdHdl, NULL, &bytesRx, m75_RxBuf, verbose))) {
			recFrames++;
			if(verbose)
				printf("   ==> OK: Received Frame %4d, length 0x%04X;\n\n",
				        recFrames, bytesRx);
			if( m75_verifyFrame(m75_TxBuf, m75_RxBuf, fsize, verbose) )
				vererror++;
		} else {
			recerror++;
			printf( "receive frame:\n"
					"*** %s ***\n", M_errstring( error ) );
			if( (error == M75_ERR_RX_BREAKABORT) ||
				(error == M75_ERR_RX_QFULL) ||
				(error == M75_ERR_RX_OVERFLOW)) {
					error = 0;
			}
		}
#endif
	}
	free(m75_TxBuf);

	if( fileH )
		fclose(fileH);
	if(!error)
		printf("  TEST_Tx ==> sent %d frames\n", fnum);
#ifdef M75_RETURN_RX
	if(recerror)
		printf( "  TEST RETURN_RX: sent:      %d frames\n"
				"                  received:  %d frames\n",
				fnum, fnum - recerror);
	if(vererror)
		printf( "                  erroneous: %d frames\n",
				fnum, fnum - recerror);
	if( m75_RxBuf )
		free(m75_RxBuf);
#endif
	printf("    ====================\n");
	/* give driver the chance to empty Tx Buffer */
	UOS_Delay(1000);
	return( error );

M75_ERR:

    errShow();
    printf("    => Error\n");
	free(m75_TxBuf);
#ifdef M75_RETURN_RX
	if( m75_RxBuf )
		free(m75_RxBuf);
#endif
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
	u_int8 *m75_RxBuf = NULL;

#ifdef M75_RETURN_RX
	int32 txFrameNum = 5,
		  txFrameSize=EXT_FIFO_SIZE,
		  nbrOfBytesSent = 0,
		  setBlockTout = 0x200;
#endif

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
			goto M75_ERR;
		}
	if( fileH )
		fprintf(fileH, "#Received Frames\n");

	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */


#ifdef M75_TEST_FM

	printf("FM1 encoding, using the DPLL\n");

	/* disable DPLL and BRGen */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | DPLL_DISABLE) );

	/* BR Gen Timeconstant */
	M_setstat( fdHdl, M75_BRGEN_TCONST, brGen_TConst );

	/* WR10: Coding mode */
	M_setstat( fdHdl, M75_SCC_REG_10, ((WR10_DEFAULT & ~CODE_CLR) | CODE_FM1) );

	/* WR11: Clock Mode */
	M_setstat( fdHdl, M75_SCC_REG_11, CLK_RX_DPLL |
									  CLK_TX_RTXC |
									  CLK_TRXC_OUT |
									  CLK_TRXC_BRGEN );

	/* Then handle the DPLL */
	/* set DPLL clock source */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | DPLL_CLK_BRGEN) );
	/* set DPLL mode to FM */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | DPLL_MODE_FM) );
	/* enable BRGen */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1E) | WR14_BRGEN_EN) );
	/* put DPLL in search mode */
	M_setstat( fdHdl, M75_SCC_REG_14, ((WR14_DEFAULT & 0x1F) |
										DPLL_EN_SEARCH |
										WR14_BRGEN_EN) );

	printf("ready to receive \n");

#else
	printf("NRZ encoding, Rx external clock\n");

	/* BR Gen Timeconstant */
	/* irrelevant since data is synchronized through the transmit clock (RTxC)
	M_setstat( fdHdl, M75_BRGEN_TCONST, brGen_TConst );
	*/
#endif

	/* enable IRQs */
	M_setstat(fdHdl, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(fdHdl, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */

	/* wait for returned frame */
	M_setstat(fdHdl, M75_GETBLOCK_TOUT, getBlockTout);

	/* enable Rx */
	M_setstat( fdHdl, M75_SCC_REG_03, 0xd9 );

#ifdef M75_RETURN_RX
	/* enable Transmitter (DPLL needs signal before frames in order to sync) */
	M_setstat( fdHdl, M75_SCC_REG_05, WR05_DEFAULT | 0x08 );
#endif


#ifdef M75_RETURN_RX
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
#endif

	while( M_setstat(fdHdl, M75_MAX_RXFRAME_NUM, rxFrameNum) ){
		printf( "*** Error *** set M75_MAX_RXFRAME_NUM:"
				" not enough memory for %04X frames",
				rxFrameNum);
		if( rxFrameNum == 1 )
			goto M75_ERR; /* this shouldn't happen on a normal system */
		rxFrameNum /= 2;
		printf( "              trying to allocate for %04X frames\n",
				rxFrameNum);
	}
	if( M_setstat(fdHdl, M75_MAX_RXFRAME_SIZE, rxFrameSize) ) {
		printf( "*** Error *** set M75_MAX_RXFRAME_SIZE:"
				" not enough memory for %04X frames of %04X bytes, aborting\n",
				rxFrameNum, rxFrameSize);
		goto M75_ERR;
	}


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

#ifdef M75_RETURN_RX
		printf("  Tx Parameters:\n");
		M_getstat(fdHdl, M75_SETBLOCK_TOUT, &val);
		printf("    M75_SETBLOCK_TOUT:    0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_TXFRAME_NUM, &val);
		printf("    M75_MAX_TXFRAME_NUM:  0x%04X\n", val);
		M_getstat(fdHdl, M75_MAX_TXFRAME_SIZE, &val);
		printf("    M75_MAX_TXFRAME_SIZE: 0x%04X\n", val);
#endif
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

	/* allocate Rx buffer */
	m75_RxBuf = (u_int8*) malloc( EXT_FIFO_SIZE );

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
											   &bytesRx, m75_RxBuf, verbose)) ) {
						recFrames++;
						if(verbose)
							printf("   ==> OK: Frame %4d, length 0x%04X;\n\n",
						       recFrames, bytesRx);
#ifdef M75_RETURN_RX
						/* return frame to sender */
						if( (nbrOfBytesSent = M_setblock(fdHdl,
														 m75_RxBuf,
														 bytesRx)) != bytesRx ) {
							errShow();
							error++;
						}
#endif
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
			if( !(error = m75_getFrame(fdHdl, fileH, &bytesRx,
									   m75_RxBuf, verbose)) ) {
				recFrames++;
				if(verbose)
					printf("   ==> OK: Frame %4d, length 0x%04X;\n\n",
					        recFrames, bytesRx);
#ifdef M75_RETURN_RX
				/* return frame to sender */
				if( (nbrOfBytesSent = M_setblock(fdHdl, m75_RxBuf, bytesRx))
					!= bytesRx ) {
					errShow();
					error++;
				}
#endif
			} else if(error == ERR_OSS_TIMEOUT) {
				error = 0;
			} else if( (error == M75_ERR_RX_BREAKABORT) ||
					   (error == M75_ERR_RX_QFULL) ||
					   (error == M75_ERR_RX_OVERFLOW)) {
				u_int8 errstr[32];
				if(error == M75_ERR_RX_BREAKABORT )
					strcpy( errstr, "M75_ERR_RX_BREAKABORT" );
				else if ( error == M75_ERR_RX_QFULL )
					strcpy( errstr, "M75_ERR_RX_QFULL");
				else
					strcpy( errstr, "M75_ERR_RX_OVERFLOW");

				printf("*** ERROR (LL) 0x%04x: %s ***\n", error, errstr );
				error = 0;
				numerrors++;
			} else {
				printf("*** %s ***\n",M_errstring( error ) );
				numerrors++;
			}
		}
	}

	free(m75_RxBuf);
	if( fileH )
		fclose(fileH);
	if(!numerrors)
		printf("  TEST_Rx ==> OK, received %d frames;\n", recFrames);
	else
		printf( "  TEST_Rx ==> completed with errors\n"
				"              received %d frames;\n"
				"              detected %d errors;\n", recFrames, numerrors);
	printf("    ====================\n");
	return( error );

M75_ERR:
	if(m75_RxBuf)
		free(m75_RxBuf);
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
int32 m75_getFrame(MDIS_PATH fdHdl, FILE *fileH, int32 *nbrOfBytesReceived,
				   u_int8 *m75_RxBuf, u_int8 verbose)
{

	int32  syserror;

	if( (*nbrOfBytesReceived =
			M_getblock(fdHdl, m75_RxBuf, EXT_FIFO_SIZE)) < 0 ) {
		syserror = UOS_ErrnoGet();
		return( syserror );
	} else {
		if( fileH ) {
			/* print frame to file */
			m75_printFrame(	fileH, m75_RxBuf, *nbrOfBytesReceived);
		}

		if( verbose ) {
			m75_printFrame(	NULL, m75_RxBuf, *nbrOfBytesReceived);
		}
	}
	return( ERR_SUCCESS );

}


/**************************** m75_verifyFrame ********************************/
/*! Verify frames in buffers
 *
 *  \param  frame1	\IN		buffer with first frame
 *  \param  frame2	\IN		buffer with second frame
 *  \param  len		\IN		length of frames to verify
 *  \param  verbose	\IN		flag, detailed output to stdout
 *
 *	\return	0 on identical frames or number of byte where failed
 ****************************************************************************/
int32 m75_verifyFrame(	u_int8 *frame1,
						u_int8 *frame2,
						u_int32 len,
						u_int8 verbose)
{

	u_int32	i;

	for(i=0; i < len; i++) {
		if( frame1[i] != frame2[i] ) {
			printf("     ERROR: Frame verification failed at byte %d\n", i);
			if( verbose ) {
				printf("==> Frame 1:\n");
				m75_printFrame(	NULL, frame1, len);
				printf("\n==> Frame 2:\n");
				m75_printFrame(	NULL, frame2, len);
			}
			return(i);
		}
	}
	if( verbose )
		printf("==> Verification O.K.\n");
	return(0);
}
/**************************** m75_printFrame ********************************/
/*! Print frame to file or stdout
 *
 *  \param  fileH	\IN		file to print frame to, may be passed as NULL
 *  \param  frame	\IN		buffer with frame to print
 *  \param  len		\IN		length of frame
 *
 *	\return	void
 ****************************************************************************/
void m75_printFrame(	FILE *fileH,
						u_int8 *frame,
						u_int32 len)
{
	u_int32	i;
	if( fileH ) {
		/* print frame to file */
		fprintf(fileH, "#$\n"); /* opening flag */
		for(i = 0; i < len; i++)
			fprintf(fileH, "0x%02X\n", frame[i]);
		fprintf(fileH, "#$\n"); /* closing flag */
	} else {
		printf("==> Frame size = 0x%04X;\n", len);
		for(i = 0; i < len; i++){
			printf("%s  %02X",
				( !(i%8) && (i!=0) ) ? "\n" : "", frame[i]);
		}
		printf("\n");
	}
	return;
}

