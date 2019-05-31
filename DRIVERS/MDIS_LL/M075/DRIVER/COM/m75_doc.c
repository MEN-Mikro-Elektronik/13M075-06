/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!
 *        \file  m75_doc.c
 *
 *      \author  Christian.Schuster@men.de
 *
 *      \brief   User documentation for M75 module driver
 *
 *     Required: -
 *
 *     \switches -
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

/*! \mainpage
    This is the documentation of the MDIS low-level driver for the M75 module.

    The M75 is a dual HDLC/SDLC communication controller.

	This driver provides an MDIS interface for setup of the controller and
	transmit/receive data in HDLC/SDLC mode.
	Other synchronous modes and asychronous modes are not supported
	by this driver.

	The driver is valid for M75 hardware Rev. 03 or higher.
    \n
    \section Variants Variants
    The M75 is an A08/D08 M-Module. It can be operated on all M-Module
    carrier boards. In order to support different CPU/carrier board
    combinations and to achieve maximum performance, the M75 driver can be
    built in some variants at compilation time:

    \verbatim
    Driver              Variant Description
    --------            --------------------------------
    Standard            D08 register access, non-swapped
    _sw                 D08 register access, swapped
    \endverbatim

    Here are some combinations of MEN CPU and carrier boards together with the
    required variants:

    \verbatim
    CPU                    Carrier Board          Driver Variant
    ----------------       -------------          --------------
    MEN A12  (68k VME)     A201                   Standard
    MEN A12b (68k VME)     onboard slots          _sw
    MEN F7   (PPC CPCI)    F202                   Standard
    \endverbatim

    \n \section BuildDrv Build Driver
	The driver is built using the MDIS build environment.

	\n \subsection BrkAbrtSw Switch M75_SUPPORT_BREAK_ABORT
	This preprocessor switch in the driver(_sw).mak file is selecting
	whether BREAK/ABORT interrupts should be used. When using this interrupt,
	some frames may get lost everytime the transmitter or receiver are beeing
	enabled. But the M75 device will always synchronize again when for
	example the line is dis- and reconnected or another unnormal status is left.

	When not using the BREAK/ABORT interrupt, no frames are lost during normal
	operation, but therefore the M75 device might loose the synchronisation
	between data and status FIFO. In this case, the M75 device will return the
	correct number of received data bytes, but the data of the last frame may
	partly be attached to the data of the current frame, which , in turn, will
	not be completely returned.

    \n \section FuncDesc Functional Description

    \n \subsection General General
	The driver supports the transmit/receive of data using the hardwares
	interrupt capabilities.

    \n \subsubsection general_init General Setup and Initialization of the driver and hardware
	At opening of the driver the HW and the driver is beeing initialized with
	default values.
	(see section about \ref scc_register_defaults "M75 SCC register defaults")
	At need this values may be altered
	(see section about \ref getstat_setstat_codes "M75 specific Getstat/Setstat standard codes").

	For verification purposes, the local loopback mode and auto echo can
	be enabled (M75_SCC_REG_14)

	!!! only change the register bits important to you. Bits not used or
	    declared as reserved must remain as defined in section about \ref
		scc_register_defaults "M75 SCC register defaults" !!!

    \n \subsection channels Logical Channels
	Two logical channels are assigned to the two physical communication channels
	of the M75. Every channel has its own, fully supported set of setup
	and read registers.

    \n \subsection communication Communication Principle
    For the user the procedure of receiving data is a follows
    - configuration of the M75 HW and driver with different M_setstat calls
	- enabling receiver
    - waiting for data (using M_getblock (polling) or a signal)
    - reading data through M_getblock

	The procedure of data transmission is as follows:
    - configuration of the M75 HW and driver with different M_setstat calls
    - sending of frames via M_setblock

    \n \subsection receive Receive Data
	First, the hardware and driver have to be set up if the communication
	requirements differ from the default values:
		- select front/rear I/O (M75_IO_SEL)
		- set parity settings (M75_SCC_REG04),
		- set coding method and CRC preset(M75_SCC_REG_10)
		- set clock mode (M75_SCC_REG11, source of Rx clock)
		- set baudrate (M75_BRGEN_TCONST)
		- set Rx buffer queue topology in driver
			- maximum frame number (M75_MAX_RXFRAME_NUM)
			- maximum frame size (M75_MAX_RXFRAME_SIZE)
		- set timeout for M_getstat call (M75_GETBLOCK_TOUT)
		- install Rx signal (M75_SETRXSIG)
		- set receiver parameters (M75_SCC_REG03)
	With the setting of the receiver parameters, the receiver can be enabled
	and is ready for receiving data.
    \n \subsubsection rx_getblock Using M_getblock()
	When no signals are used, M_getblock calls are used to poll driver for
	received data. If no data is present, the driver waits as set up with
	M75_GETBLOCK_TOUT and returns with M75_ERR_RX_QEMPTY or ERR_OSS_TIMEOUT
	if no data is received. If data is received, M_getblock returns the number
	of bytes received.

    \n \subsubsection rx_sig Using Signals
	Using signals, the driver sends a signal each time a frame or an error
	is received. When an error is received, the first M_getblock, after
	detection of the error, returns the error code. Subsequent M_getblock
	calls may pick up already received frames waiting in the Rx queue.

    \n \section interrupts Interrupts
    The driver supports interrupts from the M-Module. The M-Module’s interrupt
    can not be disabled by the application.
	Also the interrupt enable registers must not be changed since the interrupt
	setup is essential for proper function of the driver.

	Each Rx interrupt (special condition only) can trigger the following actions:
	- if end of frame (EOF) is received:
		- put frame into Rx queue of driver
		- send a definable user signal to the application.
		- wake up waiting M_getblock call.
	- if Rx error (Framing/CRC error, Rx overrun, Parity error) is detected:
		- set error flag (returned with waiting or next M_getblock)
		  subsequent M_getblock calls may pick up available frames in Rx queue.
		- send a definable user signal to the application.
		- wake up waiting M_getblock call.
		- no further action (eg. reset FIFOs) is taken.
		  After such an error it is recommended to close the driver and to open
		  it again (performs driver and HW reset).

  	Each Break/Abort interrupt can trigger the following actions:
		- set error flag. The next M_getblock is returned with error,
		  following M_getblock calls may pick up available frames in Rx queue.
		- reset external FIFO and Status FIFO
		- send a definable user signal to the application.
		- wake up waiting M_getblock call.

	Each Tx Underrun/EOM interrupt can trigger the following actions:
		- transmit next frame if available in Tx queue of driver.
		- wake up waiting M_setblock call.

    \n \section signals Signals
    The driver can send signals to notify the application of received data.
	The signals must be activated for each channel via the M75_SETRXSIG SetStat
    code and can be cleared through SetStat M75_CLRRXSIG.

    \n \section id_prom ID PROM
    The M-Module's ID PROM can be checked for validity before the device is
    initialized. You can set the ID_CHECK option in the device descriptor.


    \n \section api_functions Supported API Functions

    <table border="0">
    <tr>
        <td><b>API function</b></td>
        <td><b>Functionality</b></td>
        <td><b>Corresponding low level function</b></td></tr>

    <tr><td>M_open()</td><td>Open device</td><td>M75_Init()</td></tr>

    <tr><td>M_close()     </td><td>Close device             </td>
    <td>M75_Exit())</td></tr>
    <tr><td>M_setstat()   </td><td>Set device parameter     </td>
    <td>M75_SetStat()</td></tr>
    <tr><td>M_getstat()   </td><td>Get device parameter     </td>
    <td>M75_GetStat()</td></tr>
    <tr><td>M_getblock()  </td><td>Block read from device   </td>
    <td>M75_BlockRead()</td></tr>
    <tr><td>M_setblock()  </td><td>Block write from device  </td>
    <td>M75_BlockWrite()</td></tr>
    <tr><td>M_errstringTs() </td><td>Generate error message </td>
    <td>-</td></tr>
    </table>

    \n \section descriptor_entries Descriptor Entries

    The low-level driver initialization routine decodes the following entries
    ("keys") in addition to the general descriptor keys:

    <pre>
    Descriptor Entry		Description
    ----------------		-----------
	MAX_RXFRAME_SIZE		maximum Rx frame size (bytes) to be buffered in driver
							Possible values: 0x01 .. 0x800
							default: 0x800
							may be changed with SetStat M75_MAX_RXFRAME_SIZE
	MAX_RXFRAME_NUM			maximum number of Rx frames to be buffered (queue size)
							Possible values: 0x01 ... system limitations
							default: 0x10
							may be changed with SetStat M75_MAX_RXFRAME_NUM
	MAX_TXFRAME_SIZE		maximum Tx frame size (bytes) to be buffered in driver
							Possible values: 0x01 .. 0x800
							default: 0x800
							may be changed with SetStat M75_MAX_TXFRAME_SIZE
	MAX_TXFRAME_NUM			maximum number of Tx frames to be buffered (queue size)
							Possible values: 0x01 ... system limitations
							default: 0x10
							may be changed with SetStat M75_MAX_TXFRAME_NUM
    </pre>


    \n \section codes M75 specific Getstat/Setstat codes
    see section about \ref getstat_setstat_codes "Getstat/Setstat codes"

    \n \section Documents Overview of all Documents

    \subsection m75_simp  Simple example for using the driver
    m75_simp.c (see example section)

    \subsection m75_async  Simple example for using the driver in async mode
    m75_async.c (see example section)

    \subsection m75_min   Minimum descriptor
    m75_min.dsc
    demonstrates the minimum set of options necessary for using the driver.

    \subsection m75_max   Maximum descriptor
    m75_max.dsc
    shows all possible configuration options for this driver.
*/

/** \example m75_simp.c
Simple example for driver usage
*/

/** \example m75_async.c
Simple example for driver usage in asynchronous mode
*/

/** \page m75dummy MEN logo
  \menimages
*/


