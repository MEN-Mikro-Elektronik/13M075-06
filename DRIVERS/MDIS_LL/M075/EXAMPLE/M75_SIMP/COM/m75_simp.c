/****************************************************************************
 ************                                                    ************
 ************                   M75_SIMP                         ************
 ************                                                    ************
 ****************************************************************************/
/*!
 *         \file m75_simp.c
 *
 *       \author Christian.Schuster@men.de
 *        $Date: 2009/07/14 17:47:18 $
 *    $Revision: 1.4 $
 *
 *       \brief  Simple example program for the M75 driver
 *
 *               The program sends and receives one frame on the same channel
 *               using the local loopback mode of the controller.
 *               For test to make sense, Rx clock must match the Tx clock.
 *               Since internally no working solution can be found, the
 *               Tx clock is echoed on the TRxC pin and the two clocks have
 *               to be connected together:
 *               - Pin13(DSR+) to Pin14(DTR+) and Pin6(DSR-) to Pin7(DTR-) of
 *                 corresponding port
 *               - or Pin9 - Pin10 and Pin19 - Pin20 on the 24 Pin rear I/O
 *                 receptacle connector
 *
 *     Required: libraries: mdis_api
 *     \switches (none)
 */
 /*
 *---------------------------------------------------------------------------
 * (c) Copyright 2004 by MEN Mikro Elektronik GmbH, Nuremberg, Germany
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
#include <MEN/mdis_api.h>
#include <MEN/usr_oss.h>
#include <MEN/mdis_err.h>
#include <MEN/m75_drv.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
/* none */
#define EXT_FIFO_SIZE 0x800
/*--------------------------------------+
|   TYPDEFS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void PrintError(char *info);


/********************************* main ************************************/
/** Program main function
 *
 *  \param argc       \IN  argument counter
 *  \param argv       \IN  argument vector
 *
 *  \return	          success (0) or error (1)
 */
int main(int argc, char *argv[])
{
	MDIS_PATH path;
	int32	chan, tconst,
			nbrOfBytesSent,
			nbrOfBytesReceived=0,
			i, error=0, syserror=0, got_data=0;
	char	*device, *errstr;
	u_int8	txBuffer[EXT_FIFO_SIZE],
			rxBuffer[EXT_FIFO_SIZE],
			*bufp=txBuffer;
	M75_SCC_REGS_PB sccregs;
	M_SG_BLOCK msg_blk;

	if (argc < 4 || strcmp(argv[1],"-?")==0) {
		printf("\n");
		printf("Syntax: m75_simp <device> <chan> <tconst>\n");
		printf("Function:\n");
		printf("  M75 example for receiving/transmitting frames\n");
		printf("  Rx and Tx clock must match => connect Tx/Rx clocks:\n");
		printf("   - Pin13(DSR+) to Pin14(DTR+) and\n");
		printf("     Pin6(DSR-) to Pin7(DTR-) of corresponding port\n");
		printf("   or\n");
		printf("   - Pin9(DSRA) to Pin10(DTRA) and\n");
		printf("     Pin19(DSRB) to Pin20(DTRB)\n");
		printf("     on the 24-pin rear I/O receptacle connector\n");
		printf("Options:\n");
		printf("    device       device name\n");
		printf("    chan         channel number (0..n)\n");
		printf("    tconst       BR Gen timeconstant (hex)\n");
		printf("\n");
		return(1);
	}

	device = argv[1];
	chan   = atoi(argv[2]);
	tconst = strtol(argv[3], &errstr, 16);

	printf("%s\t%d\t0x%X\n",device, chan, tconst);

	/*--------------------+
    |  open path          |
    +--------------------*/
	if( (path = M_open(device)) < 0 ) {
		PrintError("open");
		return(1);
	}

	/*--------------------+
    |  config             |
    +--------------------*/
	/* channel number */
	if ((M_setstat(path, M_MK_CH_CURRENT, chan)) < 0) {
		PrintError("setstat M_MK_CH_CURRENT");
		goto abort;
	}

	/*--------------------+
    |  send parameters    |
    +--------------------*/
	/* BR Gen Timeconstant, 16bit value */
	/* some example values for SDLC (clock mode=1)
	 *	 Baudrate   |    Timeconstant
	 *     9.600	|	0x02FE
	 *    64.000	|	0x0071
	 *   115.200	|	0x003E
	 * 1.053.200	|	0x0005
	 * 2.457.600	|	0x0001
	 */
	M_setstat( path, M75_BRGEN_TCONST, tconst );

	/* WR10: Coding mode */
	M_setstat( path, M75_SCC_REG_10, WR10_DEFAULT );

	/* WR11: Clock Mode */
	M_setstat( path, M75_SCC_REG_11, WR11_DEFAULT);

	/* WR14: set Local Loopback mode */
	M_setstat( path, M75_SCC_REG_14, WR14_DEFAULT | 0x10 );

	/* Rx control */
	M_setstat(path, M75_GETBLOCK_TOUT, 0x200);
	if( M_setstat(path, M75_MAX_RXFRAME_NUM, 10) ) {
		PrintError("M_setstat M75_MAX_TXFRAME_NUM");
		goto abort;
	}
	if( M_setstat(path, M75_MAX_RXFRAME_SIZE, EXT_FIFO_SIZE) ) {
		PrintError("M_setstat M75_MAX_TXFRAME_SIZE");
		goto abort;
	}

	/* enable IRQs */
	M_setstat(path, M_MK_IRQ_ENABLE, 0x01);   /* enable global irq */
	M_setstat(path, M75_IRQ_ENABLE, 0x01);    /* enable channels irq */

	/* 8bits/char, Rx CRC En, Rx En */
	M_setstat( path, M75_SCC_REG_03, 0xd9 );

	/* Tx control */
	M_setstat(path, M75_SETBLOCK_TOUT, 0);
	if( M_setstat(path, M75_MAX_TXFRAME_NUM, 10) ) {
		PrintError("M_setstat M75_MAX_TXFRAME_NUM");
		goto abort;
	}
	if( M_setstat(path, M75_MAX_TXFRAME_SIZE, EXT_FIFO_SIZE)) {
		PrintError("M_setstat M75_MAX_TXFRAME_SIZE");
		goto abort;
	}

	msg_blk.size = sizeof(M75_SCC_REGS_PB);
	msg_blk.data = (void *)&sccregs;
	M_getstat(path, M75_SCC_REGS, (int32 *)&msg_blk);
	printf( "    Current configuration:\n"
		    "        WR01 = 0x%02X    WR02 = 0x%02X    WR03 = 0x%02X\n"
			"        WR04 = 0x%02X    WR05 = 0x%02X    WR06 = 0x%02X\n"
			"        WR07 = 0x%02X    WR7p = 0x%02X    WR09 = 0x%02X\n"
			"        WR10 = 0x%02X    WR11 = 0x%02X    WR12 = 0x%02X\n"
			"        WR13 = 0x%02X    WR14 = 0x%02X    WR15 = 0x%02X\n\n",
			sccregs.wr01, sccregs.wr02, sccregs.wr03,
			sccregs.wr04, sccregs.wr05, sccregs.wr06,
			sccregs.wr07, sccregs.wr7p, sccregs.wr09,
			sccregs.wr10, sccregs.wr11, sccregs.wr12,
			sccregs.wr13, sccregs.wr14, sccregs.wr15 );

	/* init Tx buffer */
	for(i=1; i<=EXT_FIFO_SIZE; i++)
		*bufp++ = (u_int8)i;

	/* send frame */
	if( (nbrOfBytesSent = M_setblock(path,txBuffer,0x4F)) != 0x4F )
		error++;
	printf("    M_setblock() \tnbrOfBytesSent=%d\n", nbrOfBytesSent);


	/* receive frame (sent bytes + 2 bytes (CRC)) */
	while( !error && !got_data ){
		if( (nbrOfBytesReceived = M_getblock(path,rxBuffer,EXT_FIFO_SIZE))
			!= nbrOfBytesSent+2 ) {
			syserror = UOS_ErrnoGet();
			printf("*** %s ***\n",M_errstring( syserror ) );
			if( syserror == M75_ERR_RX_BREAKABORT ) {
				error = 0;
			} else {
				error++;
			}
		} else {
			got_data++;
		}
	}
	printf("    M_getblock() \tnbrOfBytesReceived=%d\n", nbrOfBytesReceived);

	/* print data */
	printf("Received Frame:\n");
	for(i=1; i <= nbrOfBytesReceived; i++){
		printf("\t0x%02X", *(rxBuffer+i-1));
		if( (i!=0) && !(i%8) )
			printf("\n");
	}
	printf("\n");

	/* check data */
	if( !error ){
		for(i=0; i < nbrOfBytesSent; i++){
			if( *(rxBuffer+i) != *(txBuffer+i) ) error++;
		}
	}

	if( !error )
		printf("  ==> OK Frame send/receive\n");
	else {
		printf("\nError: Send/Receive Frame, channel %d\n"
			"\tsent bytes: %04X\n\tread bytes: %04X\n",
			chan, nbrOfBytesSent, nbrOfBytesReceived);
	}

	UOS_Delay(100);

	/*--------------------+
    |  cleanup            |
    +--------------------*/
abort:
	if (M_close(path) < 0)
		PrintError("close");

	return(0);
}

/********************************* PrintError ******************************/
/** Print MDIS error message
 *
 *  \param info       \IN  info string
*/
static void PrintError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}



