/****************************************************************************
 ************                                                    ************
 ************                   M75_ASYNC                        ************
 ************                                                    ************
 ****************************************************************************/
/*!
 *         \file m75_async.c
 *
 *       \author Christian.Schuster@men.de
 *        $Date: 2009/07/14 17:47:20 $
 *    $Revision: 1.1 $
 *
 *       \brief  Simple example program for the M75 driver in ASYNC mode
 *
 *               The program sends some chars on one device and reads them
 *               on another
 *
 *     Required: libraries: mdis_api
 *     \switches (none)
 */
 /*
 *---------------------------------------------------------------------------
 * Copyright (c) 2009-2019, MEN Mikro Elektronik GmbH
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
static MDIS_PATH setupChannel(char* dev, int32 chan, int32 tconst);
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
	MDIS_PATH path1 = -1, path2 = -1;
	int32	chan1, chan2,
			tconst,
			nbrOfBytesSent,
			nbrOfBytesReceived=0,
			i, n=0, fSize = 0x10, got_data=0, loops = 1,
			error=0, syserror = 0, ckerror = 0;
			/* syserror=0, */
	char	*dev1, *dev2, *errstr;
	u_int8	txBuffer[EXT_FIFO_SIZE],
			rxBuffer[EXT_FIFO_SIZE],
			*bufp=txBuffer;

	if (argc < 6 || strcmp(argv[1],"-?")==0) {
		printf("\n");
		printf("Syntax: m75_simp <device1> <chan1> <device2> <chan2> <tconst> <loops>\n");
		printf("Function:\n");
		printf("  M75 example for receiving/transmitting in asynchronous mode\n");
		printf("Options:\n");
		printf("    deviceX       device name\n");
		printf("    chanX         channel number (0..n)\n");
		printf("    tconst        BR Gen timeconstant (hex)\n");
		printf("    loops         numbers of packets to Xfer\n");
		return(1);
	}

	dev1    = argv[1];
	chan1   = atoi(argv[2]);
	dev2    = argv[3];
	chan2   = atoi(argv[4]);

	tconst = strtol(argv[5], &errstr, 16);
	loops  = atoi(argv[6]);

	printf("%s:%d <-> %s:%d\t0x%X\n",dev1, chan1, dev2, chan2, tconst);


	/*------------------------------+
    |  open path and config device  |
    +------------------------------*/
    if( (path1 = setupChannel( dev1, chan1, tconst)) < 0 ) {
    	goto ABORT;
    }
    if( (path2 = setupChannel( dev2, chan2, tconst)) < 0 ) {
    	goto ABORT;
    }

	while( n++ < loops ) {
		/* init Tx buffer */
		bufp = txBuffer;
		if( fSize > EXT_FIFO_SIZE )
			fSize = 0x10;

		*bufp++ = (u_int8)n;
		for(i=1; i<fSize; i++) {
			*bufp++ = (u_int8)(i+n);
		}
		/* send frame */
		if( (nbrOfBytesSent = M_setblock(path1,txBuffer,fSize)) != fSize ) {
			error++;
		}
		printf("    M_setblock() \tnbrOfBytesSent=%d\n", nbrOfBytesSent);


		UOS_Delay( fSize*(6+(tconst>>3)) );

		/* receive frame (sent bytes) */
		while( !error && !got_data ){
			if( (nbrOfBytesReceived = M_getblock(path2,rxBuffer,EXT_FIFO_SIZE)) != nbrOfBytesSent ) {
				syserror = UOS_ErrnoGet();
				printf("*** %s ***\n",M_errstring( syserror ) );
				if( syserror != ERR_SUCCESS ) {
					error++;
				}
			} else {
				got_data++;
			}
		}
		if( got_data ) {
			printf("        M_getblock() \tnbrOfBytesReceived=%d\n", nbrOfBytesReceived);

			/* print data */
			printf("Received data:\n");
			for(i=1; i <= nbrOfBytesReceived; i++){
				printf("\t0x%02X", *(rxBuffer+i-1));
				if( (i!=0) && !(i%8) )
					printf("\n");
			}
			printf("\n");

			/* check data */
			if( !error ){
				for(i=0; i < nbrOfBytesSent; i++){
					if( *(rxBuffer+i) != *(txBuffer+i) ) ckerror++;
				}
				if( ckerror ) {
					error++;
					printf( "*** frames sent and received differ!!!\n\n\n" );

					printf("   Sent data:\n");
					for(i=1; i <= nbrOfBytesSent; i++){
						printf("\t0x%02X", *(txBuffer+i-1));
						if( (i!=0) && !(i%8) )
							printf("\n");
					}
					printf("\n");
				}
			} else {
				printf("*** data not beeing checked (error in Rx or Rx anyway)!\n");
			}
		} else {
			printf("*** NO (SUFFICIENT) DATA RECEIVED\n");
		}
		got_data = 0;
		fSize = fSize+7;

	}
	if( !error )
		printf("  ==> OK Frame send/receive\n");
	else {
		printf("\nError: Send/Receive Frame\n"
			"\tsent bytes: %04X\n\tread bytes: %04X\n",
			nbrOfBytesSent, nbrOfBytesReceived);
	}

	UOS_Delay(100);

	/*--------------------+
    |  cleanup            |
    +--------------------*/
ABORT:
	if ( (path1 >= 0) && (M_close(path1) < 0) )
		PrintError("close dev1");
	if ( (path2 >= 0) && (M_close(path2) < 0) )
		PrintError("close dev2" );

	return(0);
}

/********************************* setupChannel ******************************/
/** setup channel as requested
 *
 *  \param dev        \IN   name of device
 *  \param chan       \IN   channel
 *  \param tconst     \IN   BR Gen timeconstant to be passed to driver
 *  \param path       \OUT  open path do device
*/
static MDIS_PATH setupChannel(char* dev, int32 chan, int32 tconst)
{
	MDIS_PATH path;
	M75_SCC_REGS_PB sccregs;
	M_SG_BLOCK msg_blk;

	/*--------------------+
    |  open path          |
    +--------------------*/
	if( (path = M_open(dev)) < 0 ) {
		PrintError("open device");
		return(-1);
	}

	/* channel number */
	if( M_setstat(path, M_MK_CH_CURRENT, chan) < 0 ) {
		PrintError("M_setstat M_MK_CH_CURRENT");
		goto ABORT;
	}

	/*--------------------+
    |  send parameters    |
    +--------------------*/
	/* BR Gen Timeconstant, 16bit value */
	/* some example values for async (clock mode=16)
	 *	 Baudrate   |    Timeconstant
	 *       156	|	0x707E
	 *     9.600	|	0x002E
	 *    16.000	|	0x0016
	 *    38.400	|	0x000A
	 *   115.200    |   0x0002
	 *   230.400    |   0x0000
	 */
	if( M_setstat( path, M75_BRGEN_TCONST, tconst ) < 0 ) {
		PrintError("M_setstat M75_BRGEN_TCONST");
		goto ABORT;
	}

	/* enable IRQs */
	if( M_setstat( path, M_MK_IRQ_ENABLE, 0x01 ) < 0 ) {   /* enable global irq */
		PrintError("M_setstat M75_BRGEN_TCONST");
		goto ABORT;
	}
	if( M_setstat( path, M75_IRQ_ENABLE, 0x01 ) < 0 ) {    /* enable channels irq */
		PrintError("M_setstat M_MK_IRQ_ENABLE");
		goto ABORT;
	}

	/* Rx En */
	if( M_setstat( path, M75_RXEN, 1 ) < 0 ) {
		PrintError("M_setstat M75_RXEN");
		goto ABORT;
	}

	/* Tx control */
	if( M_setstat( path, M75_SETBLOCK_TOUT, 0 ) < 0 ) {
		PrintError("M_setstat M75_SETBLOCK_TOUT");
		goto ABORT;
	}
	if( M_setstat( path, M75_MAX_TXFRAME_NUM, 10 )  < 0 ) {
		PrintError("M_setstat M75_MAX_TXFRAME_NUM");
		goto ABORT;
	}
	if( M_setstat( path, M75_MAX_TXFRAME_SIZE, EXT_FIFO_SIZE ) < 0 ) {
		PrintError("M_setstat M75_MAX_TXFRAME_SIZE");
		goto ABORT;
	}

	msg_blk.size = sizeof(M75_SCC_REGS_PB);
	msg_blk.data = (void *)&sccregs;
	M_getstat(path, M75_SCC_REGS, (int32 *)&msg_blk);
	printf( "    Current configuration of %s:%d:\n"
		    "        WR01 = 0x%02X    WR02 = 0x%02X    WR03 = 0x%02X\n"
			"        WR04 = 0x%02X    WR05 = 0x%02X    WR06 = 0x%02X\n"
			"        WR07 = 0x%02X    WR7p = 0x%02X    WR09 = 0x%02X\n"
			"        WR10 = 0x%02X    WR11 = 0x%02X    WR12 = 0x%02X\n"
			"        WR13 = 0x%02X    WR14 = 0x%02X    WR15 = 0x%02X\n\n",
			dev, chan,
			sccregs.wr01, sccregs.wr02, sccregs.wr03,
			sccregs.wr04, sccregs.wr05, sccregs.wr06,
			sccregs.wr07, sccregs.wr7p, sccregs.wr09,
			sccregs.wr10, sccregs.wr11, sccregs.wr12,
			sccregs.wr13, sccregs.wr14, sccregs.wr15 );

	return( path );

ABORT:
	if (M_close(path) < 0)
		PrintError("close dev");
	return( -1 );

} /* setupChannel */
/********************************* PrintError ******************************/
/** Print MDIS error message
 *
 *  \param info       \IN  info string
*/
static void PrintError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}


