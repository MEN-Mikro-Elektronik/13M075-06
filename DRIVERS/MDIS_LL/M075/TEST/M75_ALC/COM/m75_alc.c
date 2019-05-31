/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         \file m75_alc.c
 *
 *       \author Christian.Schuster@men.de
 *
 *  Description: Test simulating the hardware stimulation by certain customer
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
#include <string.h>

#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/mdis_err.h>

#include <MEN/m75_drv.h>

/* #include <MEN/dbg.h> */

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*-----------------------------------------+
|  TYPEDEFS                                |
+------------------------------------------*/

/*-----------------------------------------+
|  DEFINES & CONST                         |
+------------------------------------------*/
#define TEST_MAXFRAMES	500		/* Maximum number of packets for M75_test_q */
#define EXT_FIFO_SIZE	2048	/* Size of external FIFO on M75 HW */

static const int32 OFF = 0;
static const int32 ON  = 1;

static const int32 T_OK = 0;
static const int32 T_ERROR = 1;



/*-----------------------------------------+
|  GLOBALS                                 |
+------------------------------------------*/

/*-----------------------------------------+
|  STATICS                                 |
+------------------------------------------*/

/*-----------------------------------------+
|  PROTOTYPES                              |
+------------------------------------------*/
static void errShow( void );
static void usage(void);

static int32 m75_alc_a( MDIS_PATH fdHdl );
static int32 m75_alc_b( MDIS_PATH fdHdl );


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
	printf("\n\nUsage: m75_alc [<opts>] <device> [<opts>]\n");
	printf("Function: M75 hardware access like customer\n");
	printf("Options:\n");
	printf("    device        device name (M75)           [M75_1]\n");
	printf("    -a            test channel A              [no]\n");
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
 *                argv[1]   device name
 *
 *  Output.....:  return    0   if no error
 *                          1   if error
 *
 *  Globals....:  -
 ****************************************************************************/
 int main( int argc, char *argv[ ] )
 {
	char *device;
	int32  n, testa, testb;
    MDIS_PATH	fd = 0;
	int32	error;
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

	testa        = (UTL_TSTOPT("a") ? 1 : 0);
	testb        = (UTL_TSTOPT("b") ? 1 : 0);

    printf("\nm75_alc testa=%s testb=%s\n",
		testa ? "yes" : "no",
		testb ? "yes" : "no");

	if( !(testa | testb) ) {
		printf("please specify a test\n");
		usage();
		return( 1 );
	}

	/* open device */
	printf("    M_open(%s)\n", device);
	if( (fd = M_open( device )) < 0 ) goto M75_ERR;

	/* test channel A */
	if(testa){
		printf("=========================\n");
		if(	(error = m75_alc_a( fd )) )
			printf("    ==> ERROR m75_alc_a\n");
	}

	/* test channel B */
	if(testb){
		printf("=========================\n");
		if(	(error = m75_alc_b( fd )) )
			printf("    ==> ERROR m75_alc_b: %d\n", error);
	}

	printf("\n=========================\n");
	if( M_close( fd ) ) goto M75_ERR;

	printf("    M_close\n");

	return 0;

M75_ERR:
    errShow();
    printf("    => Error\n");
    M_close( fd );
    return( 1 );
}

/* N:\Artikel\04\04m075-\PLD Sources\IC1_sicherung_18_02_04\IC33\log_files\Sdlc\	*
 * m075_likeHW\SCC1\data like hw\mmm_stimuli.txt									*/
int32 m75_alc_a(MDIS_PATH fdHdl)
{
	int32 val,ch = 0x00;
	int32	i;
	u_int8 m75_Txbuf[EXT_FIFO_SIZE],
		   m75_Rxbuf[EXT_FIFO_SIZE],
		   *bufp = m75_Txbuf;
	int32 nbrOfBytesSent1, nbrOfBytesReceived1,
		  nbrOfBytesSent2, nbrOfBytesReceived2,
		  numBytes;
	u_int32 error=0;

	printf("\n    m75_alc_a\n");

	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */

	/* enable IRQs */
	M_setstat(fdHdl, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(fdHdl, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */

	M_getstat( fdHdl, M75_SCC_REG_00, &val );
	/* ww 04 0009	-- address wr9
	 * ww 04 00a3	-- wr9 */ /* ch_res A, INTACK, NV, VIS */
	M_setstat( fdHdl, M75_SCC_REG_09, 0xa3 );
	/* ww 12 0000	-- cmr */ /* front I/O, always sync  */
	M_setstat( fdHdl, M75_IO_SEL, 0x00 );
	/* ww 04 0004	-- address wr04
	 * ww 04 0020	-- tx/rx config */
	M_setstat( fdHdl, M75_SCC_REG_04, 0x20 );
	/* ww 04 0001	-- wr01
	 * ww 04 0078 */
	M_setstat( fdHdl, M75_SCC_REG_01, 0x78 );
	/* ww 04 0002
	 * ww 04 0000	-- wr02 */
	M_setstat( fdHdl, M75_SCC_REG_02, 0x00 );
	/* ww 04 0003
	 * ww 04 00d8	 */
	M_setstat( fdHdl, M75_SCC_REG_03, 0xd8 );
	/* ww 04 0005	-- address wr05
	 * ww 04 0061	-- tx config */
	M_setstat( fdHdl, M75_SCC_REG_05, 0x61 );
	/* ww 04 0009	-- address wr09
	 * ww 04 0023	--  */
	M_setstat( fdHdl, M75_SCC_REG_09, 0x23 ); /* wozu dieses ???*/
	/* ww 04 0007	-- address wr07
	 * ww 04 007e	-- wr07 */
	M_setstat( fdHdl, M75_SCC_REG_07, 0x7e );
	/* ww 04 000a	-- address wr10
	 * ww 04 0080	-- config tx/rx */
	M_setstat( fdHdl, M75_SCC_REG_10, 0x80 );
	/* ww 04 000b	-- address wr11
	 * ww 04 0016	-- set clock mode */
	M_setstat( fdHdl, M75_SCC_REG_11, 0x16 );
	/* ww 04 000f	-- address wr15
	 * ww 04 0005	-- fifo enable/esi enable */
/*	M_setstat( fdHdl, M75_SCC_REG_15, 0x05 );*/
	/* ww 04 0007	-- address wr07'
	 * ww 04 0073	-- set auto eom reset */
	M_setstat( fdHdl, M75_SCC_REG_7P, 0x73 );
	/* ww 04 000e	-- address wr14
	 * ww 04 0006	-- BRG  */
	M_setstat( fdHdl, M75_SCC_REG_14, 0x06 );
	/* ww 04 000c	-- address wr12
	 * ww 04 0071	--  */
	M_setstat( fdHdl, M75_SCC_REG_12, 0x71 );
	/* ww 04 000d	-- address wr13
	 * ww 04 0000	--  */
	M_setstat( fdHdl, M75_SCC_REG_13, 0x00 );
	/* ww 04 000e	-- address wr14
	 * ww 04 0007	-- BRG enable */
	M_setstat( fdHdl, M75_SCC_REG_14, 0x07 );
	/* ww 04 000f	-- address wr15
	 * ww 04 00c5	--  */ /* Break/Abort IE, Tx Underr/EOM IE, Status FIFO, WR7' */
	M_setstat( fdHdl, M75_SCC_REG_15, 0xc5 );
	/* ww 0c 0000 */
	M_setstat(fdHdl, M75_TXEN, 0x00 );
	/* ww 04 0000
	 * ww 04 0010  	-- reset ext stat Int */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0011
	 * ww 04 00f9 */
	M_setstat( fdHdl, M75_SCC_REG_01, 0xf9 ); /* Rx Int on spec. cond. only, Ext Int En */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0000
	 * rw 04 */
	M_getstat( fdHdl, M75_SCC_REG_00, &val );
	printf("    RR00 = 0x%04X\n", val);
	/* ww 10 0005   -- fifo reset register */
	M_setstat(fdHdl, M75_FIFO_RESET, 0x05 );
	/* ww 04 0013
	 * ww 04 00d9  	-- wr03 */ /*  8bits/char, Rx CRC En, Rx En */
	M_setstat( fdHdl, M75_SCC_REG_03, 0xd9 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0005
	 * ww 04 0069  	-- wr05 */ /* 8bits/char, Tx En, Tx CRC En */
	M_setstat( fdHdl, M75_SCC_REG_05, 0x69 );
	/* ww 04 0011
	 * ww 04 00f9   -- wr01*/
	M_setstat( fdHdl, M75_SCC_REG_01, 0xf9 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0009
	 * ww 04 002b  	-- wr09 */
	M_setstat( fdHdl, M75_SCC_REG_09, 0x2b );

	/* if Rx queue empty, wait */
	M_setstat(fdHdl, M75_GETBLOCK_TOUT, 0x200);

	/* init buffer */
	for(i=1; i<=EXT_FIFO_SIZE; i++)
		*bufp++ = (u_int8)i;

	/* send frame */
	numBytes = 0x0A;
	if( (nbrOfBytesSent1 = M_setblock(fdHdl,m75_Txbuf,numBytes)) != numBytes )
		error++;
	printf("    M_setblock(%04X) \tnbrOfBytesSent=%04X\n", numBytes, nbrOfBytesSent1);

	/* send frame */
	numBytes = 0x0C;
	if( (nbrOfBytesSent2 = M_setblock(fdHdl,m75_Txbuf,numBytes)) != numBytes )
		error++;
	printf("    M_setblock(%04X) \tnbrOfBytesSent=%04X\n", numBytes, nbrOfBytesSent2);

	/* receive frame */
	if( (nbrOfBytesReceived1 = M_getblock(fdHdl,m75_Rxbuf,EXT_FIFO_SIZE))
		!= nbrOfBytesSent1+2 )
		error++;
	printf("    M_getblock(%d) \tnbrOfBytesReceived=%d\n", EXT_FIFO_SIZE, nbrOfBytesReceived1);

	if( !error ){
		for(i=0; i < nbrOfBytesSent1; i++){
			if( *(m75_Rxbuf+i) != *(m75_Txbuf+i) ) error++;
		}
	}

	if( !error )
		printf("        ==> OK Frame send/receive\n");
	else {
		printf("\nError: Send/Receive Frame, channel %d\n"
			"\tsent bytes: %04X\n\tread bytes: %04X\n",
			ch, nbrOfBytesSent1, nbrOfBytesReceived1);

		printf("Received Frame:\n");
		for(i=0; i < nbrOfBytesReceived1; i++){
			printf("\t0x%02X", *(m75_Rxbuf+i));
			if( (i!=0) && !(i%8) )
				printf("\n");
		}
		printf("\n");
	}

	/* receive frame */
	if( (nbrOfBytesReceived2 = M_getblock(fdHdl,m75_Rxbuf,EXT_FIFO_SIZE))
		!= nbrOfBytesSent2+2 )
		error++;
	printf("    M_getblock(%d) \tnbrOfBytesReceived=%d\n", EXT_FIFO_SIZE, nbrOfBytesReceived2);

	if( !error ){
		for(i=0; i < nbrOfBytesSent2; i++){
			if( *(m75_Rxbuf+i) != *(m75_Txbuf+i) ) error++;
		}
	}

	if( !error )
		printf("        ==> OK Frame send/receive\n");
	else {
		printf("\nError: Send/Receive Frame, channel %d\n"
			"\tsent bytes: %04X\n\tread bytes: %04X\n",
			ch, nbrOfBytesSent2, nbrOfBytesReceived2);

		printf("Received Frame:\n");
		for(i=0; i < nbrOfBytesReceived2; i++){
			printf("\t0x%02X", *(m75_Rxbuf+i));
			if( (i!=0) && !(i%8) )
				printf("\n");
		}
		printf("\n");
	}

	UOS_Delay(800);


#if 0 /* interrupt handling is done by the drivers interrupt service routine */
	/* ww 0c 0080	-- enable fifos */
	M_setstat( fdHdl, M75_TXEN, 0x01 );

	/* -- wait for interrupt */
	/* ha 00			*/
	UOS_Delay(800);
	/* ww 04 0003
	 * rw 04	-- rr03 */
	M_getstat( fdHdl, M75_SCC_REG_03, &val );
	printf("    RR03 = 0x%04X\n", val);
	/* ww 06 0002
	 * rw 06	-- read modified vector */
	M_setstat(fdHdl, M_MK_CH_CURRENT, 0x01);    /* set current channel to B*/
	M_getstat( fdHdl, M75_SCC_REG_02, &val );
	printf("    RR02 = 0x%04X (channel B)\n", val);
	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* reset current channel */
	/* ww 04 0000
	 * rw 04			*/
	M_getstat( fdHdl, M75_SCC_REG_00, &val );
	printf("    RR00 = 0x%04X\n", val);
	/* ww 04 0000
	 * ww 04 0010	-- reset ext stat irq */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x00 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0000
	 * ww 04 0038	-- reset ius */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x00 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x38 );
	/* ww 04 0003
	 * rw 04		-- still interrupt */
	M_getstat( fdHdl, M75_SCC_REG_03, &val );
	printf("    RR03 = 0x%04X\n", val);
#endif
	return( 0 );

}

int32 m75_alc_b(MDIS_PATH fdHdl)
{
	int32 val,ch = 0x01;
	int32	i;
	u_int8 m75_Txbuf[EXT_FIFO_SIZE],
		   m75_Rxbuf[EXT_FIFO_SIZE],
		   *bufp = m75_Txbuf;
	int32 nbrOfBytesSent1, nbrOfBytesReceived1,
		  nbrOfBytesSent2, nbrOfBytesReceived2,
		  numBytes;
	u_int32 error=0;



	printf("\n    m75_alc_b\n");

	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* set current channel */

	/* enable IRQs */
	M_setstat(fdHdl, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(fdHdl, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */

	/* ww 04 0009	-- address wr9
	 * ww 04 00a3	-- wr9 */ /* ch_res A, INTACK, NV, VIS */
	M_setstat( fdHdl, M75_SCC_REG_09, 0x63 );
	/* ww 12 0000	-- cmr */ /* front I/O, always sync  */
	M_setstat( fdHdl, M75_IO_SEL, 0x00 );
	/* ww 04 0004	-- address wr04
	 * ww 04 0020	-- tx/rx config */
	M_setstat( fdHdl, M75_SCC_REG_04, 0x20 );
	/* ww 04 0001	-- wr01
	 * ww 04 0078 */
	M_setstat( fdHdl, M75_SCC_REG_01, 0x78 );
	/* ww 04 0002
	 * ww 04 0000	-- wr02 */
	M_setstat( fdHdl, M75_SCC_REG_02, 0x00 );
	/* ww 04 0003
	 * ww 04 00d8	 */
	M_setstat( fdHdl, M75_SCC_REG_03, 0xd8 );
	/* ww 04 0005	-- address wr05
	 * ww 04 0061	-- tx config */
	M_setstat( fdHdl, M75_SCC_REG_05, 0x61 );
	/* ww 04 0009	-- address wr09
	 * ww 04 0023	--  */
	M_setstat( fdHdl, M75_SCC_REG_09, 0x23 ); /* wozu dieses ???*/
	/* ww 04 0007	-- address wr07
	 * ww 04 007e	-- wr07 */
	M_setstat( fdHdl, M75_SCC_REG_07, 0x7e );
	/* ww 04 000a	-- address wr10
	 * ww 04 0080	-- config tx/rx */
	M_setstat( fdHdl, M75_SCC_REG_10, 0x80 );
	/* ww 04 000b	-- address wr11
	 * ww 04 0016	-- set clock mode */
	M_setstat( fdHdl, M75_SCC_REG_11, 0x16 );
	/* ww 04 000f	-- address wr15
	 * ww 04 0005	-- fifo enable/esi enable */
/*	M_setstat( fdHdl, M75_SCC_REG_15, 0x05 );*/
	/* ww 04 0007	-- address wr07'
	 * ww 04 0073	-- set auto eom reset */
	M_setstat( fdHdl, M75_SCC_REG_7P, 0x73 );
	/* ww 04 000e	-- address wr14
	 * ww 04 0006	-- BRG  */
	M_setstat( fdHdl, M75_SCC_REG_14, 0x06 );
	/* ww 04 000c	-- address wr12
	 * ww 04 0071	--  */
	M_setstat( fdHdl, M75_SCC_REG_12, 0x71 );
	/* ww 04 000d	-- address wr13
	 * ww 04 0000	--  */
	M_setstat( fdHdl, M75_SCC_REG_13, 0x00 );
	/* ww 04 000e	-- address wr14
	 * ww 04 0007	-- BRG enable */
	M_setstat( fdHdl, M75_SCC_REG_14, 0x07 );
	/* ww 04 000f	-- address wr15
	 * ww 04 00c5	--  */ /* Break/Abort IE, Tx Underr/EOM IE, Status FIFO, WR7' */
	M_setstat( fdHdl, M75_SCC_REG_15, 0xc5 );
	/* ww 0c 0000 */
	M_setstat(fdHdl, M75_TXEN, 0x00 );
	/* ww 04 0000
	 * ww 04 0010  	-- reset ext stat Int */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0011
	 * ww 04 00f9 */
	M_setstat( fdHdl, M75_SCC_REG_01, 0xf9 ); /* Rx Int on spec. cond. only, Ext Int En */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0000
	 * rw 04 */
	M_getstat( fdHdl, M75_SCC_REG_00, &val );
	printf("    RR00 = 0x%04X\n", val);
	/* ww 10 0005   -- fifo reset register */
	M_setstat(fdHdl, M75_FIFO_RESET, 0x05 );
	/* ww 04 0013
	 * ww 04 00d9  	-- wr03 */ /*  8bits/char, Rx CRC En, Rx En */
	M_setstat( fdHdl, M75_SCC_REG_03, 0xd9 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0005
	 * ww 04 0069  	-- wr05 */ /* 8bits/char, Tx En, Tx CRC En */
	M_setstat( fdHdl, M75_SCC_REG_05, 0x69 );
	/* ww 04 0011
	 * ww 04 00f9   -- wr01*/
	M_setstat( fdHdl, M75_SCC_REG_01, 0xf9 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0009
	 * ww 04 002b  	-- wr09 */
	M_setstat( fdHdl, M75_SCC_REG_09, 0x2b );

	/* if Rx queue empty, wait */
	M_setstat(fdHdl, M75_GETBLOCK_TOUT, 0x200);

	/* init buffer */
	for(i=1; i<=EXT_FIFO_SIZE; i++)
		*bufp++ = (u_int8)i;

	/* send frame */
	numBytes = 0x0A;
	if( (nbrOfBytesSent1 = M_setblock(fdHdl,m75_Txbuf,numBytes)) != numBytes )
		error++;
	printf("    M_setblock(%04X) \tnbrOfBytesSent=%04X\n", numBytes, nbrOfBytesSent1);

	/* send frame */
	numBytes = 0x0C;
	if( (nbrOfBytesSent2 = M_setblock(fdHdl,m75_Txbuf,numBytes)) != numBytes )
		error++;
	printf("    M_setblock(%04X) \tnbrOfBytesSent=%04X\n", numBytes, nbrOfBytesSent2);

	/* receive frame */
	if( (nbrOfBytesReceived1 = M_getblock(fdHdl,m75_Rxbuf,EXT_FIFO_SIZE))
		!= nbrOfBytesSent1+2 )
		error++;
	printf("    M_getblock(%d) \tnbrOfBytesReceived=%d\n", EXT_FIFO_SIZE, nbrOfBytesReceived1);

	if( !error ){
		for(i=0; i < nbrOfBytesSent1; i++){
			if( *(m75_Rxbuf+i) != *(m75_Txbuf+i) ) error++;
		}
	}

	if( !error )
		printf("        ==> OK Frame send/receive\n");
	else {
		printf("\nError: Send/Receive Frame, channel %d\n"
			"\tsent bytes: %04X\n\tread bytes: %04X\n",
			ch, nbrOfBytesSent1, nbrOfBytesReceived1);

		printf("Received Frame:\n");
		for(i=0; i < nbrOfBytesReceived1; i++){
			printf("\t0x%02X", *(m75_Rxbuf+i));
			if( (i!=0) && !(i%8) )
				printf("\n");
		}
		printf("\n");
	}

	/* receive frame */
	if( (nbrOfBytesReceived2 = M_getblock(fdHdl,m75_Rxbuf,EXT_FIFO_SIZE))
		!= nbrOfBytesSent2+2 )
		error++;
	printf("    M_getblock(%d) \tnbrOfBytesReceived=%d\n", EXT_FIFO_SIZE, nbrOfBytesReceived2);

	if( !error ){
		for(i=0; i < nbrOfBytesSent2; i++){
			if( *(m75_Rxbuf+i) != *(m75_Txbuf+i) ) error++;
		}
	}

	if( !error )
		printf("        ==> OK Frame send/receive\n");
	else {
		printf("\nError: Send/Receive Frame, channel %d\n"
			"\tsent bytes: %04X\n\tread bytes: %04X\n",
			ch, nbrOfBytesSent2, nbrOfBytesReceived2);

		printf("Received Frame:\n");
		for(i=0; i < nbrOfBytesReceived2; i++){
			printf("\t0x%02X", *(m75_Rxbuf+i));
			if( (i!=0) && !(i%8) )
				printf("\n");
		}
		printf("\n");
	}

	UOS_Delay(800);

#if 0
	/* -- wait for interrupt */
	/* ha 00			*/
	UOS_Delay(800);
	/* ww 04 0003
	 * rw 04	-- rr03 */
	M_getstat( fdHdl, M75_SCC_REG_03, &val );
	printf("    RR03 = 0x%04X\n", val);
	/* ww 06 0002
	 * rw 06	-- read modified vector */
	M_setstat(fdHdl, M_MK_CH_CURRENT, 0x01);    /* set current channel to B*/
	M_getstat( fdHdl, M75_SCC_REG_02, &val );
	printf("    RR02 = 0x%04X (channel B)\n", val);
	M_setstat(fdHdl, M_MK_CH_CURRENT, ch);    /* reset current channel */
	/* ww 04 0000
	 * rw 04			*/
	M_getstat( fdHdl, M75_SCC_REG_00, &val );
	printf("    RR00 = 0x%04X\n", val);
	/* ww 04 0000
	 * ww 04 0010	-- reset ext stat irq */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x00 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x10 );
	/* ww 04 0000
	 * ww 04 0038	-- reset ius */
	M_setstat( fdHdl, M75_SCC_REG_00, 0x00 );
	M_setstat( fdHdl, M75_SCC_REG_00, 0x38 );
	/* ww 04 0003
	 * rw 04		-- still interrupt */
	M_getstat( fdHdl, M75_SCC_REG_03, &val );
	printf("    RR03 = 0x%04X\n", val);
#endif
	return( 0 );

}




