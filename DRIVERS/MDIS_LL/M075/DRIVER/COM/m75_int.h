/***********************  I n c l u d e  -  F i l e  ************************/
/*!
 *        \file  m75_int.h
 *
 *      \author  Christian.Schuster@men.de
 *        $Date: 2009/07/14 17:47:16 $
 *    $Revision: 1.6 $
 *
 *  	 \brief  Internal header file for M75 driver
 *
 *     Switches: -
 */
/*-------------------------------[ History ]---------------------------------
 *
 * $Log: m75_int.h,v $
 * Revision 1.6  2009/07/14 17:47:16  cs
 * R:1. driver used "magic numbers" for register bit settings
 *   2. DBGWRT_* in SCC_* access macros where not interrupt safe
 * M:1. add definitions for some register bits and masks
 *   2.a) changed DBGWRT macros in SCC_* access macros to IDBGWRT_*
 *     b) lowered level for debug outputs to 5-6
 *
 * Revision 1.5  2005/02/07 16:12:40  cs
 * Added switch M75_SUPPORT_BREAK_ABORT (only affects M75_WR15_EN_INT)
 *
 * Revision 1.3  2004/12/29 19:34:54  cs
 * Added define for interrupt enable in WR15
 *
 * Revision 1.2  2004/08/31 12:07:03  cs
 * write access optimized
 * added irqEnabled flag to each channel
 *
 * Revision 1.1  2004/08/06 11:20:19  cs
 * Initial Revision
 *
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2004 by MEN Mikro Elektronik GmbH, Nuernberg, Germany
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

#ifdef __cplusplus
	extern "C" {
#endif

#include <MEN/men_typs.h>   /* system dependend definitions   */
#include <MEN/dbg.h>        /* debug functions                */
#include <MEN/oss.h>        /* oss functions                  */
#include <MEN/maccess.h>    /* hw access macros and types     */
#include <MEN/desc.h>       /* descriptor functions           */
#include <MEN/mdis_api.h>   /* MDIS global defs               */
#include <MEN/mdis_com.h>   /* MDIS common defs               */
#include <MEN/mdis_err.h>   /* MDIS error codes               */
#include <MEN/ll_defs.h>    /* low level driver definitions   */
#include <MEN/modcom.h>    /* low level driver definitions   */

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/* debug settings */
#define DBG_MYLEVEL			llHdl->dbgLevel
#define DBH					llHdl->dbgHdl

/* general MDIS defs */
#define CH_NUMBER			2			/**< number of device channels */
#define USE_IRQ				TRUE		/**< interrupt required  */
#define ADDRSPACE_COUNT		1			/**< nbr of required address spaces */
#define ADDRSPACE_SIZE		0x100		/**< size of address space */

#define MOD_ID_MAGIC		0x5346		/**< ID PROM magic word */
#define MOD_ID_SIZE			128			/**< ID PROM size [bytes] */
#define MOD_ID				75			/**< ID PROM module ID */

/* M75 global registers */
#if (defined(_BIG_ENDIAN_) && !defined(MAC_BYTESWAP))\
	|| (defined(_LITTLE_ENDIAN_) && defined(MAC_BYTESWAP))
#define DATA_REG_A 0x01		/* equivalent to WR08 */
#define DATA_REG_B 0x03		/* equivalent to WR08 */
#define COMM_REG_A 0x05		/* equivalent to WR00 */
#define COMM_REG_B 0x07		/* equivalent to WR00 */
#define FIFO_REG_A 0x09
#define FIFO_REG_B 0x0B
#define FIFO_STATREG_A 0x0D
#define FIFO_STATREG_B 0x0F
#define FIFO_RESET_REG 0x11
#define CHM_REG_A  0x13
#define CHM_REG_B  0x15
#else
#define DATA_REG_A 0x00		/* equivalent to WR08 */
#define DATA_REG_B 0x02		/* equivalent to WR08 */
#define COMM_REG_A 0x04		/* equivalent to WR00 */
#define COMM_REG_B 0x06		/* equivalent to WR00 */
#define FIFO_REG_A 0x08
#define FIFO_REG_B 0x0A
#define FIFO_STATREG_A 0x0C
#define FIFO_STATREG_B 0x0E
#define FIFO_RESET_REG 0x10
#define CHM_REG_A  0x12
#define CHM_REG_B  0x14
#endif

/* SCC register offsets */
#define M75_R00		0x00	/* WR00 & RR00 */
#define M75_R01		0x01	/* WR01 & RR01 */
#define M75_R02		0x02	/* WR02 & RR02 */
#define M75_R03		0x03	/* WR03 & RR03 */
#define M75_R04		0x04	/* WR04 & RR04 */
#define M75_R05		0x05	/* WR05 & RR05 */
#define M75_R06		0x06	/* WR06 & RR06 */
#define M75_R07		0x07	/* WR07 & WR07' & RR07 */
#define M75_R08		0x08	/* WR08 & RR08 */
#define M75_R09		0x09	/* WR09 & RR09 */
#define M75_R10		0x0A	/* WR10 & RR10 */
#define M75_R11		0x0B	/* WR11 & RR11 */
#define M75_R12		0x0C	/* WR12 & RR12 */
#define M75_R13		0x0D	/* WR13 & RR13 */
#define M75_R14		0x0E	/* WR14 & RR14 */
#define M75_R15		0x0F	/* WR15 & RR15 */

/* M75 register bit definitions */
/* global regs */
#define M75_FIFO_STATREG_TXFIFO_EMPTY 0x10	/* Tx FIFOs empty */
#define M75_FIFO_STATREG_TXEN		  0x80	/* enable Tx FIFOs */

/* WR00 */
#define M75_SCC_WR00_POINT_HIGH		0x08	/* Point High */
#define M75_SCC_WR00_RST_EXT_STAT	0x10	/* Reset Ext/Status Interrupts */
#define M75_SCC_WR00_SEND_ABORT		0x18	/* SDLC Send Abort */
#define M75_SCC_WR00_EN_INT_NXT_RX	0x20	/* Enable Int on Next Rx Char */
#define M75_SCC_WR00_RST_TXINT		0x28	/* Reset Tx Int Pending */
#define M75_SCC_WR00_RST_ERROR		0x30	/* Reset Error */
#define M75_SCC_WR00_RST_HIGH_IUS	0x38	/* Reset Highest IUS */

#define M75_SCC_WR00_RST_RX_CRC		0x40	/* Reset Rx CRC Checker */
#define M75_SCC_WR00_RST_TX_CRC		0x80	/* Reset Tx CRC Generator */
#define M75_SCC_WR00_RST_UNDERR_EOM	0xC0	/* Reset Tx Underrun/EOM Latch */

/* WR01 */
#define M75_SCC_WR01_EXT_IE			0x01	/* Ext Int Enable */
#define M75_SCC_WR01_TX_IE			0x02	/* Tx Int Enable */
#define M75_SCC_WR01_PAR_SPEC_COND	0x04	/* Parity is Special Condition */

#define M75_SCC_WR01_RX_FIRST_SPEC_IE 0x08	/* MASK: Rx Int On First Char or Special Condition */
#define M75_SCC_WR01_RX_ALL_SPEC_IE   0x10	/* MASK: Rx Int On All Char or Special Condition */
#define M75_SCC_WR01_RX_SPEC_ONLY_IE  0x18	/* MASK: Rx Int On Special Condition Only */

/* WR03 */
#define M75_SCC_WR03_RX_EN			0x01	/* Rx Enable */

/* WR05 */
#define M75_SCC_WR05_TX_CRC_EN		0x01	/* Tx CRC Enable */
#define M75_SCC_WR05_TX_EN			0x08	/* Tx Enable */
#define M75_SCC_WR05_TX_BPC_MASK	0x60	/* Tx Bits/Character Mask */
#define M75_SCC_WR05_TX_5BPC		0x00	/* Tx 5 Bits(Or Less)/Char */
#define M75_SCC_WR05_TX_6BPC		0x20	/* Tx 6 Bits/Char */
#define M75_SCC_WR05_TX_7BPC		0x40	/* Tx 7 Bits/Char */
#define M75_SCC_WR05_TX_8BPC		0x60	/* Tx 8 Bits/Char */

/* WR07 */
#define M75_SCC_WR07P_TX_FIFO_EMPTY	0x20	/* Tx FIFO Empty */

/* WR09 */
#define M75_SCC_WR09_MIE			0x08	/* Master Interrupt Enable */
#define M75_SCC_WR09_RST_CHB		0x40	/* Channel Reset B */
#define M75_SCC_WR09_RST_CHA		0x80	/* Channel Reset A */
#define M75_SCC_WR09_RST_HW			0xC0	/* Force HW Reset */

/* WR14 */
#define M75_SCC_WR14_BRG_EN			0x01		/* BR Generator Enable */

/* WR15 */
#define M75_SCC_WR15_WR7P_EN		0x01	/* WR7' SDLC Feature Enable */
#define M75_SCC_WR15_SDLC_FIFO_EN	0x04	/* SDLC Status FIFO Enable */
#define M75_SCC_WR15_DCD_IE			0x08	/* DCD Interrupt enable */
#define M75_SCC_WR15_SYNC_HUNT_IE	0x10	/* SYNC/HUNT Interrupt enable */
#define M75_SCC_WR15_CTS_IE			0x20	/* CTS Interrupt enable */
#define M75_SCC_WR15_TX_UNDR_EOM_IE	0x40	/* Tx Underrun/EOM Interrupt enable */
#define M75_SCC_WR15_BREAK_ABORT_IE	0x80	/* Break/Abort Interrupt enable */

/* RR00 */
#define M75_SCC_RR00_RX_CHAR_AVAIL	0x01	/* Rx Character Available */
#define M75_SCC_RR00_ZERO_CNT		0x02	/* Zero Count */
#define M75_SCC_RR00_TX_BUF_EMPTY	0x04	/* Tx Buffer Empty */
#define M75_SCC_RR00_DCD			0x08	/* DCD */
#define M75_SCC_RR00_SYNC_HUNT		0x10	/* Sync/Hunt */
#define M75_SCC_RR00_CTS			0x20	/* CTS */
#define M75_SCC_RR00_TX_UNDR_EOM	0x40	/* Tx Underrun/EOM */
#define M75_SCC_RR00_BREAK_ABORT	0x80	/* Break/Abort */

/* RR01 */
#define M75_SCC_RR01_ALL_SENT		0x01	/* All Sent */
#define M75_SCC_RR01_ERR_PAR		0x10	/* Parity Error */
#define M75_SCC_RR01_ERR_RX_OVR		0x20	/* Rx Overrun Error */
#define M75_SCC_RR01_ERR_CRC_FRM	0x40	/* CRC/Framing Error */
#define M75_SCC_RR01_END_FRAME		0x80	/* End Of Frame (SDLC) */

/* RR03 */
#define M75_SCC_RR03_CHB_EXTSTAT_IP	0x01	/* Channel B Ext/Status Interrupt pending */
#define M75_SCC_RR03_CHB_TX_IP		0x02	/* Channel B Tx Interrupt pending */
#define M75_SCC_RR03_CHB_RX_IP		0x04	/* Channel B Rx Interrupt pending */
#define M75_SCC_RR03_CHA_EXTSTAT_IP	0x08	/* Channel A Ext/Status Interrupt pending */
#define M75_SCC_RR03_CHA_TX_IP		0x10	/* Channel A Tx Interrupt pending */
#define M75_SCC_RR03_CHA_RX_IP		0x20	/* Channel A Rx Interrupt pending */

/* RR07 */
#define M75_SCC_RR07_BC_MASK		0x3F	/* Byte Count Mask */
#define M75_SCC_RR07_FDA			0x40	/* FIFO Data Available */
#define M75_SCC_RR07_FOS			0x80	/* FIFO Overflow Status */

/* other defines */
#define M75_DELAY			100		/* Delay in msec */
#define M75_SYNC_MODE		1		/* Default mode is SYNC */
#define FRAME_NUM_DEF		0x010	/* Default maximum frame number */
#define FRAME_SIZE_DEF		0x800	/* Default maximum frame size */
#define GETSETBLOCK_TOUT	0x000	/* Default maximum frame size */
#define M75_WR01_MASK_INT	0x1F	/* Mask IEs in WR01 */
#define M75_WR15_MASK_INT	0xFA	/* Mask Ext/Status IEs in WR15 */

#ifdef M75_SUPPORT_BREAK_ABORT
	#define M75_WR15_EN_INT	(M75_SCC_WR15_TX_UNDR_EOM_IE | M75_SCC_WR15_BREAK_ABORT_IE)	/* Interrupts to be enabled */
									/* Break/Abort, TxUnderr/EOM */
#else
	#define M75_WR15_EN_INT	(M75_SCC_WR15_TX_UNDR_EOM_IE)	/* Interrupts to be enabled */
									 /* Tx Underrun/EOM */
#endif

/* Macros for accessing SCC registers */
#define WRITE_SCC_COMM( ma,ch,val )						\
	do {												\
		if( ch == 0 ){	/* write to channel A */		\
			MWRITE_D8( ma, COMM_REG_A, val );			\
			IDBGWRT_6( (DBH, "Write COMM_REG_A %02X\n", val) );	\
		} else {		/* write to channel B */		\
			MWRITE_D8( ma, COMM_REG_B, val );			\
			IDBGWRT_6( (DBH, "Write COMM_REG_B %02X\n", val) );	\
		}												\
    } while( 0 )

#define READ_SCC_COMM( ma,ch,val )						\
	do {												\
		if( ch == 0 ){	/* write to channel A */		\
			val = MREAD_D8( ma, COMM_REG_A );	\
			IDBGWRT_6( (DBH, "Read  COMM_REG_A %02X\n", val) );	\
		} else {		/* write to channel B */		\
			val = MREAD_D8( ma, COMM_REG_B );	\
			IDBGWRT_6( (DBH, "Read  COMM_REG_B %02X\n", val) );	\
		}												\
    } while( 0 )

#define WRITE_SCC_REG( ma,ch,reg,val )								\
	do {															\
		OSS_IRQ_STATE irqStateWAcc;									\
		irqStateWAcc = OSS_IrqMaskR(llHdl->osHdl, llHdl->irqHdl);	\
		WRITE_SCC_COMM( ma,ch,reg );								\
		WRITE_SCC_COMM( ma,ch,val );								\
		if( reg == M75_R08) {	/* data register, do nothing more */\
		} else if( reg != M75_R07 )	{ /* normal SCC register */		\
			/* mirror it */											\
			*llHdl->chan[ch].sccRegs.wrp[reg] = val;				\
		} else if( (reg == M75_R07) &&								\
				   (llHdl->chan[ch].sccRegs.wr15 & M75_SCC_WR15_WR7P_EN) ) {/* WR7P */	\
			llHdl->chan[ch].sccRegs.wr7p = val;						\
		} else if( reg == M75_R07 ){					 /* WR07 */	\
			llHdl->chan[ch].sccRegs.wr07 = val;						\
		}															\
		OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqStateWAcc ); \
		IDBGWRT_5( (DBH, "Write ch %d SCC Reg %02d: %02X\n", ch, reg, val) ); \
    } while( 0 )

#define READ_SCC_REG( ma,ch,reg,val )								\
	do {															\
		OSS_IRQ_STATE irqStateRAcc;									\
		irqStateRAcc = OSS_IrqMaskR(llHdl->osHdl, llHdl->irqHdl);	\
		WRITE_SCC_COMM( ma,ch,reg );								\
		READ_SCC_COMM( ma,ch,val );									\
		OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqStateRAcc ); \
		IDBGWRT_5( (DBH, "Read  ch %d SCC Reg %02d: %02X\n", ch, reg, val) ); \
    } while( 0 )

/** Macro to unlock device semaphore */
#define DEVSEM_UNLOCK(llHdl) \
 if( llHdl->devSemHdl ) OSS_SemSignal( llHdl->osHdl, llHdl->devSemHdl );

/** Macro to lock device semaphore */
/* ??? while( error == ERR_OSS_SIG_OCCURED ) might be a problem in Linux???*/
#define DEVSEM_LOCK(llHdl) \
 if( llHdl->devSemHdl ){ \
     int32 error;\
     do {\
         error=OSS_SemWait( llHdl->osHdl, llHdl->devSemHdl, OSS_SEM_WAITFOREVER );\
     } while( error == ERR_OSS_SIG_OCCURED );\
 }

/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/

/** mirror of SCC Register Set object structure */
typedef struct {
	u_int8			wr00;	/**< SCC register WR00, only dummy, not used  */
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
	u_int8			*wrp[16]; /* pointers to above registers */
} SCC_REG;


/** queue entry structure */
typedef struct mqueue_ent {
	struct mqueue_ent	*next;		/**< ptr to next entry */
	u_int8				*frame;		/**< tx/rx frame data */
	u_int32				size;		/**< tx/rx frame size (bytes) */
	u_int8				ready;		/**< flag if frame is ready for sending */
	u_int8				xfering;	/**< flag if frame is just beeing sent/received */
} MQUEUE_ENT;

/** queue header structure */
typedef struct {
	MQUEUE_ENT 	*first;			/**< next entry to read */
	MQUEUE_ENT  *last;			/**< last entry, attach here */
	void		*startAlloc;	/**< pointer to start of queue memory */
	OSS_SEM_HANDLE *sem;		/**< semaphore to wake read/write waiter */
	u_int32		totEntries;		/**< total number of entries */
	u_int32		memAlloc;		/**< size of memory allocated in this queue  */
	u_int32		maxFrameSize;	/**< maximum frame size in buffer */
	u_int32		maxFrameNum;	/**< max. number of frames buffered by driver */
	u_int8		errSent;		/**< flags if overrun error has been sent */
	u_int8		qinit;			/**< flags if queue is ready initialized */
	u_int8		waiting;		/**< flags read/write waiter waiting  */
} MQUEUE_HEAD;

/** per channel object structure */
typedef struct {
	MQUEUE_HEAD		rxQ;			/**< Receive queue header */
	MQUEUE_HEAD		txQ;			/**< Transmit queue header */
	OSS_SIG_HANDLE	*sig;			/**< signal installed */
	u_int32			asyRxSigWaterM;	/**< ASYNC mode: Rx signal water mark */
	int32		rxERR;			/**< if Rx error, error code is stored here */
	SCC_REG		sccRegs;		/**< mirror for some SCC registers/bits */
	u_int32		getBlockTout;	/**< M_getblock timeout, Rx queue is empty */
	u_int32		setBlockTout;	/**< M_setblock timeout, Tx queue is full */
	u_int8		syncMode;		/**< channel mode: SYNC/ASYNC */
	u_int8		irqEnabled;		/**< flags individual interrupts enabled (M75_IRQ_ENABLE) */
	u_int8		txUnderrEOMgot;	/**< flags if Tx Under/EOM int received */
	u_int8		txBufEmpty;		/**< flags if Tx Buffer Empty int received */
	u_int8		rxStatCnt;		/**< counter for Status FIFO entries on HW */
} CHN_OBJ;

/** ll handle */
typedef struct {
	/* general */
    int32           memAlloc;		/**< size allocated for the handle */
    OSS_HANDLE      *osHdl;         /**< oss handle */
    OSS_IRQ_HANDLE  *irqHdl;        /**< irq handle */
    DESC_HANDLE     *descHdl;       /**< desc handle */
    MACCESS         ma;             /**< hw access handle */
	MDIS_IDENT_FUNCT_TBL idFuncTbl;	/**< id function table */
	OSS_SEM_HANDLE	*devSemHdl;		/**< device semaphore handle */
	/* debug */
    u_int32         dbgLevel;		/**< debug level */
	DBG_HANDLE      *dbgHdl;        /**< debug handle */

	CHN_OBJ			chan[2]; 		/**< channel objects, channel A and B */

	u_int8			irqEnabled;		/**< flags global interrupts enabled
	                                  *  (M_MK_IRQ_ENABLE) */
	u_int32			irqCount;		/**< number of irqs occurred  */

	u_int32			idCheck;		/**< ID PROM check enabled */
	u_int32			maxIrqTime;
} LL_HANDLE;

/* include files which need LL_HANDLE */
#include <MEN/ll_entry.h>   /* low-level driver jump table  */
#include <MEN/m75_drv.h>   /* M75 driver header file */


/*--------------------------------------+
|   STATICS                             |
+--------------------------------------*/

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/

#ifdef __cplusplus
	}
#endif






