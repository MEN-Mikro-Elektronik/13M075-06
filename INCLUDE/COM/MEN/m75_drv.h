/***********************  I n c l u d e  -  F i l e  ***********************/
/*!
 *        \file  m75_drv.h
 *
 *      \author  Christian.Schuster@men.de
 *        $Date: 2009/07/14 17:47:33 $
 *    $Revision: 2.5 $
 *
 *       \brief  Header file for M75 driver containing
 *               M75 specific status codes and
 *               M75 function prototypes
 *
 *    \switches  _ONE_NAMESPACE_PER_DRIVER_
 *               _LL_DRV_
 *               M75_SW
 */
 /*
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

#ifndef _M75_DRV_H
#define _M75_DRV_H

#ifdef __cplusplus
      extern "C" {
#endif


/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
/**@{*/
/** Structure for passing data with M75_SCC_REGS Getstat */
typedef struct {
	u_int8			wr01;	/**< SCC register WR01  */
	u_int8			wr02;	/**< SCC register WR02  */
	u_int8			wr03;	/**< SCC register WR03  */
	u_int8			wr04;	/**< SCC register WR04  */
	u_int8			wr05;	/**< SCC register WR05  */
	u_int8			wr06;	/**< SCC register WR06  */
	u_int8			wr07;	/**< SCC register WR07  */
	u_int8			wr7p;	/**< SCC register WR07' */
	u_int8			wr09;	/**< SCC register WR09  */
	u_int8			wr10;	/**< SCC register WR10  */
	u_int8			wr11;	/**< SCC register WR11  */
	u_int8			wr12;	/**< SCC register WR12  */
	u_int8			wr13;	/**< SCC register WR13  */
	u_int8			wr14;	/**< SCC register WR14  */
	u_int8			wr15;	/**< SCC register WR15  */
} M75_SCC_REGS_PB;
/**@}*/
/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/** \name M75 SCC register defaults
 *  \anchor scc_register_defaults */
#define WR01_DEFAULT		0xF9	/**<SCC Interrupt Enables */
									/*!<     RxInt on special condition only,
									 *       Parity is SC, others disable */
#define WR01_DEFAULT_ASY	0x13	/**<SCC Interrupt Enables */
									/*!<     RxInt on first char or special condition,
									 *       Parity is SC, Tx Int, others disable */
#define WR02_DEFAULT		0x00	/**<Interrupt Vector */
									/*!<     not used */
#define WR03_DEFAULT		0xD8	/**<Rx Control and Parameters */
									/*!<     8bpc, Hunt, Rx CRC en, Rx disable */
#define WR03_DEFAULT_ASY	0xC0	/**<Rx Control and Parameters, async mode */
									/*!<     8bpc, Rx disable */
#define WR04_DEFAULT		0x20	/**<Rx and Tx Control */
									/*!<     SDLC mode, parity even & disabled */
#define WR04_DEFAULT_ASY	0x4C	/**<Rx and Tx Control in async mode */
									/*!<     ASYNC, CLK X16, 2StopB, no parity */
#define WR05_DEFAULT		0x61	/**<Tx Control */
									/*!<     8bpC, Tx disable, Tx CRC en */
#define WR05_DEFAULT_ASY	0x60	/**<Tx Control, async mode */
									/*!<     8bpC, Tx disable */
#define WR06_DEFAULT		0x00	/**< SDLC Address */
									/*!<     not used */
#define WR07_DEFAULT		0x7E	/**<WR7: SDLC Flag */
									/*!<     standard SDLC flag*/
#define WR7P_DEFAULT		0x73	/**<WR7': Extended Feature, extended read */
									/*!<     Extended read, complete CRC Rx,
									 *       DTR/REQ Fast Mode
									 *       Auto EOM res & Tx Flag */
#define WR09_DEFAULT		0x2B	/**<Master Interrupt Control and Reset */
									/*!<     SW Intack, MIE, NV, VIS */
#define WR10_DEFAULT		0x80	/**<Rx and Tx miscellaneous Control Bits */
									/*!<     CRC preset 1, NRZ, others disable */
#define WR11_DEFAULT		0x16	/**<Clock Mode */
									/*!<     Rx clock: RTxC pin,
									 *       Tx: BR Gen, TxC Out: BR Gen */
#define WR11_DEFAULT_ASY	0x56	/**<Clock Mode */
									/*!<     Rx clock: BR Gen,
									 *       Tx: BR Gen, TxC Out: BR Gen */
#define WR12_DEFAULT		0x71	/**<BR Generator Time Constant, Lower Byte */
									/*!<     See descrition WR13_DEFAULT */
#define WR13_DEFAULT		0x00	/**<BR Generator Time Constant, Upper Byte */
									/*!<     Baud Rate: 64.000 */
#define WR14_DEFAULT		0x07	/**<Miscellaneous Control */
									/*!<     DTR/REQ function,
									 *       BR Gen Source: PCLK Input
									 *       BR Gen Enable */
#ifdef M75_SUPPORT_BREAK_ABORT
#	define WR15_DEFAULT		0xC5	/**<Ext. Status/Source interrupt control */
									/*!<     Break/Abort, TxUndr/EOM,
		                             *       Status FIFO en, WR7' en
									 *       Sync/Hunt IE should always be disabled */
#else
#	define WR15_DEFAULT		0x45	/**<Ext. Status/Source interrupt control */
									/*!<     TxUndr/EOM,
		                             *       Status FIFO en, WR7' en
									 *       Sync/Hunt IE should always be disabled */
#endif
									/*       bit in rr0 also indicates Break/Abort */

/** \name M75 specific Getstat/Setstat standard codes
 *  \anchor getstat_setstat_codes
 *
 *   !!! extended read is only set/reset by WR15(2) !!!
 */
/**@{*/
#define M75_IRQ_ENABLE      M_DEV_OF+0x1E
/**<S: Enable/Disable current channels interrupts */
/*!< This SetStat does not affect the Master IE or the other channels interrupts.
 *   Before enabling the channels individual interrupts, the global interrupt
 *   has to be enabled ( M_MK_IRQ_ENABLE ).\n
 *   possible values: 1: enable interrupts; 0: disable interrupts
 */
#define M75_SCC_REG_03      M_DEV_OF+0x03
/**<S: WR03: Rx control & parameters*/
/*!<\verbatim
 *.   -----------------------
 *.  |D7|D6|D5|D4|D3|D2|D1|D0|
 *.   -----------------------
 *.    |  |        |  |     |
 *.    |  |        |  |      --- Rx Enable
 *.    |  |        |   --------- Address Search Mode (SDLC)
 *.    |  |         ------------ Rx CRC Enable
 *.    |  |
 *.    0  0  Rx 5 bits/char
 *.    0  1  Rx 7 bits/char
 *.    1  0  Rx 6 bits/char
 *.    1  1  Rx 8 bits/char
 * \endverbatim
 * - Bits 7 and 6: Receiver Bits/Character\n
 *	The state of these two bits determines the number of bits to be assembled
 *	as a characterin the received serial data stream. The number of bits per
 *	character can be changed while a character is being assembled, but only
 *	before the number of bits currently programmed is reached.
 *	In synchronous modes and SDLC modes, the SCC transfers an 8-bit section of
 *	the serial data stream to the receive FIFO at the appropriate time.\n\n
 * - Bit 3: Receiver CRC Enable\n
 *	This bit is used to initiate CRC calculation at the beginning of the last
 *	byte transferred from the Receiver Shift register to the receive FIFO.
 *	This operation occurs independently of the number of bytes in the receive
 *	FIFO. If this feature is used, care must be taken to ensure that eight bits
 *	per character is selected in the receiver because of an inherent delay from
 *	the Receive Shift register to the CRC checker.
 *	This bit is internally set to 1 in SDLC mode and the SCC calculates CRC
 *	on all bits ex-cept inserted zeros between the opening and closing
 *	character flags.\n\n
 * - Bit 2: Address Search Mode (SDLC)\n
 *	Setting this bit in SDLC mode causes messages with addresses not matching
 *	the address programmed in WR6 to be rejected. No receiver interrupts can
 *	occur in this mode unless there is an address match. The address that the
 *	SCC attempts to match is unique (1 in 256).\n\n
 * - Bit 0: Receiver Enable\n
 *	When this bit is set to 1, receiver operation begins. This bit should be
 *	set only after all other receiver parameters are established and the
 *	receiver is completely initialized. This bit is reset by a channel or
 *	hardware reset command, and it disables the receiver.\n
 *  Before enabling the receiver, the interrupts have to be enabled first.
 */

/*  G: RR03: Interrupt pending reg channel A only */
#define M75_SCC_REG_04      M_DEV_OF+0x04
/**<S: WR04: Rx/Tx control bits */
/*!<\verbatim
 *.   -----------------------
 *.  |D7|D6|D5|D4|D3|D2|D1|D0|
 *.   -----------------------
 *.                      |  |
 *.                      |   --- Parity Enable
 *.                      |
 *.                       ------ Parity Even/Odd
 * \endverbatim
 * - Bit 1: Parity Even/Odd \n
 *	This bit determines whether parity is checked as even or odd.
 *	A 1 programmed here selects even parity, and a 0 selects odd parity.
 *	This bit is ignored if the Parity Enable bit is not set.\n\n
 * - Bit 0: Parity Enable\n
 *	When this bit is set, an additional bit position beyond those specified
 *	in the bits/character control is added to the transmitted data and is
 *	expected in the receive data. The Received Parity bit is transferred to
 *	the CPU as part of the data unless eight bits per character is selected
 *	in the receiver.
 */
/*  G: RR04: extended read: WR04, else RR00 */
#define M75_SCC_REG_05      M_DEV_OF+0x05
/**<S: WR05: Tx control & parameters */
/*!<\verbatim
 *.   -----------------------
 *.  |D7|D6|D5|D4|D3|D2|D1|D0|
 *.   -----------------------
 *.       |  |     |        |
 *.       |  |     |         --- Tx CRC Enable
 *.       |  |     |
 *.       |  |      ------------ Tx Enable
 *.       |  |
 *.       0  0  Tx 5 or less bits/char
 *.       0  1  Tx 7 bits/char
 *.       1  0  Tx 6 bits/char
 *.       1  1  Tx 8 bits/char
 * \endverbatim
 * - Bits 6 and 5: TX Bits/Character 1 and 0 \n
 *	These bits control the number of bits in each byte transferred to the
 *	transmit buffer. Bits sent must be right justified with least significant
 *	bits first. The Five Or Less mode allows transmission of one to five bits
 *	per character; however, the CPU should format the data character as shown
 *	below. In the Six or Seven Bits/Character modes, unused data bits are
 *	ignored.
 * \verbatim
 *.   |D7|D6|D5|D4|D3|D2|D1|D0|   Transmitter
 *.     1  1  1  1  0  0  0 D0    Sends one data bit
 *.     1  1  1  0  0  0 D1 D0    Sends two data bits
 *.     1  1  0  0  0 D2 D1 D0    Sends three data bits
 *.     1  0  0  0 D3 D2 D1 D0    Sends four data bits
 *.     0  0  0 D4 D3 D2 D1 D0    Sends five data bits
 * \endverbatim
 *
 * - Bit 3: Transmit Enable \n
 *	Data is not transmitted until this bit is set, and the TxD output sends
 *	continuous 1s unless Auto Echo mode or SDLC Loop mode is selected.
 *	If this bit is reset after transmission has started, the transmission
 *	of data or sync characters is completed. If the transmitter is disabled
 *	during the transmission of a CRC character, sync or flag characters
 *	are sent instead of CRC. This bit is reset by a channel or hardware reset.\n
 *  Before enabling the transmitter, the interrupts have to be enabled first.\n\n
 * - Bit 0: Transmit CRC Enable \n
 *	This bit determines whether or not CRC is calculated on a transmit
 *	character. If this bit is set at the time the character is loaded from the
 *	transmit buffer to the Transmit Shift register, CRC is calculated on that
 *	character. CRC is not automatically sent unless this bit is set when the
 *	transmit underrun exists.\n\n
 */
/*  G: RR05: extended read: WR05, else RR01 */
#define M75_SCC_REG_06      M_DEV_OF+0x06
/**<S: WR06: SDLC Address */
/*!<
 * WR06 is programmed to contain the secondary address field used to compare
 * against the address field of the SDLC Frame. In SDLC mode, the SCC does not
 * automatically transmit the station address at the beginning of a response
 * frame.
 */
/*  G: RR06: ext. read: LSB of frame byte count,
			 else RR02 */
#define M75_SCC_REG_10      M_DEV_OF+0x0A
/**<S: WR10: Tx/Rx misc control */
/*!<\verbatim
 *.   -----------------------
 *.  |D7|D6|D5|D4|D3|D2|D1|D0|
 *.   -----------------------
 *.    |  |  |     |
 *.    |  |  |      ----------- Mark/Flag Idle
 *.    |  |  |
 *.    |  0  0  NRZ
 *.    |  0  1  NRZI
 *.    |  1  x  reserved
 *.    |
 *.     ----------------------- CRC Preset I/O
 * \endverbatim
 * - Bit 7: CRC Presets 1 or 0\n
 *	This bit specifies the initialized condition of the receive CRC checker
 *	and the transmit CRC generator. If this bit is set to 1, the CRC
 *	generator and checker are preset to 1. If this bit is set to 0,
 *	the CRC generator and checker are preset to 0. The transmitted CRC is
 *	inverted before transmission and the received CRC is checked against the
 *	bit pattern "0001110100001111". This bit is reset by a channel or hardware
 *	reset.\n\n
 * - Bits 6 and 5: Data Encoding 1 and 2\n
 *	These bits control the coding method used for both the transmitter and the
 *	receiver. All of the clocking options are available for all coding methods.
 *	Any coding method can be used in the X1 clock mode.\n\n
 * - Bit 3: Mark/Flag Idle\n
 *	This bit is used to control the idle line condition. If this bit is set
 *	to 0, the transmitter sends flags as an idle line. If this bit is set
 *	to 1, the transmitter sends continuous 1s after the closing flag of
 *	a frame. The idle line condition is selected byte by byte; i.e., either
 *	a flag or eight 1s are transmitted.
 */
/*
 * Bit 3: Mark/Flag Idle additional:
 *	The primary station in an SDLC loop should be programmed for Mark Idle
 *	to create the EOP sequence. Mark Idle must be deselected at the beginning
 *	of a frame before the first data are written to the SCC, so that an opening
 *	flag can be transmitted. This bit is ignored in Loop mode, but the
 *	programmed value takes effect upon exiting the Loop mode. This bit is reset
 *	by a channel or hardware reset.
*/

#define M75_SCC_REG_11      M_DEV_OF+0x0B
/**<S: WR11: Clock mode */
/*!< WR11 is the Clock Mode Control register. The bits in this register control
 * the sources of both the receive and transmit clocks, the type of signal on
 * the Sync and RTxC pins, and the direction of the TRxC pin.
 *\verbatim
 *.   -----------------------
 *.  |D7|D6|D5|D4|D3|D2|D1|D0|
 *.   -----------------------
 *.       |  |  |  |  |  |  |
 *.       |  |  |  |  |  0  0  /TRxC Out = Xtal Output
 *.       |  |  |  |  |  0  1  /TRxC Out = Transmit Clock
 *.       |  |  |  |  |  1  0  /TRxC Out = BR Generator Output
 *.       |  |  |  |  |  0  0  /TRxC Out = reserved
 *.       |  |  |  |   ------- /TRxC O/I
 *.       |  |  |  |
 *.       |  |  0  0  Transmit Clock = /RTxC Pin
 *.       |  |  0  1  Transmit Clock = /TRxC Pin
 *.       |  |  1  0  Transmit Clock = BR Generator Output
 *.       |  |  1  1  Transmit Clock = reserved
 *.       |  |
 *.       0  0  Receive Clock = /RTxC Pin
 *.       0  1  Receive Clock = /TRxC Pin
 *.       1  0  Receive Clock = BR Generator Output
 *.       1  1  Receive Clock = reserved
 * \endverbatim
 * - Bits 6 and 5: Receiver Clock 1 And 0\n
 *	These bits determine the source of the receive clock. They do not interfere
 *	with any of the modes of operation in the SCC but simply control a
 *	multiplexer just before the internal receive clock input. A hardware reset
 *	forces the receive clock to come from the TRxC pin.\n\n
 * - Bits 4 and 3: Transmit Clock 1 and 0\n
 *	These bits determine the source of the transmit clock. They do not interfere
 *	with any of the modes of operation of the SCC but simply control a
 *	multiplexer just before the internal transmit clock input. A hardware reset
 *	selects the TRxC pin as the source of the transmit clocks.\n\n
 * - Bit 2: TRxC O/I\n
 *	This bit determines the direction of the TRxC pin. If this it is set to 1,
 *	the TRxC pin is an output and carries the signal selected by D1 and D0 of
 *	this register. However, if either the receive or the transmit clock is
 *	programmed to come from the TRxC pin, TRxC will be an input, regardless of
 *	the state of this bit. The TRxC pin is also an input if this bit is set to
 *	0. A hardware reset forces this bit to 0.\n\n
 * - Bits 1 and 0: TRxC Output Source 1 And 0\n
 *	These bits determine the signal to be echoed out of the SCC via the TRxC
 *	pin. No signal is produced if TRxC has been programmed as the source of
 *	either the receive or the transmit clock. If TRxC O/I (bit 2) is set to 0,
 *	these bits are ignored. If the XTAL oscillator output is programmed to be
 *	echoed, and the XTAL oscillator has not been enabled, the TRxC pin goes
 *	High. Hardware reset selects the XTAL oscillator as the output source.
 */
/*  G: RR11: extended read: WR10 else WR15 */

#define M75_SCC_REG_14      M_DEV_OF+0x0E
/**<S: WR14: Misc control */
/*!<\verbatim
 *.   -----------------------
 *.  |D7|D6|D5|D4|D3|D2|D1|D0|
 *.   -----------------------
 *.             |  |
 *.             |   ---------- Auto Echo
 *.              ------------- Local Loopback
 * \endverbatim
 * - Bit 4: Local Loopback\n
 *	Setting this bit to 1 selects the Local Loopback mode of operation In this
 *	mode, the internal transmitted data are routed back to the receiver, as well
 *	as to the TxD pin. The CTS and DCD inputs are ignored as enables in Local
 *	Loopback mode, even if Auto Enables is selected. (if so programmed,
 *	transitions on these inputs still cause interrupts.) This mode works with
 *	any Transmit/Receive mode except Loop mode. For meaningful results, the
 *	frequency of the transmit and receive clocks must be the same. This bit is
 *	reset by a channel or hardware reset.\n\n
 * - Bit 3: Auto Echo\n
 *	Setting this bit to 1 selects the Auto Enable mode of operation. In this
 *	mode, the TxD pin is connected to RxD, as in Local Loopback mode, but the
 *	receiver still listens to the RxD input. Transmitted data are never seen
 *	inside or outside the SCC in this mode, and CTS is ignored as a transmit
 *	enable. This bit is reset by a channel or hardware reset.
 */
/*  G: RR14: extended read: WR07P else WR14 */
#define M75_IO_SEL          M_DEV_OF+0x15
/**<S: IO select bit for current channel */
/*!< 0: select front IO; 1: select rear IO; */
#define M75_GETBLOCK_TOUT   M_DEV_OF+0x16
/**<G,S: Timeout for M_getblock calls, cur chan */
/*!<
 *-1: wait endless;      \n
 * 0: return immediately \n
 *>0: time in ms
 */
#define M75_SETBLOCK_TOUT   M_DEV_OF+0x17
/**<G,S: Timeout for M_setblock calls, cur chan */
/*!<
 *-1: wait endless;      \n
 * 0: return immediately \n
 *>0: time in ms
 */
#define M75_MAX_TXFRAME_SIZE M_DEV_OF+0x18
/**<G,S: Maximum Tx frame size, cur channel */
/*!< possible values: 0 .. 0x800 \n
 *   !!! all data is lost
 */
#define M75_MAX_RXFRAME_SIZE M_DEV_OF+0x19
/**<G,S: Maximum Rx frame size, cur channel */
/*!< possible values: 0 .. 0x800 \n
 *   !!! all data is lost
 */
#define M75_MAX_TXFRAME_NUM M_DEV_OF+0x1A
/**<G,S: Maximum number Tx frames, cur channel */
/*!< possible values: 0 .. system limitations \n
 *   !!! all data is lost
 */
#define M75_MAX_RXFRAME_NUM M_DEV_OF+0x1B
/**<G,S: Maximum number Rx frames, cur bchannel */
/*!< possible values: 0 .. system limitations \n
 *   !!! all data is lost
 */
#define M75_SETRXSIG		M_DEV_OF+0x1C
/**<S: install Rx Signal */
#define M75_CLRRXSIG		M_DEV_OF+0x1D
/**<S: remove Rx Signal */
#define M75_BRGEN_TCONST 	M_DEV_OF+0x1F
/**<S: set Baud Rate Generator Time constant, 16 bit value */
/*!< The time constant is computed according to the formula below:
 * \verbatim
 *.             _                _
 *.            |                  |
 *.            | clock frequency  |          with:
 *.   tconst = |----------------- | - 2;        clock frequency = 14.7456 MHz
 *.            |  2 * Baud Rate   |
 *.            |_                _|
 *
 *	 some example values for SDLC (clock mode=1)
 *
 *.   Baudrate	|	Timeconstant
 *.      9,600	|	0x02FE
 *.     64,000	|	0x0071
 *.    115,200	|	0x003E
 *.  1,053,200	|	0x0005
 *.  2,457,600	|	0x0001
 *\endverbatim
 */

/**@}*/
#define M75_SCC_REG_00		M_DEV_OF+0x00
							/*  S: WR00: SCC command reg
							    G: RR00: Rx/Tx buffer status */
#define M75_SCC_REG_01      M_DEV_OF+0x01
							/*  S: WR01: SCC interrupt modes \n
							    G: RR01: Special Rx condition status */
#define M75_SCC_REG_02      M_DEV_OF+0x02
							/*  S: WR02: Interrupt vector \n
							    G: WR02: Interrupt vector, Chan B: modified */
#define M75_SCC_REG_07      M_DEV_OF+0x07
		  					/*  S: WR07: SDLC FLAG \n
							    G: RR07: ext. read: MSB of frame byte count,
							             else RR03 */
#define M75_SCC_REG_7P      M_DEV_OF+0x10
							/*  S: WR07P: Extended feature and FIFO control */
#define M75_SCC_REG_08      M_DEV_OF+0x08
							/*  S: WR08: Tx data register \n
							    G  RR08: Rx data register */
#define M75_SCC_REG_09      M_DEV_OF+0x09
							/*  S: WR09: Master interrupt control & reset \n
							    G: RR09: extended read: WR03 else WR13 */
#define M75_SCC_REG_12      M_DEV_OF+0x0C
							/*  S,G: WR12: LSB of BR Generator Time Constant */
#define M75_SCC_REG_13      M_DEV_OF+0x0D
							/*  S,G: WR13: MSB of BR Generator Time Constant */
#define M75_SCC_REG_15      M_DEV_OF+0x0F
							/*  S,G: WR15: External/Status source control */
#define M75_FIFO_DATA       M_DEV_OF+0x11
							/*  G,S: Tx/Rx FIFO Data register, cur. channel */
#define M75_FIFO_STATUS     M_DEV_OF+0x12
							/*  G: Tx/Rx FIFO status, current channel */
#define M75_FIFO_RESET      M_DEV_OF+0x13
							/*  S: Tx/Rx FIFO reset register, both channels */
#define M75_TXEN            M_DEV_OF+0x14
							/*  S: Tx En-/Disable for current channel */
#define M75_RXEN            M_DEV_OF+0x20
							/*  S: Rx En-/Disable for current channel */

/** \name M75 specific Getstat/Setstat block codes */
/**@{*/
#define M75_SCC_REGS 		M_DEV_BLK_OF+0x00
							/**<G: get all SCC WRxx registers (BlockGetstat) */
							/*!< returns a structure of type M75_SCC_REGS_PB */
/**@}*/

/** \name M75 specific Error/Warning codes */
/**@{*/
#define	M75_ERR_BADPARAMETER	(ERR_LL_ILL_PARAM)	/**< bad parameter */
#define M75_ERR_RX_QEMPTY		(ERR_DEV+1)	/**< Rx queue empty */
#define M75_ERR_RX_QFULL		(ERR_DEV+2)	/**< Rx queue full or not initialized */
#define M75_ERR_RX_BREAKABORT	(ERR_DEV+3)	/**< Break/Abort received */
#define M75_ERR_RX_OVERFLOW		(ERR_DEV+4)	/**< Status or Data FIFO overflow */
#define M75_ERR_RX_ERROR		(ERR_DEV+5)	/**< General Rx Error (CRC/Parity/Overrun)*/
#define M75_ERR_TX_QFULL		(ERR_DEV+6)	/**< Tx queue full */
#define M75_ERR_FRAMETOOLARGE	(ERR_DEV+7)	/**< frame larger than MAX_FRAME_SIZE */
#define M75_ERR_TXFIFO_NOACCESS	(ERR_DEV+8)	/**< Cannot send data to Tx FIFO */
#define M75_ERR_CH_NUMBER		(ERR_DEV+9)	/**< bad channel number */
#define M75_ERR_SIGBUSY			(ERR_DEV+10)/**< signal already installed/removed */
#define M75_ERR_INT_DISABLED	(ERR_DEV+11)/**< global or individual interrupts not enabled */
/**@}*/

/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
#ifdef _LL_DRV_

#	ifdef _ONE_NAMESPACE_PER_DRIVER_
#		define M75_GetEntry LL_GetEntry
#	else
#		ifdef M75_SW
#			define M75_GetEntry M75_SW_GetEntry
#		endif /* M75_SW */
		extern void M75_GetEntry( LL_ENTRY* drvP );
#	endif /* _ONE_NAMESPACE_PER_DRIVER_ */

#endif /* _LL_DRV_ */

/*-----------------------------------------+
|  BACKWARD COMPATIBILITY TO MDIS4         |
+-----------------------------------------*/
#ifndef U_INT32_OR_64
	/* we have an MDIS4 men_types.h and mdis_api.h included */
	/* only 32bit compatibility needed!                     */
	#define INT32_OR_64  int32
	#define U_INT32_OR_64 u_int32

	typedef INT32_OR_64  MDIS_PATH;
#endif /* U_INT32_OR_64 */


#ifdef __cplusplus
      }
#endif

#endif /* _M75_DRV_H */




