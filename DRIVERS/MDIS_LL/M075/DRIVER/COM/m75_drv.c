/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!
 *        \file  m75_drv.c
 *
 *      \author  Christian.Schuster@men.de
 *        $Date: 2009/07/14 17:47:14 $
 *    $Revision: 1.5 $
 *
 *      \brief   Low-level driver for M75 module
 *
 *     Required: OSS, DESC, DBG, ID libraries
 *
 *     \switches _ONE_NAMESPACE_PER_DRIVER_
 */
 /*-------------------------------[ History ]--------------------------------
 *
 * $Log: m75_drv.c,v $
 * Revision 1.5  2009/07/14 17:47:14  cs
 * R:1. driver was missing support for ASYNC mode
 *   2. driver used "magic" numbers for register bits
 *   3. Ported to MDIS5
 * M:1.a) parse descriptor key to detect SYNC/ASYNC mode at init
 *     b) added special initialization for ASYNC mode
 *     c) Setstat M_MK_IRQ_ENABLE: only en-/disable master interrupt
 *        leave other bits as set previously
 *   2. use bit definitions from m75_drv.h for register bits
 *      at least where the code is touched anyway
 *   3.a) added support for 64bit (API, pointer casts, ...)
 *     b) adapted DBG prints for 64bit pointers
 *     c) put all MACCESS macros in conditionals in brackets
 *
 * Revision 1.4  2004/12/29 19:34:50  cs
 * bugfixes: it happened that Tx was blocked because no EOM interrupt occurred
 * Break/Abort interrupts and handling disabled.
 * This interrupt handling lead to unconsistencies between status and data FIFOs
 * cosmetics in documentation
 *
 * Revision 1.3  2004/09/03 09:27:46  CSchuster
 * bugfix (keep status fifo and ext fifo consistent after CRC/Framing error)
 * bugfix in M75_IO_SEL SetStat (overwrote CHM bit of channel mode register)
 *
 * Revision 1.2  2004/08/31 12:06:58  cs
 * added M75_IRQ_ENABLE, enhanced Irq enable/disable handling
 * fixed M75_ResetQ (now complete queue is initialized)
 * fixed M75_RedoQ handling
 * added safety checks in SetStats
 * cosmetics in documentation
 *
 * Revision 1.1  2004/08/06 11:20:17  cs
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2004 by MEN Mikro Elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

#define _NO_LL_HANDLE		/* ll_defs.h: don't define LL_HANDLE struct */
#include "m75_int.h"		/* internal include file */
/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/

/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/

/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
static int32 M75_Init(DESC_SPEC *descSpec, OSS_HANDLE *osHdl,
					   MACCESS *ma, OSS_SEM_HANDLE *devSemHdl,
					   OSS_IRQ_HANDLE *irqHdl, LL_HANDLE **llHdlP);
static int32 M75_Exit(LL_HANDLE **llHdlP );
static int32 M75_Read(LL_HANDLE *llHdl, int32 ch, int32 *value);
static int32 M75_Write(LL_HANDLE *llHdl, int32 ch, int32 value);
static int32 M75_SetStat(LL_HANDLE *llHdl, int32 code, int32 ch, INT32_OR_64  value32_or_64);
static int32 M75_GetStat(LL_HANDLE *llHdl, int32 code, int32 ch, INT32_OR_64 *value32_or_64P);
static int32 M75_BlockRead(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
							int32 *nbrRdBytesP);
static int32 M75_BlockWrite(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
							 int32 *nbrWrBytesP);
static int32 M75_Irq(LL_HANDLE *llHdl );
static int32 M75_Info(int32 infoType, ... );

static char* Ident( void );
static int32 Cleanup(LL_HANDLE *llHdl, int32 retCode);

static int32 M75_Tx(LL_HANDLE *llHdl, int32 ch);
static int32 M75_TxFrame_Sync(LL_HANDLE *llHdl, int32 ch);
static int32 M75_TxData_Async(LL_HANDLE *llHdl, int32 ch);
/* static int32 M75_TxFrame_AsyncIrq(LL_HANDLE *llHdl, int32 ch); */

static int32 M75_IrqRx(LL_HANDLE *llHdl, u_int32 ch);
static int32 M75_IrqRx_Frame_Sync(LL_HANDLE *llHdl, u_int32 ch);
static int32 M75_IrqRx_Data_Async(LL_HANDLE *llHdl, u_int32 ch);

#ifdef M75_SUPPORT_BREAK_ABORT
static int32 M75_BreakAbortHandler(LL_HANDLE *llHdl, u_int32 ch);
#endif

static int32 M75_RedoQ(LL_HANDLE *llHdl, MQUEUE_HEAD *compQ);
static int32 M75_ResetQ(MQUEUE_HEAD *compQ);

/****************************** M75_GetEntry ********************************/
/** Initialize driver's jump table
 *
 *  \param drvP     \OUT pointer to the initialized jump table structure
 */
extern void M75_GetEntry( LL_ENTRY* drvP )
{
    drvP->init        = M75_Init;
    drvP->exit        = M75_Exit;
    drvP->read        = M75_Read;
    drvP->write       = M75_Write;
    drvP->blockRead   = M75_BlockRead;
    drvP->blockWrite  = M75_BlockWrite;
    drvP->setStat     = M75_SetStat;
    drvP->getStat     = M75_GetStat;
    drvP->irq         = M75_Irq;
    drvP->info        = M75_Info;
}

/******************************** M75_Init **********************************/
/** Allocate and return low-level handle, initialize hardware
 *
 * The function initializes all channels with the values defined
 * in the descriptor and/or default values\n
 * (see \ref scc_register_defaults "section about M75 SCC register defaults").\n
 * Interrupts are still disabled.\n
 *
 * The following descriptor keys are used:
 *
 * \code
 * Descriptor key        Default          Range
 * --------------------  ---------------  -------------
 * DEBUG_LEVEL_DESC      OSS_DBG_DEFAULT  see dbg.h
 * DEBUG_LEVEL           OSS_DBG_DEFAULT  see dbg.h
 * ID_CHECK              1                0 .. 1
 * MAX_RXFRAME_SIZE      FRAME_SIZE_DEF   1 .. 0x800
 * MAX_TXFRAME_SIZE      FRAME_SIZE_DEF   1 .. 0x800
 * MAX_RXFRAME_NUM       FRAME_NUM_DEF    1 .. system limitations
 * MAX_TXFRAME_NUM       FRAME_NUM_DEF    1 .. system limitations
 * \endcode
 *
 *  \param descP      \IN  pointer to descriptor data
 *  \param osHdl      \IN  oss handle
 *  \param ma         \IN  hw access handle
 *  \param devSemHdl  \IN  device semaphore handle
 *  \param irqHdl     \IN  irq handle
 *  \param llHdlP     \OUT pointer to low-level driver handle
 *
 *  \return           \c 0 on success or error code
 */
static int32 M75_Init(
    DESC_SPEC       *descP,
    OSS_HANDLE      *osHdl,
    MACCESS         *ma,
    OSS_SEM_HANDLE  *devSemHdl,
    OSS_IRQ_HANDLE  *irqHdl,
    LL_HANDLE       **llHdlP
)
{
    LL_HANDLE *llHdl = NULL;
    u_int32 gotsize;
    int32 error=0;
    u_int32 value, irqEnable=0;
	u_int32 ch;

    DBGCMD( static const char functionName[] = "LL - M75_Init()"; )
    /*------------------------------+
    |  prepare the handle           |
    +------------------------------*/
	*llHdlP = NULL;		/* set low-level driver handle to NULL */

	/* alloc */
    if ((llHdl = (LL_HANDLE*)OSS_MemGet(
    				osHdl, sizeof(LL_HANDLE), &gotsize)) == NULL)
       return(ERR_OSS_MEM_ALLOC);

	/* clear */
    OSS_MemFill(osHdl, gotsize, (char*)llHdl, 0x00);

	/* init */
    llHdl->memAlloc   = gotsize;
    llHdl->osHdl      = osHdl;
    llHdl->irqHdl     = irqHdl;
    llHdl->ma		  = *ma;
	llHdl->devSemHdl  = devSemHdl;
    /*------------------------------+
    |  init id function table       |
    +------------------------------*/
	/* driver's ident function */
	llHdl->idFuncTbl.idCall[0].identCall = Ident;
	/* library's ident functions */
	llHdl->idFuncTbl.idCall[1].identCall = DESC_Ident;
	llHdl->idFuncTbl.idCall[2].identCall = OSS_Ident;
	/* terminator */
	llHdl->idFuncTbl.idCall[3].identCall = NULL;

    /*------------------------------+
    |  prepare debugging            |
    +------------------------------*/
	DBG_MYLEVEL = OSS_DBG_DEFAULT;	/* set OS specific debug level */
	DBGINIT((NULL,&DBH));

    /*------------------------------+
    |  scan descriptor              |
    +------------------------------*/
	/* prepare access */
    if ((error = DESC_Init(descP, osHdl, &llHdl->descHdl)))
		return( Cleanup(llHdl,error) );

    /* DEBUG_LEVEL_DESC */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&value, "DEBUG_LEVEL_DESC")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	DESC_DbgLevelSet(llHdl->descHdl, value);	/* set level */

    /* DEBUG_LEVEL */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&llHdl->dbgLevel, "DEBUG_LEVEL")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

    DBGWRT_1((DBH, "%s ma=%08p\n", functionName, llHdl->ma));

    /* ID_CHECK */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&llHdl->idCheck, "ID_CHECK")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

    /* IRQ_ENABLE */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&irqEnable, "IRQ_ENABLE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	for(ch=0; ch < CH_NUMBER; ch++){
		/* SYNC_MODE */
		if ((error = DESC_GetUInt32(llHdl->descHdl, M75_SYNC_MODE,
									&value, "CHAN_%d/SYNC_MODE", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		if( 0 != value ) {
			llHdl->chan[ch].syncMode = M75_SYNC_MODE;
		} else {
			llHdl->chan[ch].syncMode = 0;
			if ((error = DESC_GetUInt32(llHdl->descHdl, M75_SYNC_MODE,
										&value, "CHAN_%d/ASYNC_RX_SIG_MARK", ch)) &&
				error != ERR_DESC_KEY_NOTFOUND)
				return( Cleanup(llHdl,error) );
			llHdl->chan[ch].asyRxSigWaterM = value;
		}
		/* MAX_RXFRAME_SIZE */
		if ((error = DESC_GetUInt32(llHdl->descHdl, FRAME_SIZE_DEF,
									&llHdl->chan[ch].rxQ.maxFrameSize,
									"CHAN_%d/MAX_RXFRAME_SIZE", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		/* MAX_TXFRAME_SIZE */
		if ((error = DESC_GetUInt32(llHdl->descHdl, FRAME_SIZE_DEF,
									&llHdl->chan[ch].txQ.maxFrameSize,
									"CHAN_%d/MAX_TXFRAME_SIZE", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		/* MAX_RXFRAME_NUM */
		if ((error = DESC_GetUInt32(llHdl->descHdl, FRAME_NUM_DEF,
									&llHdl->chan[ch].rxQ.maxFrameNum,
									"CHAN_%d/MAX_RXFRAME_NUM", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		/* MAX_TXFRAME_NUM */
		if ((error = DESC_GetUInt32(llHdl->descHdl, FRAME_NUM_DEF,
									&llHdl->chan[ch].txQ.maxFrameNum,
									"CHAN_%d/MAX_TXFRAME_NUM", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		DBGWRT_3((DBH, "Read Descriptor chan %d:\n"
						"    MAX_RXFRAME_SIZE 0x%04X\n"
						"    MAX_RXFRAME_NUM  0x%04X\n"
						"    MAX_TXFRAME_SIZE 0x%04X\n"
						"    MAX_TXFRAME_NUM  0x%04X\n",
						ch,
						llHdl->chan[ch].rxQ.maxFrameSize,
						llHdl->chan[ch].rxQ.maxFrameNum,
						llHdl->chan[ch].txQ.maxFrameSize,
						llHdl->chan[ch].txQ.maxFrameNum ));
	}

    /*------------------------------+
    |  check module ID              |
    +------------------------------*/
	if (llHdl->idCheck) {
		int modIdMagic = m_read((U_INT32_OR_64)llHdl->ma, 0);
		int modId      = m_read((U_INT32_OR_64)llHdl->ma, 1);

		if (modIdMagic != MOD_ID_MAGIC) {
			DBGWRT_ERR((DBH," *** %s: illegal magic=0x%04x\n", functionName, modIdMagic));
			error = ERR_LL_ILL_ID;
			return( Cleanup(llHdl,error) );
		}
		if (modId != MOD_ID) {
			DBGWRT_ERR((DBH," *** %s: illegal id=%d\n", functionName, modId));
			error = ERR_LL_ILL_ID;
			return( Cleanup(llHdl,error) );
		}
		DBGWRT_3((DBH, "%s: ModIdCheck successful\n", functionName));
		/* dummy read due to HW bug */
		READ_SCC_REG( llHdl->ma, 0, M75_R00, value );

	}

	/*------------------------------+
	|  init M75 hardware and driver |
	+------------------------------*/
	/* init pointers to register mirrors */
	for(ch = 0; ch<CH_NUMBER; ch++){
		llHdl->chan[ch].sccRegs.wrp[0] = &llHdl->chan[ch].sccRegs.wr00;
		llHdl->chan[ch].sccRegs.wrp[1] = &llHdl->chan[ch].sccRegs.wr01;
		llHdl->chan[ch].sccRegs.wrp[2] = &llHdl->chan[ch].sccRegs.wr02;
		llHdl->chan[ch].sccRegs.wrp[3] = &llHdl->chan[ch].sccRegs.wr03;
		llHdl->chan[ch].sccRegs.wrp[4] = &llHdl->chan[ch].sccRegs.wr04;
		llHdl->chan[ch].sccRegs.wrp[5] = &llHdl->chan[ch].sccRegs.wr05;
		llHdl->chan[ch].sccRegs.wrp[6] = &llHdl->chan[ch].sccRegs.wr06;
		llHdl->chan[ch].sccRegs.wrp[7] = &llHdl->chan[ch].sccRegs.wr07;
		llHdl->chan[ch].sccRegs.wrp[8] = &llHdl->chan[ch].sccRegs.wr7p;
		llHdl->chan[ch].sccRegs.wrp[9] = &llHdl->chan[ch].sccRegs.wr09;
		llHdl->chan[ch].sccRegs.wrp[10] = &llHdl->chan[ch].sccRegs.wr10;
		llHdl->chan[ch].sccRegs.wrp[11] = &llHdl->chan[ch].sccRegs.wr11;
		llHdl->chan[ch].sccRegs.wrp[12] = &llHdl->chan[ch].sccRegs.wr12;
		llHdl->chan[ch].sccRegs.wrp[13] = &llHdl->chan[ch].sccRegs.wr13;
		llHdl->chan[ch].sccRegs.wrp[14] = &llHdl->chan[ch].sccRegs.wr14;
		llHdl->chan[ch].sccRegs.wrp[15] = &llHdl->chan[ch].sccRegs.wr15;
	}

	/*------------------------------+
	|  init hardware                |
	+------------------------------*/
	DBGWRT_3((DBH, "%s: init HW\n",	functionName));
	/* reset HW */
	WRITE_SCC_REG( llHdl->ma, 0, M75_R09, 0xC0 );
	READ_SCC_REG( llHdl->ma, 0, M75_R00, value ); /* dummy read */

	/* reset FIFOs */
	MWRITE_D8( llHdl->ma, FIFO_RESET_REG, 0x55 );

	/*----------------------+
    |  init channels (llHdl)|
    +----------------------*/
	for(ch = 0; ch<CH_NUMBER; ch++){

		DBGWRT_3((DBH, "%s: init channel %d\n", functionName, ch));

		/* init flags */
		llHdl->chan[ch].txUnderrEOMgot = 1;
		llHdl->chan[ch].txBufEmpty = 1;

		/* init queues */
		llHdl->chan[ch].getBlockTout = GETSETBLOCK_TOUT;
		llHdl->chan[ch].setBlockTout = GETSETBLOCK_TOUT;

		/* init Rx/Tx semaphores */
		if( (error = OSS_SemCreate( llHdl->osHdl, OSS_SEM_BIN, 1,
			&llHdl->chan[ch].rxQ.sem )) ){

				DBGWRT_ERR((DBH,"*** %s: error 0x%x creating sem\n", functionName, error));
				return( Cleanup(llHdl, error) );
		}
		if( (error = OSS_SemCreate( llHdl->osHdl, OSS_SEM_BIN, 1,
									&llHdl->chan[ch].txQ.sem )) ){

				DBGWRT_ERR((DBH,"*** %s: error 0x%x creating sem\n", functionName, error));
				return( Cleanup(llHdl, error) );
		}

		/* allocate RxQ */
		DBGWRT_3((DBH, "      init queue for channel %d\n", ch));
		if( (error = M75_RedoQ( llHdl, &llHdl->chan[ch].rxQ)) ) {
				return( Cleanup(llHdl, error) );
		}
		llHdl->chan[ch].rxQ.qinit=1;
		if( (error = M75_RedoQ( llHdl, &llHdl->chan[ch].txQ)) ) {
				return( Cleanup(llHdl, error) );
		}
		llHdl->chan[ch].txQ.qinit=1;


		/*------------------------------+
		|  init SCC registers           |
		+------------------------------*/

		/* init SCC WRxx registers */
		DBGWRT_3((DBH, "      init SCC registers for channel %d\n", ch));

		/* globally disable interrupts */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R09, WR09_DEFAULT  & ~M75_SCC_WR09_MIE);

		if( M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
			/* init Channel mode register */
			/* front IO, sync */
			MWRITE_D8(llHdl->ma, CHM_REG_A+(ch<<1), 0x00);
			/* tx/rx config; set SDLC mode */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R04, WR04_DEFAULT );
			/* Rx control; disable Rx while init*/
			WRITE_SCC_REG( llHdl->ma, ch, M75_R03, WR03_DEFAULT );
			/* Tx control; disable Tx while init*/
			WRITE_SCC_REG( llHdl->ma, ch, M75_R05, WR05_DEFAULT );
			/* clock mode; Rx: RTxC pin, Tx: BR Gen, TxC Out: BR Gen */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R11, WR11_DEFAULT );
		} else {
			/* init Channel mode register */
			/* front IO, async */
			MWRITE_D8(llHdl->ma, CHM_REG_A+(ch<<1), 0x01);
			/* tx/rx config; set async mode */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R04, WR04_DEFAULT_ASY );
			/* Rx control; disable Rx while init*/
			WRITE_SCC_REG( llHdl->ma, ch, M75_R03, WR03_DEFAULT_ASY );
			/* Tx control; disable Tx while init, no CRC*/
			WRITE_SCC_REG( llHdl->ma, ch, M75_R05, WR05_DEFAULT_ASY );
			/* clock mode; Rx: RTxC pin, Tx: BR Gen, TxC Out: BR Gen */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R11, WR11_DEFAULT_ASY );
		}

		/* interrupt vector, not used */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R02, WR02_DEFAULT );
		/* SDLC address reg, not used */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R06, WR06_DEFAULT );
		/* Ext/Status source reg / WR7' feature; disable WR7' */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15, 0x00 );
		/* SDLC flag */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R07, WR07_DEFAULT );
		/* tx/rx config; CRC preset 1, NRZ, others disable */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R10, WR10_DEFAULT );
		/* Ext/Status source reg / WR7' feature;
		* enable SDLC status FIFO, enable WR7' */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15, (M75_SCC_WR15_SDLC_FIFO_EN | M75_SCC_WR15_WR7P_EN));
		/* extended features / extended read */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R07, WR7P_DEFAULT );
		/* misc control; disable BR Gen */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R14, WR14_DEFAULT & ~(M75_SCC_WR14_BRG_EN));

		if( M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
			/* clock mode; Rx: RTxC pin, Tx: BR Gen, TxC Out: BR Gen */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R11, WR11_DEFAULT );
		} else {
			/* clock mode; Rx: BR Gen, Tx: BR Gen, TxC Out: BR Gen */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R11, WR11_DEFAULT_ASY );
		}
		/* lower Byte of time constant for BR Gen */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R12, WR12_DEFAULT );
		/* upper Byte of time constant for BR Gen */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R13, WR13_DEFAULT );
		/* misc control; enable BR Gen */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R14, WR14_DEFAULT );
		/* External/Status source control, Status Fifo En, WR7' */
		/* disable all external/status interrupts */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
						WR15_DEFAULT & ~M75_WR15_MASK_INT );
		/* disable Tx FIFO */
		MWRITE_D8( llHdl->ma, FIFO_STATREG_A+(ch<<1), 0x00 );
		/* reset Ext/Status Int */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_EXT_STAT );
		/* SCC various interrupt control */
		/* all interrupts disabled */
		if( M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
			WRITE_SCC_REG( llHdl->ma, ch, M75_R01,
							WR01_DEFAULT & ~M75_WR01_MASK_INT );
		} else {
			WRITE_SCC_REG( llHdl->ma, ch, M75_R01,
							WR01_DEFAULT_ASY & ~M75_WR01_MASK_INT );
		}
	}

	*llHdlP = llHdl;	/* set low-level driver handle */
	return(ERR_SUCCESS);
} /* M75_Init */

/****************************** M75_Exit ************************************/
/** De-initialize hardware and clean up memory
 *
 *  The function deinitializes all channels by disabling the communication and
 *  the interrupts.
 *
 *  \param llHdlP      \IN  pointer to low-level driver handle
 *
 *  \return           \c 0 on success or error code
 */
static int32 M75_Exit(
   LL_HANDLE    **llHdlP
)
{
    LL_HANDLE *llHdl = *llHdlP;
	int32 error = 0, ch;

    DBGWRT_1((DBH, "LL - M75_Exit\n"));

    /*------------------------------+
    |  de-init hardware             |
    +------------------------------*/
	/* disabling receiver and transmitter */
	for( ch=0; ch< CH_NUMBER; ch++ ) {
		WRITE_SCC_REG( llHdl->ma, ch, M75_R03, 0x00 );
		WRITE_SCC_REG( llHdl->ma, ch, M75_R05, 0x00 );
	}

    /*------------------------------+
    |  clean up memory               |
    +------------------------------*/
	/* while first->next {this = first; first = first->next; free this */
	/* free all four buffer queues */

	*llHdlP = NULL;		/* set low-level driver handle to NULL */
	error = Cleanup(llHdl,error);

	return(error);
}

/****************************** M75_Read ************************************/
/** Read a value from the device, not supported by this driver.*/
static int32 M75_Read(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 *valueP
)
{
	return(ERR_LL_ILL_FUNC);
}

/****************************** M75_Write ***********************************/
/** Write a value to the device, not supported by this driver */
static int32 M75_Write(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 value
)
{
	return(ERR_LL_ILL_FUNC);
}

/****************************** M75_SetStat *********************************/
/** Set the driver status
 *
 *  The driver supports \ref getstat_setstat_codes "these status codes"
 *  in addition to the standard codes (see mdis_api.h).
 *
 *  \param llHdl  	     \IN  low-level handle
 *  \param code          \IN  \ref getstat_setstat_codes "status code"
 *  \param ch            \IN  current channel
 *  \param value32_or_64 \IN  data or
 *                            pointer to block data structure (M_SG_BLOCK)
 *                            for block status codes
 *  \return              \c 0 on success or error code
 */
static int32 M75_SetStat(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64  value32_or_64
)
{
	int32 error = ERR_SUCCESS;
	u_int8 ich, wr14, retVal = 0;
	int32		value	= (int32)value32_or_64;	/* 32bit value     */
	/*INT32_OR_64	valueP  = value32_or_64;*/		/* stores 32/64bit pointer */


    DBGCMD( static const char functionName[] = "LL - M75_SetStat()"; )
    DBGWRT_1((DBH, "%s: ch=%d code=0x%04x value=%08p\n",
			  functionName, ch, code, value32_or_64));

    switch(code) {
        /*--------------------------+
        |  debug level              |
        +--------------------------*/
        case M_LL_DEBUG_LEVEL:
            llHdl->dbgLevel = value;
            break;
        /*--------------------------+
        |  enable interrupts        |
        +--------------------------*/
        case M_MK_IRQ_ENABLE:
			if( value ) {
				/* reset and interrupt control reg */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R09,
								llHdl->chan[ch].sccRegs.wr09 | M75_SCC_WR09_MIE );
				llHdl->irqEnabled = TRUE;
			} else {
				llHdl->irqEnabled = FALSE;
				/* reset and interrupt control reg;
				 * disable interrupts */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R09,
								llHdl->chan[ch].sccRegs.wr09 & ~M75_SCC_WR09_MIE );
			}
            break;
        /*-------------------------------+
        |  enable M75 board interrupts   |
        +-------------------------------*/
        case M75_IRQ_ENABLE:
			if( value ) {
				/* SCC various interrupt control; */
				if( M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
					WRITE_SCC_REG( llHdl->ma, ch, M75_R01,
									(llHdl->chan[ch].sccRegs.wr01 | M75_SCC_WR01_EXT_IE | M75_SCC_WR01_RX_SPEC_ONLY_IE) );
				} else { /* async */
					WRITE_SCC_REG( llHdl->ma, ch, M75_R01,
									(llHdl->chan[ch].sccRegs.wr01 | M75_SCC_WR01_EXT_IE | M75_SCC_WR01_TX_IE | M75_SCC_WR01_RX_ALL_SPEC_IE) );
				}
				/* Ext/Status source reg / WR7' feature;
				 * enable individual ext/status interrupts */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
							   (llHdl->chan[ch].sccRegs.wr15 | M75_WR15_EN_INT) );
				llHdl->chan[ch].irqEnabled = TRUE;
			} else {
				llHdl->chan[ch].irqEnabled = FALSE;
				/* SCC various interrupt control;
				 * Rx Int disable, Ext Int disable, Tx Int disable */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R01,
								llHdl->chan[ch].sccRegs.wr01 & ~M75_WR01_MASK_INT );
				/* Ext/Status source reg / WR7' feature;
				 * disable all Ext/Status interrupts */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
								llHdl->chan[ch].sccRegs.wr15 & ~M75_WR15_MASK_INT );
			}
            break;
        /*--------------------------+
        |  set irq counter          |
        +--------------------------*/
        case M_LL_IRQ_COUNT:
            llHdl->irqCount = value;
            break;
		/*--------------------------+
		|  set SCC registers WRxx   |
		+--------------------------*/
		case M75_SCC_REG_00:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, (u_int8)value );
			break;
		case M75_SCC_REG_01:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R01, (u_int8)value );
			break;
		case M75_SCC_REG_02:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R02, (u_int8)value );
			break;
		case M75_SCC_REG_03:
			if( !(llHdl->chan[ch].sccRegs.wr03 & M75_SCC_WR03_RX_EN) &&
				( value & 0x01 ) ) {/* enabling receiver */
				if( !llHdl->chan[ch].irqEnabled ) {
					/* if irqs not enabled, driver won't work */
					DBGWRT_ERR((DBH, "*** ERROR - %s: enabling Rx with interrupts disabled will not work!!! \n"
								     "\t please use M_Setstats M_MK_IRQ_ENABLE and M75_IRQ_ENABLE \n"
									 "\t to enable interrupts first!!!", functionName, code) );
					return( M75_ERR_INT_DISABLED );
				}

				/* reset Status FIFO first (disable here) */
				if( llHdl->chan[ch].sccRegs.wr15 & M75_SCC_WR15_SDLC_FIFO_EN )
					WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
									llHdl->chan[ch].sccRegs.wr15 & (M75_SCC_WR15_WR7P_EN | M75_SCC_WR15_SDLC_FIFO_EN | M75_SCC_WR15_DCD_IE) );

				/* reset external Rx FIFO */
				MWRITE_D8( llHdl->ma, FIFO_RESET_REG, 0x04<<(ch*4) );
			}

			/* when Rx Enabled, Status FIFO has always to be enabled */
			if( value & 0x01 )
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
								llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_SDLC_FIFO_EN );

			WRITE_SCC_REG( llHdl->ma, ch, M75_R03, (u_int8)value );
			break;
		case M75_SCC_REG_04:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R04, (u_int8)value );
			break;
		case M75_SCC_REG_05:
			if( !(llHdl->chan[ch].sccRegs.wr05 & 0x08) &&
				(value & 0x08 ) &&/* enabling transmitter */
				!llHdl->chan[ch].irqEnabled )
			{	/* if irqs not enabled, driver won't work */
				return( M75_ERR_INT_DISABLED );
			}
			WRITE_SCC_REG( llHdl->ma, ch, M75_R05, (u_int8)value );
			break;
		case M75_SCC_REG_06:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R06, (u_int8)value );
			break;
		case M75_SCC_REG_07:
			/* disable WR7' feature */
			if( llHdl->chan[ch].sccRegs.wr15 & M75_SCC_WR15_WR7P_EN ) /* is WR7' enabled? */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
								llHdl->chan[ch].sccRegs.wr15 & ~(M75_SCC_WR15_WR7P_EN) );

			WRITE_SCC_REG( llHdl->ma, ch, M75_R07, (u_int8)value );
			break;
		case M75_SCC_REG_7P:
			/* enable WR7' feature */
			if( !(llHdl->chan[ch].sccRegs.wr15 & M75_SCC_WR15_WR7P_EN) ) /* is WR7' disabled? */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
								(llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_WR7P_EN) );

			WRITE_SCC_REG( llHdl->ma, ch, M75_R07, (u_int8)value );
			break;
		case M75_SCC_REG_08:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R08, (u_int8)value );
			break;
		case M75_SCC_REG_09:
			if( (llHdl->irqEnabled && !(value && M75_SCC_WR09_MIE)) ||
				(!llHdl->irqEnabled && (value && M75_SCC_WR09_MIE)) ) {
				DBGWRT_ERR((DBH, "*** ERROR - %s: %0x04X\n"
								 "\t please use M_Setstats M_MK_IRQ_ENABLE and M75_IRQ_ENABLE"
								 "to enable/disable interrupts", functionName, code) );
			}
			if( llHdl->irqEnabled && (value & 0x10) ) {
				DBGWRT_ERR((DBH, "*** ERROR - %s: \n"
								 "\t when interrupts are enabled, M75_SCC_REG_09 StatusHigh/Low must always be 0\n",
								 functionName) );
				value = (value & 0xEF );
			}
			WRITE_SCC_REG( llHdl->ma, ch, M75_R09, (u_int8)value );
			/* if a reset is performed */
			/* set the reset values in the local SCC reg mirrors */
			if( value & M75_SCC_WR09_RST_CHA ) {			/* reset chan A */
				llHdl->chan[0].sccRegs.wr05 =
					llHdl->chan[0].sccRegs.wr05 & (M75_SCC_WR05_TX_8BPC | M75_SCC_WR05_TX_CRC_EN);
				llHdl->chan[0].sccRegs.wr14 =
					(llHdl->chan[0].sccRegs.wr14 & 0xC3) | 0x20;
				llHdl->chan[0].sccRegs.wr7p = M75_SCC_WR07P_TX_FIFO_EMPTY;
				/* reset queue */
				M75_ResetQ(&llHdl->chan[0].rxQ);	/* reset RxQ */
				M75_ResetQ(&llHdl->chan[0].txQ);	/* reset TxQ */
				if( (llHdl->chan[0].sccRegs.wr01 != WR01_DEFAULT) ||
					(llHdl->chan[0].sccRegs.wr15 != WR15_DEFAULT) ) {
					DBGWRT_ERR((DBH, "*** ERROR - %s: %0x04X, reset channel 0\n"
									 "WR01 and/or WR15 differ from recommended values\n"
									 "\t WR01 should be 0x%02X,\t is 0x%02X\n"
									 "\t WR15 should be 0x%02X,\t is 0x%02X\t***\n",
									 functionName, code,
									 WR01_DEFAULT, llHdl->chan[ch].sccRegs.wr01,
									 WR15_DEFAULT, llHdl->chan[ch].sccRegs.wr15 ));
				}
			} else if ( value & M75_SCC_WR09_RST_CHB ) {	/* reset chan B */
				llHdl->chan[1].sccRegs.wr05 = llHdl->chan[1].sccRegs.wr05 & (M75_SCC_WR05_TX_8BPC | M75_SCC_WR05_TX_CRC_EN);
				llHdl->chan[1].sccRegs.wr14 = (llHdl->chan[1].sccRegs.wr14 & 0xC3) | 0x20;
				llHdl->chan[1].sccRegs.wr7p = M75_SCC_WR07P_TX_FIFO_EMPTY;
				/* reset queue */
				M75_ResetQ(&llHdl->chan[1].rxQ);	/* reset RxQ */
				M75_ResetQ(&llHdl->chan[1].txQ);	/* reset TxQ */
			} else if ( value & M75_SCC_WR09_RST_HW ) {	/* HW reset */
				for(ich=0; ich<CH_NUMBER; ich++){
					llHdl->chan[ich].sccRegs.wr05 = llHdl->chan[ich].sccRegs.wr05 & (M75_SCC_WR05_TX_8BPC | M75_SCC_WR05_TX_CRC_EN);
					llHdl->chan[ich].sccRegs.wr7p = M75_SCC_WR07P_TX_FIFO_EMPTY;
					llHdl->chan[ich].sccRegs.wr14 = (llHdl->chan[ich].sccRegs.wr14 & 0xC0) | 0x30;
					/* reset queues */
					M75_ResetQ(&llHdl->chan[ich].rxQ);	/* reset RxQ */
					M75_ResetQ(&llHdl->chan[ich].txQ);	/* reset TxQ */
				}
			}
			break;
 		case M75_SCC_REG_10:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R10, (u_int8)value );
			break;
 		case M75_SCC_REG_11:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R11, (u_int8)value );
			break;
		case M75_SCC_REG_12:
			/* disable BR Gen*/
			wr14 = llHdl->chan[ch].sccRegs.wr14;
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, wr14 & ~(M75_SCC_WR14_BRG_EN));

			WRITE_SCC_REG( llHdl->ma, ch, M75_R12, (u_int8)value );

			/* restore WR14 */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, wr14);
			break;
		case M75_SCC_REG_13:
			/* disable BR Gen */
			wr14 = llHdl->chan[ch].sccRegs.wr14;
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, wr14 & ~(M75_SCC_WR14_BRG_EN));

			WRITE_SCC_REG( llHdl->ma, ch, M75_R13, (u_int8)value );

			/* restore WR14 */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, wr14);
			break;
		case M75_BRGEN_TCONST:
			/* disable BR Gen */
			wr14 = llHdl->chan[ch].sccRegs.wr14;
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, wr14 & ~(M75_SCC_WR14_BRG_EN));

			WRITE_SCC_REG( llHdl->ma, ch, M75_R12, (u_int8)value );
			WRITE_SCC_REG( llHdl->ma, ch, M75_R13, (u_int8)(value>>8) );

			/* restore WR14 */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, wr14);
			break;
		case M75_SCC_REG_14:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R14, (u_int8)value );
			break;
		case M75_SCC_REG_15:
			WRITE_SCC_REG( llHdl->ma, ch, M75_R15, (u_int8)value );
			break;
		/*--------------------------+
		|  set M75 global registers |
		+--------------------------*/
		case M75_FIFO_DATA:
			MWRITE_D8( llHdl->ma, FIFO_REG_A+(ch<<1), (u_int8)value );
			break;
		case M75_FIFO_RESET:
			if( value ) {
				/* mask value with 0x05, only current channel is set */
				MWRITE_D8( llHdl->ma, FIFO_RESET_REG, (((u_int8)value & 0x05) << (ch*4)) );
				if( value & 0x4 ) /* reset RxQ */
					M75_ResetQ(	&llHdl->chan[ch].rxQ );

				if( value & 0x1 ) /* reset TxQ */
					M75_ResetQ(	&llHdl->chan[ch].txQ );
			}
			break;
		case M75_TXEN:
			if( (value != 0) && (value != 1) )
				return( ERR_LL_ILL_PARAM );
			MWRITE_D8( llHdl->ma, FIFO_STATREG_A+(ch<<1), (u_int8)value<<7 );
			break;
		case M75_RXEN:
			if( value ) {
				WRITE_SCC_REG( llHdl->ma, ch, M75_R03, (llHdl->chan[ch].sccRegs.wr03 | M75_SCC_WR03_RX_EN) );
			} else {
				WRITE_SCC_REG( llHdl->ma, ch, M75_R03, (llHdl->chan[ch].sccRegs.wr03 & ~(M75_SCC_WR03_RX_EN)) );
			}
			break;
		case M75_IO_SEL:
			if( (value != 0) && (value != 1) ) return( ERR_LL_ILL_PARAM );
                        retVal = MREAD_D8( llHdl->ma, CHM_REG_A+(ch<<1) );
                        MWRITE_D8( llHdl->ma, CHM_REG_A+(ch<<1), (((u_int8)value<<1) | (retVal & 0x01)) );
			break;
		case M75_GETBLOCK_TOUT:
			if( value < -1 ) /* sanity check */
				return(M75_ERR_BADPARAMETER);
			llHdl->chan[ch].getBlockTout = value;
			break;
		case M75_SETBLOCK_TOUT:
			if( value < -1 ) /* sanity check */
				return(M75_ERR_BADPARAMETER);
			llHdl->chan[ch].setBlockTout = value;
			break;
		case M75_MAX_RXFRAME_SIZE:
		{
			/* !!! all data is lost !!!*/

			 /* sanity checks */
			if( value <= 0x00 )
				return(M75_ERR_BADPARAMETER);

			if( (u_int32)value == llHdl->chan[ch].rxQ.maxFrameSize )
				break;

			llHdl->chan[ch].rxQ.qinit = 0; /* take queue offline */
			llHdl->chan[ch].rxQ.maxFrameSize = (u_int32)value;

			if( (error = M75_RedoQ( llHdl, &llHdl->chan[ch].rxQ)) ) {
				return( error );
			}
			llHdl->chan[ch].rxQ.qinit++; /* take queue back online */

			break;
		}
		case M75_MAX_RXFRAME_NUM:
		{
			/* !!! all data is lost !!!*/

			if( value <= 0x00 ) /* sanity checks */
				return(M75_ERR_BADPARAMETER);

			if( (u_int32)value == llHdl->chan[ch].rxQ.maxFrameNum )
				break;

			llHdl->chan[ch].rxQ.qinit = 0; /* take queue offline */
			llHdl->chan[ch].rxQ.maxFrameNum = (u_int32)value;

			if( (error = M75_RedoQ( llHdl, &llHdl->chan[ch].rxQ )) ) {
				return( error );
			}

			llHdl->chan[ch].rxQ.qinit++; /* take queue back online */

			break;
		}
		case M75_MAX_TXFRAME_SIZE:
		{
			/* !!! all data is lost !!!*/

			 /* sanity checks */
			if( value <= 0x00 )
				return(M75_ERR_BADPARAMETER);

			if( (u_int32)value == llHdl->chan[ch].txQ.maxFrameSize )
				break;

			llHdl->chan[ch].txQ.qinit = 0; /* take queue offline */
			llHdl->chan[ch].txQ.maxFrameSize = (u_int32)value;

			if( (error = M75_RedoQ( llHdl, &llHdl->chan[ch].txQ)) ) {
				return( error );
			}

			llHdl->chan[ch].txQ.qinit++; /* take queue back online */

			break;
		}
		case M75_MAX_TXFRAME_NUM:
		{
			/* !!! all data is lost !!!*/

			if( value <= 0x00 ) /* sanity check */
				return(M75_ERR_BADPARAMETER);

			if( (u_int32)value == llHdl->chan[ch].txQ.maxFrameNum )
				break;

			llHdl->chan[ch].txQ.qinit = 0; /* take queue offline */
			llHdl->chan[ch].txQ.maxFrameNum = (u_int32)value;

			if( (error = M75_RedoQ( llHdl, &llHdl->chan[ch].txQ)) ) {
				return( error );
			}

			llHdl->chan[ch].txQ.qinit++; /* take queue back online */

			break;
		}
		case M75_SETRXSIG:
			if( llHdl->chan[ch].sig != NULL )
				return( M75_ERR_SIGBUSY );

			if( (error = OSS_SigCreate( llHdl->osHdl,
										value,
										&llHdl->chan[ch].sig )))
				llHdl->chan[ch].sig = NULL;
			break;
		case M75_CLRRXSIG:
			if( llHdl->chan[ch].sig == NULL )
				return( M75_ERR_SIGBUSY );

			error = OSS_SigRemove( llHdl->osHdl,
									&llHdl->chan[ch].sig );
			break;

		/*--------------------------+
		|  (unknown)                |
        +--------------------------*/

        default:
            error = ERR_LL_UNK_CODE;
    }

	return(error);
} /* M75_SetStat */

/****************************** M75_RedoQ *********************************/
/** Free, allocate or reallocate memory for queue
 *
 *  When maxFrameSize or maxFrameNum of any queue changes, this function
 *  adapts the queue to the new values (taken from llHdl)
 *
 *  \param llHdl	\IN  low-level handle
 *  \param compQ	\IN  queue header of queue to be processed
 *  \return			\c   0 on success or error code
 */
static int32 M75_RedoQ(
    LL_HANDLE *llHdl,
	MQUEUE_HEAD *compQ)
{

	u_int32 gotsize, i;
	MQUEUE_ENT *qEnt;
	u_int8 *datap;

	/* delete old queue if existant */
	if( compQ->first ) {
		compQ->totEntries = 0;
		/* free all allocated memory in queue */
		OSS_MemFree(llHdl->osHdl, compQ->startAlloc, compQ->memAlloc);
		compQ->startAlloc = NULL;
		compQ->memAlloc = 0;
		compQ->first = NULL;
		compQ->last  = NULL;
	}

	/* allocate new queue */
	/*  _________________________________________________________________
	 * |1|2|3|.|.|.|.| | |  1  |  2  |  3  |  .  |  .  |  .  |     |     |
	 * | queue structure |              queue frame data                 |
	 * |_________________|_______________________________________________| */
	if ((compQ->startAlloc = (MQUEUE_ENT *)OSS_MemGet(llHdl->osHdl,
				compQ->maxFrameNum * (sizeof(MQUEUE_ENT) + compQ->maxFrameSize),
				&gotsize)) == NULL) {
		compQ->maxFrameSize = 0;
		compQ->maxFrameNum = 0;
		return( ERR_OSS_MEM_ALLOC );
	}
	compQ->memAlloc = gotsize;
	/* init queue entries to 0, data fields not initialized */
	OSS_MemFill(llHdl->osHdl,
				compQ->maxFrameNum * sizeof(MQUEUE_ENT),
				(char*)compQ->startAlloc, 0x00);

	compQ->first = (MQUEUE_ENT*)compQ->startAlloc;
	qEnt = compQ->first;
	/* queue data entries start at end of queue structure entries */
	datap = (u_int8*)(qEnt + compQ->maxFrameNum);

	/* init queue */
	compQ->last = qEnt++;
	compQ->last->ready = 0;
	compQ->last->frame = datap;
	for(i = 1; i < compQ->maxFrameNum; i++){
		compQ->last->next = qEnt; /* attach to last */
		compQ->last = qEnt++;
		compQ->last->ready = 0;
		datap += compQ->maxFrameSize;
		compQ->last->frame = datap;
	}


	/* make queue wrap around */
	compQ->last->next = compQ->first;
	/* queue empty => last points to same as first */
	compQ->last = compQ->first;
	return( ERR_SUCCESS );


}


/****************************** M75_ResetQ *********************************/
/** Reset queue
 *
 *  \param compQ	\IN  queue header of queue to be processed
 *  \return			\c   0 on success or error code
 */
static int32 M75_ResetQ(
	MQUEUE_HEAD *compQ
)
{

	/* take queue offline */
	compQ->qinit--;
	if(compQ->first) { /* if queue exists */
		compQ->last = compQ->first;
		do {
			compQ->first=compQ->first->next;
			compQ->first->size=0;
			compQ->first->ready=0;
		} while(compQ->first != compQ->last);
		compQ->totEntries=0;
	}

	/* take queue back online */
	compQ->qinit++;

	return( ERR_SUCCESS );

}
/****************************** M75_GetStat *********************************/
/** Get the driver status
 *
 *  The driver supports \ref getstat_setstat_codes "these status codes"
 *  in addition to the standard codes (see mdis_api.h).
 *
 *  \param llHdl      \IN  low-level handle
 *  \param code       \IN  \ref getstat_setstat_codes "status code"
 *  \param ch         \IN  current channel
 *  \param valueP     \IN  pointer to block data structure (M_SG_BLOCK) for
 *                         block status codes
 *  \param value32_or_64P \OUT data pointer or pointer to block data structure
 *                             (M_SG_BLOCK) for block status codes
 *
 *  \return           \c 0 on success or error code
 */
static int32 M75_GetStat(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64 *value32_or_64P
)
{
	int32		*valueP		= (int32*)value32_or_64P;		/* pointer to 32bit value */
	INT32_OR_64	*value64P	= value32_or_64P;		  		/* stores 32/64bit pointer */
	M_SG_BLOCK  *blk 		= (M_SG_BLOCK*)value32_or_64P;	/* stores block struct pointer */

	u_int32 retVal = 0, retVal1 = 0;
	int32 error = ERR_SUCCESS;

    DBGCMD( static const char functionName[] = "LL - M75_GetStat()"; )
    DBGWRT_1((DBH, "%s: ch=%d code=0x%04x\n", functionName, ch, code) );

    switch(code)
    {
        /*--------------------------+
        |  debug level              |
        +--------------------------*/
        case M_LL_DEBUG_LEVEL:
            *valueP = llHdl->dbgLevel;
            break;
        /*--------------------------+
        |  number of channels       |
        +--------------------------*/
        case M_LL_CH_NUMBER:
            *valueP = CH_NUMBER;
            break;
        /*--------------------------+
        |  channel direction        |
        +--------------------------*/
        case M_LL_CH_DIR:
            *valueP = M_CH_INOUT;
            break;
        /*--------------------------+
        |  channel length [bits]    |
        +--------------------------*/
        case M_LL_CH_LEN:
            *valueP = 32;
            break;
        /*--------------------------+
        |  channel type info        |
        +--------------------------*/
        case M_LL_CH_TYP:
            *valueP = M_CH_BINARY;
            break;
        /*--------------------------+
        |  irq counter              |
        +--------------------------*/
        case M_LL_IRQ_COUNT:
            *valueP = llHdl->irqCount;
            break;
        /*--------------------------+
        |  ID PROM check enabled    |
        +--------------------------*/
        case M_LL_ID_CHECK:
            *valueP = llHdl->idCheck;
            break;
        /*--------------------------+
        |   ID PROM size            |
        +--------------------------*/
        case M_LL_ID_SIZE:
            *valueP = MOD_ID_SIZE;
            break;
        /*--------------------------+
        |   ID PROM data            |
        +--------------------------*/
        case M_LL_BLK_ID_DATA:
		{
			u_int8 n;
			u_int16 *dataP = (u_int16*)blk->data;

			if (blk->size < MOD_ID_SIZE)		/* check buf size */
				return(ERR_LL_USERBUF);

			for (n=0; n<MOD_ID_SIZE/2; n++)		/* read MOD_ID_SIZE/2 words */
				*dataP++ = (u_int16)m_read((U_INT32_OR_64)llHdl->ma, n);
			READ_SCC_REG( llHdl->ma, ch, M75_R00, retVal ); /* dummy read */

			break;
		}
        /*--------------------------+
        |   ident table pointer     |
        |   (treat as non-block!)   |
        +--------------------------*/
        case M_MK_BLK_REV_ID:
           *value64P  = (INT32_OR_64)&llHdl->idFuncTbl;
           break;
		/*--------------------------+
		|  get SCC registers RRxx   |
		+--------------------------*/
		case M75_SCC_REG_00:
			READ_SCC_REG( llHdl->ma, ch, M75_R00, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_01:
			READ_SCC_REG( llHdl->ma, ch, M75_R01, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_02:
			READ_SCC_REG( llHdl->ma, ch, M75_R02, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_03:
			if(ch == 0) {
				READ_SCC_REG( llHdl->ma, ch, M75_R03, retVal );
				*valueP = retVal;
			} else {
				DBGWRT_ERR((DBH, "*** ERROR - %s: access reg RR03 chan 1"
								 "(line %d)***\n", functionName, __LINE__));
				error = ERR_LL_UNK_CODE;
			}
			break;
		case M75_SCC_REG_04:
			READ_SCC_REG( llHdl->ma, ch, M75_R04, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_05:
			READ_SCC_REG( llHdl->ma, ch, M75_R05, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_06:
			READ_SCC_REG( llHdl->ma, ch, M75_R06, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_07:
			READ_SCC_REG( llHdl->ma, ch, M75_R07, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_08:
			READ_SCC_REG( llHdl->ma, ch, M75_R08, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_09:
			READ_SCC_REG( llHdl->ma, ch, M75_R09, retVal );
			*valueP = retVal;
			break;
 		case M75_SCC_REG_10:
			READ_SCC_REG( llHdl->ma, ch, M75_R10, retVal );
			*valueP = retVal;
			break;
 		case M75_SCC_REG_11:
			READ_SCC_REG( llHdl->ma, ch, M75_R11, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_12:
			READ_SCC_REG( llHdl->ma, ch, M75_R12, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_13:
			READ_SCC_REG( llHdl->ma, ch, M75_R13, retVal );
			*valueP = retVal;
			break;
		case M75_BRGEN_TCONST:
			READ_SCC_REG( llHdl->ma, ch, M75_R12, retVal );
			READ_SCC_REG( llHdl->ma, ch, M75_R13, retVal1 );
			*valueP = (retVal1<<8)+retVal;
			break;
		case M75_SCC_REG_14:
			READ_SCC_REG( llHdl->ma, ch, M75_R14, retVal );
			*valueP = retVal;
			break;
		case M75_SCC_REG_15:
			READ_SCC_REG( llHdl->ma, ch, M75_R15, retVal );
			*valueP = retVal;
			break;
		/*--------------------------+
		|  get M75 global registers |
		+--------------------------*/
		case M75_FIFO_DATA:
			retVal = MREAD_D8( llHdl->ma, FIFO_REG_A+(ch<<1) );
			break;
		case M75_FIFO_STATUS:
			retVal = MREAD_D8( llHdl->ma, FIFO_STATREG_A+(ch<<1) );
			*valueP = retVal;
			break;
		case M75_GETBLOCK_TOUT:
			*valueP = llHdl->chan[ch].getBlockTout;
			break;
		case M75_SETBLOCK_TOUT:
			*valueP = llHdl->chan[ch].setBlockTout;
			break;
		case M75_MAX_RXFRAME_SIZE:
			*valueP = llHdl->chan[ch].rxQ.maxFrameSize;
			break;
		case M75_MAX_TXFRAME_SIZE:
			*valueP = llHdl->chan[ch].txQ.maxFrameSize;
			break;
		case M75_MAX_RXFRAME_NUM:
			*valueP = llHdl->chan[ch].rxQ.maxFrameNum;
			break;
		case M75_MAX_TXFRAME_NUM:
			*valueP = llHdl->chan[ch].txQ.maxFrameNum;
			break;
		case M75_SCC_REGS:
		{
			M75_SCC_REGS_PB *sccp = (M75_SCC_REGS_PB *)blk->data;
			if( blk->size != sizeof(M75_SCC_REGS_PB) ){
				DBGWRT_ERR((DBH, "*** ERR %s: wrong blk->size for M75_CUR_CONFIG\n", functionName));
				error = ERR_LL_ILL_PARAM;
				break;
			}

			sccp->wr01 = llHdl->chan[ch].sccRegs.wr01;
			sccp->wr02 = llHdl->chan[ch].sccRegs.wr02;
			sccp->wr03 = llHdl->chan[ch].sccRegs.wr03;
			sccp->wr04 = llHdl->chan[ch].sccRegs.wr04;
			sccp->wr05 = llHdl->chan[ch].sccRegs.wr05;
			sccp->wr06 = llHdl->chan[ch].sccRegs.wr06;
			sccp->wr07 = llHdl->chan[ch].sccRegs.wr07;
			sccp->wr7p = llHdl->chan[ch].sccRegs.wr7p;
			sccp->wr09 = llHdl->chan[ch].sccRegs.wr09;
			sccp->wr10 = llHdl->chan[ch].sccRegs.wr10;
			sccp->wr11 = llHdl->chan[ch].sccRegs.wr11;
			sccp->wr12 = llHdl->chan[ch].sccRegs.wr12;
			sccp->wr13 = llHdl->chan[ch].sccRegs.wr13;
			sccp->wr14 = llHdl->chan[ch].sccRegs.wr14;
			sccp->wr15 = llHdl->chan[ch].sccRegs.wr15;

			break;
		}
        /*--------------------------+
        |  (unknown)                |
        +--------------------------*/
        default:
            error = ERR_LL_UNK_CODE;
    }

	return(error);
}

/******************************* M75_BlockRead ******************************/
/** Read a data block from the device
 *
 *  \param llHdl       \IN  low-level handle
 *  \param ch          \IN  current channel
 *  \param buf         \IN  data buffer
 *  \param size        \IN  data buffer size
 *  \param nbrRdBytesP \OUT number of read bytes
 *
 *  \return            \c 0 on success or error code
 */
static int32 M75_BlockRead(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrRdBytesP
)
{
	MQUEUE_HEAD *rxQ = &llHdl->chan[ch].rxQ;
	u_int32 n;
	int32 error = 0;
	u_int8 *frm;
	OSS_IRQ_STATE irqState;
	u_int8 irqMasked=0;
    DBGCMD( static const char functionName[] = "LL - M75_BlockRead()"; )

    DBGWRT_1((DBH, "%s: ch=%d, size=%d\n", functionName, ch, size));

	/* return number of read bytes */
	*nbrRdBytesP = 0;

	/* parameter checks */
	if( ch >= CH_NUMBER  || ch < 0)
		return M75_ERR_CH_NUMBER;

	if( size < (int32)rxQ->first->size )
		return(M75_ERR_FRAMETOOLARGE);

	irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );
	irqMasked++;

	/* check if errors occured */
	if( llHdl->chan[ch].rxERR ) {
		int32 rxerror=llHdl->chan[ch].rxERR;
		llHdl->chan[ch].rxERR = 0;
		rxQ->errSent = TRUE;
		OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );
		return( rxerror );
	}

	/* check for read frames */
	if( !rxQ->totEntries || !rxQ->first->ready || !rxQ->qinit ){
		/* Rx buffer queue empty, or frame not completely passed yet */

		if( ( M75_SYNC_MODE != llHdl->chan[ch].syncMode ) && rxQ->last->xfering )
		{
			/* ASYNC mode:
			 * Rx chars available in curent buffer, pass them to user */
			rxQ->last->ready   = TRUE;
			rxQ->last->xfering = FALSE;
			rxQ->totEntries++;
			rxQ->last = rxQ->last->next;

			IDBGDMP_4((DBH, "LL - M75_BlockRead(), Rx Data:", llHdl->chan[ch].rxQ.last->frame, llHdl->chan[ch].rxQ.last->size, 1));
		} else {
			/* wait for data */
			rxQ->waiting = TRUE; /* flag, waiting for sem */

			DBGWRT_2((DBH, "%s: Rx buffer queue empty\n", functionName));

			if( llHdl->chan[ch].getBlockTout == 0 ) {
			/* return immediately */
				rxQ->waiting = FALSE; /* flag, waiting for sem */
				OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );
				return( M75_ERR_RX_QEMPTY );
			}

			OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );
			irqMasked--;

			while( error == 0 ) {
				DBGWRT_3((DBH, "%s: Rx waiting\n", functionName));

				DEVSEM_UNLOCK( llHdl );

				/* wait for data in queue */
				error = OSS_SemWait( llHdl->osHdl, rxQ->sem,
									 llHdl->chan[ch].getBlockTout );

				DEVSEM_LOCK( llHdl );

				if( rxQ->waiting == FALSE ){
					/*
					 * data available or error occured
					 */
					DBGWRT_3((DBH, "%s: Rx got sem\n", functionName));

					/* check if errors occured */
					if( llHdl->chan[ch].rxERR ) {
						int32 rxerror=llHdl->chan[ch].rxERR;
						llHdl->chan[ch].rxERR = 0;
						rxQ->errSent = TRUE;
						OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );
						return( rxerror );
					}
					irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );
					irqMasked++;

					if( ( M75_SYNC_MODE != llHdl->chan[ch].syncMode ) &&
						( !rxQ->totEntries || !rxQ->first->ready )    &&
						rxQ->last->xfering )
					{
						/* ASYNC mode, data available in unfinished queue entry only
						 * pass this to user */
						rxQ->last->ready   = TRUE;
						rxQ->last->xfering = FALSE;
						rxQ->totEntries++;
						rxQ->last = rxQ->last->next;

						IDBGDMP_4((DBH, "LL - M75_BlockRead(), Rx Data:", llHdl->chan[ch].rxQ.last->frame, llHdl->chan[ch].rxQ.last->size, 1));
					}

					break;
				}
				else if( error ){
					rxQ->waiting = FALSE;
					DBGWRT_ERR((DBH,"*** %s: error 0x%x waiting for data\n", functionName, error ));
					*nbrRdBytesP = 0;
					return( error );
				}
				/* no error, but no fifo space, continue waiting */

			}
		}
	}

	/*----------------------+
	|  Get frame from queue |
	+----------------------*/
	if( !irqMasked )
		irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );

	n = rxQ->first->size;
	frm = (u_int8 *)buf;

	OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );

	OSS_MemCopy(llHdl->osHdl, rxQ->first->size,
				(char*)rxQ->first->frame,
				(char*)buf );

	irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );

	rxQ->totEntries--;
	rxQ->errSent = FALSE;
	rxQ->first->ready++;

	/* return nr of read bytes */
	*nbrRdBytesP = rxQ->first->size;
	rxQ->first->size = 0;

	rxQ->first = rxQ->first->next;

	OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );

	DBGWRT_3((DBH, "%s: passing %d bytes\n", functionName, *nbrRdBytesP));

	return(ERR_SUCCESS);
} /* M75_BlockRead */

/****************************** M75_BlockWrite *****************************/
/** Write a data block to the device
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch          \IN  current channel
 *  \param buf         \IN  data buffer
 *  \param size        \IN  data buffer size
 *  \param nbrWrBytesP \OUT number of written bytes
 *
 *  \return            \c 0 on success or error code
 */
static int32 M75_BlockWrite(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrWrBytesP
)
{
	int32 error = ERR_SUCCESS;
	OSS_IRQ_STATE irqState;
    DBGCMD( static const char functionName[] = "LL - M75_BlockWrite()"; )
    DBGWRT_1((DBH, "%s: ch=%d, size=%d\n", functionName, ch, size));

	/* sanity check */
	if( ch >= CH_NUMBER  || ch < 0) {
		error = M75_ERR_CH_NUMBER;
		goto ERR_ABORT;
	}

	if( (u_int32)size > llHdl->chan[ch].txQ.maxFrameSize ) {
		error = M75_ERR_FRAMETOOLARGE;
		goto ERR_ABORT;
	}

	/* check for queue space */

	if ( (llHdl->chan[ch].txQ.totEntries == llHdl->chan[ch].txQ.maxFrameNum) ||
		  !llHdl->chan[ch].txQ.qinit){

		DBGWRT_2((DBH, "   %s: Tx buffer queue full\n", functionName));

		/* Tx buffer queue full */
		if( llHdl->chan[ch].setBlockTout == 0 ) {
		/* return immediately */
			*nbrWrBytesP = 0;
			error = M75_ERR_TX_QFULL;
			goto ERR_ABORT;
		}

		while( error == 0 ) {
			DBGWRT_3((DBH, "   %s: Tx waiting\n", functionName));

			llHdl->chan[ch].txQ.waiting = TRUE; /* flag, waiting for sem */

			DEVSEM_UNLOCK( llHdl );

			/* wait for FIFO space */
			error = OSS_SemWait( llHdl->osHdl, llHdl->chan[ch].txQ.sem,
								 llHdl->chan[ch].setBlockTout );

			DEVSEM_LOCK( llHdl );

			if( llHdl->chan[ch].txQ.waiting == FALSE ){
				/*
				 * fifo space available
				 */
				DBGWRT_3((DBH, "   %s: Tx got queue space\n", functionName));
				break;
			}
			else if( error ){
				llHdl->chan[ch].txQ.waiting = FALSE;
				DBGWRT_ERR((DBH,"*** %s: error 0x%x waiting for Tx queue space\n", functionName, error ));
				goto ERR_ABORT;
			}
			/* no error, but no fifo space, continue waiting */

		}
	}

	/* put frame into queue */
	OSS_MemCopy(llHdl->osHdl, size, (char*)buf,
				(char*)llHdl->chan[ch].txQ.last->frame );

	irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );

	llHdl->chan[ch].txQ.last->size = size;
	llHdl->chan[ch].txQ.last->xfering = 0;
	llHdl->chan[ch].txQ.last->ready = 1;
	llHdl->chan[ch].txQ.totEntries++;
	llHdl->chan[ch].txQ.last = llHdl->chan[ch].txQ.last->next;

	/* return number of written bytes */
	*nbrWrBytesP = size;

	OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );

	/* try to send data/frame */
	error = M75_Tx(llHdl,ch);

ERR_ABORT:
	return(error);
} /* M75_BlockWrite */

/****************************** M75_TxFrame **********************************/
/** Transfer a frame from the Tx queue to the Tx FIFO.
 *  Enable transmitter and FIFO.
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch          \IN  current channel
 *
 *  \return            \c 0 on success or error code
 */
static int32 M75_Tx(
	LL_HANDLE *llHdl,
	int32 ch
)
{
	int32 error = ERR_SUCCESS;
	IDBGWRT_1((DBH, ">>> LL - M75_Tx: ch=%d\n", ch));

	if( M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
		error = M75_TxFrame_Sync( llHdl,ch);
	} else {
		error = M75_TxData_Async( llHdl,ch);
	}

	IDBGWRT_1((DBH, "<<< LL - M75_Tx: ch=%d\n", ch));
	return(error);

} /* M75_Tx */

/****************************** M75_TxFrame_Sync *****************************/
/** Transfer a frame from the Tx queue to the Tx FIFO in synchronous mode.
 *  Enable transmitter and FIFO.
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch          \IN  current channel
 *
 *  \return            \c 0 on success or error code
 */
static int32 M75_TxFrame_Sync(
	LL_HANDLE *llHdl,
	int32 ch
)
{
	MQUEUE_HEAD *qHead = &llHdl->chan[ch].txQ;
	OSS_IRQ_STATE irqState;
	u_int8 retVal;
    DBGCMD( static const char functionName[] = "LL - M75_Tx (SYNC)"; )

	IDBGWRT_2((DBH, "   >>> %s: ch=%d; totEntries=%d\n", functionName, ch, qHead->totEntries));

	irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );

	if( !llHdl->chan[ch].txUnderrEOMgot ) {
		/* read status of external FIFO */
		retVal = MREAD_D8( llHdl->ma, FIFO_STATREG_A+(ch<<1) );
		if( retVal & M75_FIFO_STATREG_TXFIFO_EMPTY ) {	/* external Tx FIFO is empty */
			/* eventually allow CRC to be transmitted */
			OSS_MikroDelay(llHdl->osHdl, 5);
			llHdl->chan[ch].txUnderrEOMgot = 1;
		}
	}

	if( llHdl->chan[ch].txUnderrEOMgot &&	/* TxUnderr/EOM interrupt got */
		qHead->totEntries &&				/* frame in queue */
		qHead->first->ready &&				/* frame is ready to be sent */
		qHead->qinit ){

		/* set frame inactive so if interrupted by ISR, *
		 * this frame is not sent twice */
		qHead->first->ready = 0;
		llHdl->chan[ch].txUnderrEOMgot = 0;

		OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );

		IDBGWRT_2((DBH, "    %s: frame %4d; size=0x%04X\n",
						functionName, qHead->first->frame[0], qHead->first->size));

		/* enable Tx */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R05, llHdl->chan[ch].sccRegs.wr05 | M75_SCC_WR05_TX_EN );

		MFIFO_WRITE_D8( llHdl->ma, (FIFO_REG_A+(ch<<1)), qHead->first->size, qHead->first->frame );

		/* enable Tx FIFO */
		/* when this bit is set the data is transferred from the FIFO to the transmit buffer */
		MWRITE_D8( llHdl->ma, FIFO_STATREG_A+(ch<<1), M75_FIFO_STATREG_TXEN );

		/* send semaphore if BlockWrite is waiting for free buffer space */
		if( qHead->waiting ) {
			IDBGWRT_3((DBH, "    %s: wake write waiter\n", functionName));
			qHead->waiting = FALSE;
			OSS_SemSignal( llHdl->osHdl, qHead->sem );
		}

		/* delete frame from queue */
		irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );
		qHead->first = qHead->first->next;
		qHead->totEntries--;
		OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );
	} else {
		IDBGWRT_3((DBH, "    %s (not sent): txUnderrEOMgot=%d; totEntries=%d; ready=%d; qinit=%d\n",
						functionName, llHdl->chan[ch].txUnderrEOMgot, qHead->totEntries, qHead->first->ready, qHead->qinit));
	}
	OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );

	IDBGWRT_2((DBH, "   <<< %s\n", functionName));
	return(ERR_SUCCESS);
} /* M75_TxFrame_Sync */

/****************************** M75_TxData_Async *****************************/
/** Transfer data from the Tx queue to the Data Reg in async/IRQ mode.
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch          \IN  current channel
 *
 *  \return            \c 0 on success or error code
 */
static int32 M75_TxData_Async(
	LL_HANDLE *llHdl,
	int32 ch
)
{
	MQUEUE_HEAD *qHead = &llHdl->chan[ch].txQ;
	OSS_IRQ_STATE irqState;
    DBGCMD( static const char functionName[] = "LL - M75_Tx (ASYNC)"; )

	IDBGWRT_2((DBH, "   >>> %s: ch=%d; totEntries=%d\n", functionName, ch, qHead->totEntries));

	irqState = OSS_IrqMaskR( llHdl->osHdl, llHdl->irqHdl );

	if( llHdl->chan[ch].txBufEmpty &&	/* Tx buffer empty interrupt got */
		qHead->qinit &&					/* Tx Queue initialized */
		qHead->totEntries &&			/* frame in queue */
		qHead->first->xfering )			/* frame is beeing processed */
	{
		IDBGWRT_2((DBH, "   >>> %s: processing active frame (send byte %d)\n", functionName, qHead->first->xfering));

		/* remember that we are transfering */
		llHdl->chan[ch].txBufEmpty = 0;
		/* send next byte */
		MWRITE_D8( llHdl->ma, DATA_REG_A+(ch<<1), qHead->first->frame[qHead->first->xfering++] );

		if( qHead->first->xfering == qHead->first->size ) {
			/* frame sent completely
			 * mark queue entry as empty, move on to next frame if available */
			qHead->first->xfering = 0;
			qHead->first->ready   = 0;
			qHead->first = qHead->first->next;
			qHead->totEntries--;
			IDBGWRT_3((DBH, "   >>> %s: return finished queue entry to queue\n", functionName));

			/* send semaphore if BlockWrite is waiting for free buffer space */
			if( qHead->waiting ) {
				IDBGWRT_3((DBH, "   >>> %s: wake write waiter\n", functionName));
				qHead->waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, qHead->sem );
			}

			goto ERR_ABORT;
		}
	} else if( llHdl->chan[ch].txBufEmpty && /* TxUnderr/EOM interrupt got */
			   qHead->qinit &&				 /* Tx Queue initialized */
			   qHead->totEntries &&			 /* frame in queue */
			   qHead->first->ready )		 /* frame is ready for being sent */
	{
		IDBGWRT_3((DBH, "   >>> %s: processing new frame %4d; size=0x%04X\n",
					functionName, qHead->first->frame[0], qHead->first->size));
		/* set frame inactive so if interrupted by ISR, this frame is not sent twice */
		qHead->first->ready = 0;
		qHead->first->xfering = 1;

		llHdl->chan[ch].txBufEmpty = 0;

		/* send first byte */
		MWRITE_D8( llHdl->ma, DATA_REG_A+(ch<<1), qHead->first->frame[0] );

		/* enable Tx */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R05, (llHdl->chan[ch].sccRegs.wr05 | M75_SCC_WR05_TX_EN) );
	} else {
		IDBGWRT_3((DBH, "     %s (not sent!): txBufEmpty=%d; totEntries=%d; ready=%d; qinit=%d\n",
						functionName, llHdl->chan[ch].txBufEmpty, qHead->totEntries, qHead->first->ready, qHead->qinit));

		/* got for what reason ever, just acknowledge IRQ so CRC, ... is beeing sent */
		/* reset Tx interrupt pending, reset highest IUS */
		WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_TXINT );
		WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_HIGH_IUS );
	}

ERR_ABORT:
	OSS_IrqRestore( llHdl->osHdl, llHdl->irqHdl, irqState );
	IDBGWRT_2((DBH, "<<< %s\n", functionName));
	return(ERR_SUCCESS);
} /* M75_TxData_Async */


/****************************** M75_IrqExtStat *******************************/
/** Interrupt service routine for Ext/Status IRQs
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param chan  	   \IN  channel interrupt occurred for
 *  \return ERR_SUCCESS
 */
static int32 M75_IrqExtStat(
   LL_HANDLE *llHdl,
   u_int32 chan
) {
	u_int32 rr0=0;
    DBGCMD( static const char functionName[] = "LL - M75_IrqExtStat"; )

	READ_SCC_REG( llHdl->ma, chan, M75_R00, rr0 );
	IDBGWRT_2((DBH, "   >>> %s RR0 = 0x%02X\n", functionName, rr0));

	/* reset Ext/Status int, reset highest IUS */
	WRITE_SCC_REG( llHdl->ma, chan, M75_R00, M75_SCC_WR00_RST_EXT_STAT );
	WRITE_SCC_REG( llHdl->ma, chan, M75_R00, M75_SCC_WR00_RST_HIGH_IUS );

	if( (rr0 & M75_SCC_RR00_BREAK_ABORT) ) {
		/* Break/Abort */
		/* !!! both transitions of the Break/Abort signal may
		 *     cause an interrupt. Break/Abort IRQ may also be
		 *     indicated by sync/hunt bit
		 *     since break abort bit in rr0 is not latched
		 */
		IDBGWRT_ERR((DBH, "   >>> ERR %s: Break/Abort ch %s\n", functionName, chan ? "B" : "A"));
#ifdef M75_SUPPORT_BREAK_ABORT
		/* ignore this at moment */
		M75_BreakAbortHandler(llHdl, chan);
#endif
	} else if( rr0 & M75_SCC_RR00_TX_UNDR_EOM ) {
		/* Tx Underrun/EOM */
		IDBGWRT_2((DBH, "   >>> %s: TxUnderr/EOM IP ch %s\n", functionName, chan ? "B" : "A"));

		/* disable Tx FIFO */
		MWRITE_D8( llHdl->ma, FIFO_STATREG_A+(chan<<1), 0x00 );

		/* flag interrupt, send next frame if available */
		llHdl->chan[chan].txUnderrEOMgot = 1;
		M75_Tx(llHdl,chan);

	} else if( rr0 & M75_SCC_RR00_SYNC_HUNT ) {
		IDBGWRT_2((DBH, "   >>> %s: Sync-Hunt IP ch %s\n", functionName, chan ? "B" : "A"));
		/* may also be an indicator for a Break/Abort interrupt */
#ifdef M75_SUPPORT_BREAK_ABORT
		/* ignore this at moment */
		M75_BreakAbortHandler(llHdl, chan);
#endif
	} else if( rr0 & 0xFA ) {
		IDBGWRT_ERR((DBH, "   >>> ERR %s: unhandled Ext/Status Irq ch %s: RR0 = 0x%02X\n",
							functionName, chan ? "B" : "A", rr0));
	}
	IDBGWRT_2((DBH, "   <<< %s\n", functionName));
	return ERR_SUCCESS;
} /* M75_IrqExtStat */

/******************************** M75_Irq ************************************/
/** Interrupt service routine
 *
 *  If the driver can detect the interrupt's cause it returns
 *  LL_IRQ_DEVICE or LL_IRQ_DEV_NOT, otherwise LL_IRQ_UNKNOWN.
 *
 *  The interrupt is triggered when data is received (special condition only)
 *  errors are received or data is fully transmitted (incl. CRC).
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \return LL_IRQ_DEVICE	irq caused by device
 *          LL_IRQ_DEV_NOT  irq not caused by device
 *          LL_IRQ_UNKNOWN  unknown
 */
static int32 M75_Irq(
   LL_HANDLE *llHdl
)
{
	u_int32 iPend=0, rr2=0;
	u_int32 ch=0;
    IDBGWRT_1((DBH, ">>> M75_Irq:\n"));

	/* get Interrupt source */
	READ_SCC_REG( llHdl->ma, 0, M75_R03, iPend );
	IDBGWRT_2((DBH, ">>>     RR03 = 0x%02X (IP Reg)\n", iPend));

	if( !iPend ) {
   		IDBGWRT_1((DBH, "<<< M75_Irq: not me\n"));
		return( LL_IRQ_DEV_NOT ); /* say: it's not me */
	}

	while( iPend ) {
		/* get modified Interrupt Vector, Irq is resetted here */
		READ_SCC_REG( llHdl->ma, 1, M75_R02, rr2 );
		IDBGWRT_2((DBH, ">>>     RR02 = 0x%02X (IV Reg)\n", rr2));
		llHdl->irqCount++;

#if 0
		/* emergency break, to be removed later */
		if( llHdl->irqCount >= 5000 ) {
			IDBGWRT_ERR((DBH,
				"\n\n\n>>>ERR   M75_Irq: already processed 5000 Interrupts\n\n\n"));
			/* disable IRQs in HW */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R09, llHdl->chan[ch].sccRegs.wr09 & ~M75_SCC_WR09_MIE );
			break;
		}
#endif

		if( iPend & M75_SCC_RR03_CHA_RX_IP ){
			/* ch A Rx IP */
			IDBGWRT_2((DBH, ">>>     Rx IP ch A\n"));

			ch = 0;
			M75_IrqRx(llHdl, ch);

			/* send error reset */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_ERROR );
		} else if( iPend & M75_SCC_RR03_CHA_TX_IP ) {
			/* ch A Tx IP */
			ch = 0;
			if(  M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
				/* not used in SYNC mode, should never happen */
				IDBGWRT_2((DBH, ">>>     Tx IP ch A, currently not handled/used\n"));
			} else {
				/* handle in async mode */
				IDBGWRT_2((DBH, ">>>     Tx IP ch A\n"));
				/* flag interrupt, send next frame if available */
				llHdl->chan[ch].txBufEmpty = 1;
				M75_TxData_Async(llHdl, ch);
			}
			/* reset Tx interrupt pending, reset highest IUS */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_TXINT );
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_HIGH_IUS );
		} else if( iPend & M75_SCC_RR03_CHA_EXTSTAT_IP ) {
			/* ch A: Ext/Status IP */
			ch = 0;
			IDBGWRT_2((DBH, ">>>     Ext/Status IP ch A\n"));

			M75_IrqExtStat(llHdl, ch);


		} else if( iPend & M75_SCC_RR03_CHB_RX_IP ) {
			/* ch B Rx IP */
			IDBGWRT_2((DBH, ">>>     Rx IP ch B\n"));

			ch = 1;
			M75_IrqRx(llHdl, ch);

			/* send error reset */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_ERROR );
		} else if( iPend & M75_SCC_RR03_CHB_TX_IP ) {
			/* ch B Tx IP */
			ch = 1;
			if(  M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
				/* not used in SYNC mode, should never happen */
				IDBGWRT_2((DBH, ">>>     Tx IP ch B, currently not handled/used\n"));
			} else {
				/* handle in async mode */
				IDBGWRT_2((DBH, ">>>     Tx IP ch B\n"));
				/* flag interrupt, send next frame if available */
				llHdl->chan[ch].txBufEmpty = 1;
				M75_TxData_Async(llHdl, ch);
			}
			/* reset Tx interrupt pending, reset highest IUS */
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_TXINT );
			WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_HIGH_IUS );
		}else if( iPend & M75_SCC_RR03_CHB_EXTSTAT_IP ) {
			/* ch B Ext/Status IP */
			ch = 1;
			IDBGWRT_2((DBH, ">>>     Ext/Status IP ch B\n"));
			M75_IrqExtStat(llHdl, ch);
		}

		READ_SCC_REG( llHdl->ma, 0, M75_R03, iPend );
		IDBGWRT_3((DBH, ">>>     RR03 = 0x%02X (IP Reg)\n", iPend));
		/* another interrupt available? */
	}

   		IDBGWRT_1((DBH, "<<< M75_Irq\n"));
	return(LL_IRQ_DEVICE);		/* say: it's me */
}

/* currently break/Abort commands are ignored, since no clean way to handle */
#ifdef M75_SUPPORT_BREAK_ABORT
/************************** M75_BreakAbortHandler **************************/
/** Handler for Break/Abort Interrupts
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch    	   \IN  channel that caused the interrupt
 *  \return ERR_SUCCESS
 */
static int32 M75_BreakAbortHandler(LL_HANDLE *llHdl, u_int32 ch){

	if( llHdl->chan[ch].sccRegs.wr03 & M75_SCC_WR03_RX_EN ) {
		/* Abort Handling only interesting when Rx enabled */
		/* only process when receiving Break/Abort, not when exiting
		 * Break/Abort state (both transitions are causing an interrupt)
		 * can be seen by checking Sync/Hunt Bit */

/* !!!  possible source of corruption, if data is received and the status FIFO
 *		updated in between the two reset operations !!!
 *		normally the receiver should be disabled, the resets performed and
 *		then the receiver enabled again. But here, the older M75 modules (chip
 *		based) produce an endless loop by generating a break/abort interrupt
 *		every timethe receiver is enabled again while the opposite transmitter
 *		is disabled (continuous 1s)
 *		also, when the drivers ISR is delayed (busy system) and a receive int.
 *		(higher priority) occurs in the mean time, this will be serviced first(?)
 *		and corrupted data may be read. (of course if it is avoided by hardware
		that an ius is preempted by an higher prioritized interrupt,
		this problem never occurs */

		/* reset external Rx FIFO and status FIFO */
		MWRITE_D8( llHdl->ma, FIFO_RESET_REG, 0x04<<(ch*4) );

		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
					   llHdl->chan[ch].sccRegs.wr15 & (M75_SCC_WR15_WR7P_EN | M75_SCC_WR15_SDLC_FIFO_EN | M75_SCC_WR15_DCD_IE) );
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
					   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_SDLC_FIFO_EN );

		llHdl->chan[ch].rxERR = M75_ERR_RX_BREAKABORT;
		/* Send signal to application if enabled */
		if( llHdl->chan[ch].sig && !llHdl->chan[ch].rxQ.errSent ) {
			IDBGWRT_3((DBH, ">>>    send Rx ch %d signal to application\n", ch));
			OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
			llHdl->chan[ch].rxQ.errSent++;
		}

		/* send semaphore if BlockRead is waiting for receive data */
		if( llHdl->chan[ch].rxQ.waiting ) {
			IDBGWRT_3((DBH, ">>>    wake read waiter ch %d\n", ch));
			llHdl->chan[ch].rxQ.waiting = FALSE;
			OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
		}
	}

	return( ERR_SUCCESS );
}
#endif /* M75_SUPPORT_BREAK_ABORT */


/****************************** M75_IrqRx ************************************/
/** Handler for Special Receive Condition Interrupts
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch    	   \IN  channel that caused the interrupt
 *  \return M75_ERR_RX_QFULL	   Rx queue full or not initialized \n
 *          M75_ERR_RX_OVERFLOW    Overrun of the Status FIFO detected \n
 *          M75_ERR_RX_ERROR       Framing/CRC/Parity/Overrun Error \n
 *          M75_ERR_FRAMETOOLARGE  Frame is larger than rxQ.maxFrameSize \n
 *          ERR_SUCCESS            Valid frame received, put into Rx queue \n
 *
 * to be called only in M75_Irq, otherwise IrqMaskR/IrqRestore have to be
 * put in several places
 */
static int32 M75_IrqRx(LL_HANDLE *llHdl, u_int32 ch)
{
	int32 error = ERR_SUCCESS;
	IDBGWRT_1((DBH, "LL - M75_IrqRx: ch=%d\n", ch));

	if( M75_SYNC_MODE == llHdl->chan[ch].syncMode ) {
		error = M75_IrqRx_Frame_Sync( llHdl,ch);
	} else {
		error = M75_IrqRx_Data_Async( llHdl,ch);
	}
	return(error);
}

/****************************** M75_IrqRx_Frame_Sync *************************/
/** Handler for Special Receive Condition Interrupts in SYNC modes
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch    	   \IN  channel that caused the interrupt
 *  \return M75_ERR_RX_QFULL	   Rx queue full or not initialized \n
 *          M75_ERR_RX_OVERFLOW    Overrun of the Status FIFO detected \n
 *          M75_ERR_RX_ERROR       Framing/CRC/Parity/Overrun Error \n
 *          M75_ERR_FRAMETOOLARGE  Frame is larger than rxQ.maxFrameSize \n
 *          ERR_SUCCESS            Valid frame received, put into Rx queue \n
 *
 * to be called only in M75_Irq, otherwise IrqMaskR/IrqRestore have to be
 * put in several places
 */
static int32 M75_IrqRx_Frame_Sync(LL_HANDLE *llHdl, u_int32 ch){

	MQUEUE_ENT *rxEnt;
	u_int8 *frame;
	u_int8 retVal, rr7, rr6, rr1 = 0, rr0=0;
	u_int32 rxSize = 0, breakAbortIEset=0, i;
	u_int8 statusFIFOempty=0;
    DBGCMD( static const char functionName[] = "LL - M75_IrqRx (SYNC)"; )

	rxEnt = llHdl->chan[ch].rxQ.last;
	frame = rxEnt->frame;

	READ_SCC_REG( llHdl->ma, ch, M75_R00, rr0 );
	IDBGWRT_2((DBH, "   >>> %s:RR00 = 0x%02X\n", functionName, rr0));

	/* clear interrupt, reset highest IUS */
	WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_HIGH_IUS );

	/* disable Break/Abort Int
	 * to avoid an out of frame Break/Abort interrupt */
	if( (breakAbortIEset = (llHdl->chan[ch].sccRegs.wr15 & M75_SCC_WR15_BREAK_ABORT_IE)) )
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
					   llHdl->chan[ch].sccRegs.wr15 & ~(M75_SCC_WR15_BREAK_ABORT_IE) );


	while( !statusFIFOempty ) { /* pick up all frames already received */
		if( (llHdl->chan[ch].rxQ.totEntries ==
			   llHdl->chan[ch].rxQ.maxFrameNum) ||
			  !llHdl->chan[ch].rxQ.qinit ){
			IDBGWRT_ERR((DBH, "   >>> ERR %s: Rx Error: M75_ERR_RX_QFULL\n", functionName));

			/* count Status FIFO entries */
			llHdl->chan[ch].rxStatCnt++;

			if( llHdl->chan[ch].rxStatCnt > 10 ) {
				/* Rx Status FIFO overflowed */

				llHdl->chan[ch].rxERR = M75_ERR_RX_OVERFLOW;

				/* disable receiver */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R03,
								llHdl->chan[ch].sccRegs.wr03 & ~(M75_SCC_WR03_RX_EN) );
				IDBGWRT_ERR((DBH, "   >>> ERR %s: Rx Error: Status FIFO overflow,"
									"receiver disabled\n", functionName));

			} else if( !llHdl->chan[ch].rxERR ) {
				llHdl->chan[ch].rxERR = M75_ERR_RX_QFULL;
			}

			/* send semaphore if BlockRead is waiting for receive data */
			if( llHdl->chan[ch].rxQ.waiting ) {
				IDBGWRT_3((DBH, "   >>> %s: wake read waiter\n", functionName));
				llHdl->chan[ch].rxQ.waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
			}
			/* Send Signal to application if enabled */
			if( llHdl->chan[ch].sig && !llHdl->chan[ch].rxQ.errSent ) {
				IDBGWRT_3((DBH, "   >>> %s: send Rx ch A signal to application\n", functionName));
				OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
				llHdl->chan[ch].rxQ.errSent++;
			}

			/* enable Break/Abort interrupts again */
			if( breakAbortIEset ){
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
							   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
			}

			return(llHdl->chan[ch].rxERR);
		}

		/* get Rx status */
		READ_SCC_REG( llHdl->ma, ch, M75_R07, rr7 );
		READ_SCC_REG( llHdl->ma, ch, M75_R06, rr6 );

		/* get source (special condition) */
		READ_SCC_REG( llHdl->ma, ch, M75_R01, rr1 );
		IDBGWRT_3((DBH, "   >>> %s: RR01 = 0x%02X\n", functionName, rr1));

		if( llHdl->chan[ch].rxStatCnt )
			llHdl->chan[ch].rxStatCnt--;

		if( !(rr7 & M75_SCC_RR07_FDA) ) {
			/* Data from Status FIFO */
			statusFIFOempty++;
		} else if((rr1 & 0x70) || (rr7 & M75_SCC_RR07_FOS)) {
			/* Rx CRC/Framing, Parity, Overrun Error */
			IDBGWRT_ERR((DBH, "   >>> ERR %s: Rx Error: RR1 = 0x%02X, RR7 = 0x%02X\n", functionName, rr1, rr7));
			if( !llHdl->chan[ch].rxERR )
				llHdl->chan[ch].rxERR = M75_ERR_RX_ERROR;

			/* read to dummy to keep FIFO aligned */
			rxSize = ((rr7 & M75_SCC_RR07_BC_MASK) << 8) + rr6;
			for(i=0; i < rxSize; i++)
				retVal = MREAD_D8( llHdl->ma, FIFO_REG_A+(ch<<1) );

			/* send semaphore if BlockRead is waiting for receive data */
			if( llHdl->chan[ch].rxQ.waiting ) {
				IDBGWRT_3((DBH, "   >>> %s: wake read waiter\n", functionName));
				llHdl->chan[ch].rxQ.waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
			}
			/* Send Signal to application if enabled */
			if( llHdl->chan[ch].sig && !llHdl->chan[ch].rxQ.errSent ) {
				IDBGWRT_3((DBH, "   >>> %s: send Rx ch A signal to application\n", functionName));
				OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
				llHdl->chan[ch].rxQ.errSent++;
			}

			/* enable Break/Abort interrupts again */
			if( breakAbortIEset ){
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
							   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
			}

			IDBGWRT_2((DBH, "   <<< %s: Rx CRC/Framing/Overrun ERROR\n", functionName));
			return(M75_ERR_RX_ERROR);

		} else if( (rr1 & M75_SCC_RR01_END_FRAME) ) {
			/* detected EOF, get frame */

			IDBGWRT_3((DBH, "   >>> %s: end of frame detected: ", functionName));
			rxSize = ((rr7 & M75_SCC_RR07_BC_MASK) << 8) + rr6;

			if( rxSize > llHdl->chan[ch].rxQ.maxFrameSize ) {
				IDBGWRT_3((DBH, "Error\n"));
				IDBGWRT_ERR((DBH, "   >>> ERR %s:  Rx Error: M75_ERR_FRAMETOOLARGE\n", functionName));

				if( !llHdl->chan[ch].rxERR )
					llHdl->chan[ch].rxERR = M75_ERR_FRAMETOOLARGE;

				/* read to dummy to keep FIFO aligned */
				for(i=0; i < rxSize; i++)
					retVal = MREAD_D8( llHdl->ma, FIFO_REG_A+(ch<<1) );

				/* Send Signal to application if enabled */
				if( llHdl->chan[ch].sig ) {
					IDBGWRT_3((DBH, "   >>> %s: send Rx ch A signal to application\n", functionName));
					OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
				}

				/* send semaphore if BlockRead is waiting for receive data */
				if( llHdl->chan[ch].rxQ.waiting ) {
					IDBGWRT_3((DBH, "   >>> %s: wake read waiter\n", functionName));
					llHdl->chan[ch].rxQ.waiting = FALSE;
					OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
				}

				/* enable Break/Abort interrupts again */
				if( breakAbortIEset ){
					WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
								   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
				}

				IDBGWRT_2((DBH, "   <<< %s: ERROR: FRAME too large\n", functionName));
				return( M75_ERR_FRAMETOOLARGE );
			}

			/* put frame into buffer */
			MFIFO_READ_D8(  llHdl->ma, (FIFO_REG_A+(ch<<1)),
							((rxSize > llHdl->chan[ch].rxQ.maxFrameSize) ?
							 llHdl->chan[ch].rxQ.maxFrameSize : rxSize),
							frame );


			IDBGWRT_3((DBH, " received 0x%04X bytes\n", rxSize));

			/* update buffer */
			rxEnt->size = rxSize;
			rxEnt->ready++;
			llHdl->chan[ch].rxQ.totEntries++;
			llHdl->chan[ch].rxQ.last = llHdl->chan[ch].rxQ.last->next;

			IDBGDMP_4((DBH, "Rx Data:", frame, rxEnt->size, 1));

			/* send semaphore if BlockRead is waiting for receive data */
			if( llHdl->chan[ch].rxQ.waiting ) {
				IDBGWRT_3((DBH, "    wake read waiter\n"));
				llHdl->chan[ch].rxQ.waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
			}

			/* Send Signal to application if enabled */
			if( llHdl->chan[ch].sig ) {
				IDBGWRT_3((DBH, "   >>> %s: send Rx ch %s signal to application\n", functionName, (ch==0)?"A":"B"));
				OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
			}

		}
	}
	/* enable Break/Abort interrupts again */
	if( breakAbortIEset ){
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
					   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
	}

	IDBGWRT_2((DBH, "   <<< %s\n", functionName));
	return(ERR_SUCCESS);
} /* M75_IrqRx_Frame_Sync */

/************************* M75_IrqRx_Data_Async ******************************/
/** Handler for Special Receive Condition Interrupts in ASYNC modes
 *
 *  \param llHdl  	   \IN  low-level handle
 *  \param ch    	   \IN  channel that caused the interrupt
 *  \return M75_ERR_RX_QFULL	   Rx queue full or not initialized \n
 *          M75_ERR_RX_OVERFLOW    Overrun of the Status FIFO detected \n
 *          M75_ERR_RX_ERROR       Framing/CRC/Parity/Overrun Error \n
 *          M75_ERR_FRAMETOOLARGE  Frame is larger than rxQ.maxFrameSize \n
 *          ERR_SUCCESS            Valid frame received, put into Rx queue \n
 *
 * to be called only in M75_Irq, otherwise IrqMaskR/IrqRestore have to be
 * put in several places
 */
static int32 M75_IrqRx_Data_Async(LL_HANDLE *llHdl, u_int32 ch){

	MQUEUE_ENT *rxEnt = llHdl->chan[ch].rxQ.last;
	u_int8 retVal, rr1 = 0, rr0=0;
	u_int32 breakAbortIEset=0;
	u_int8 rxEmpty;
    DBGCMD( static const char functionName[] = "LL - M75_IrqRx (ASYNC)"; )

	READ_SCC_REG( llHdl->ma, ch, M75_R00, rr0 );
	rxEmpty = !(rr0 & M75_SCC_RR00_RX_CHAR_AVAIL );

	IDBGWRT_2((DBH, "   >>> %s: RR00 = 0x%02X\n", functionName, rr0));

	/* clear interrupt, reset highest IUS */
	WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_EXT_STAT );
	WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_RST_HIGH_IUS );

	/* disable Break/Abort Int
	 * to avoid an out of frame Break/Abort interrupt */
	if( (breakAbortIEset = (llHdl->chan[ch].sccRegs.wr15 & M75_SCC_WR15_BREAK_ABORT_IE)) )
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
					   llHdl->chan[ch].sccRegs.wr15 & ~M75_SCC_WR15_BREAK_ABORT_IE );


	while( !rxEmpty ) { /* pick up all bytes already received */
		if( (llHdl->chan[ch].rxQ.totEntries ==
			   llHdl->chan[ch].rxQ.maxFrameNum) ||
			  !llHdl->chan[ch].rxQ.qinit )
		{
			IDBGWRT_ERR((DBH, "   >>> ERR %s: Rx Error: M75_ERR_RX_QFULL\n", functionName));

			/* count Rx FIFO entries */
			llHdl->chan[ch].rxStatCnt++;

			if( llHdl->chan[ch].rxStatCnt > 10 ) {
				/* Rx Status FIFO overflowed */

				llHdl->chan[ch].rxERR = M75_ERR_RX_OVERFLOW;

				/* disable receiver */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R03,
								llHdl->chan[ch].sccRegs.wr03 & ~(M75_SCC_WR03_RX_EN) );
				IDBGWRT_ERR((DBH, "   >>> ERR %s: Rx Error: Status FIFO overflow, receiver disabled\n", functionName));

			} else if( !llHdl->chan[ch].rxERR ) {
				llHdl->chan[ch].rxERR = M75_ERR_RX_QFULL;
				/* tell me when next Char is received */
				WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_EN_INT_NXT_RX );
			}

			/* send semaphore if BlockRead is waiting for receive data */
			if( llHdl->chan[ch].rxQ.waiting ) {
				IDBGWRT_3((DBH, "   >>> %s: wake read waiter\n"));
				llHdl->chan[ch].rxQ.waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
			}
			/* Send Signal to application if enabled */
			if( llHdl->chan[ch].sig && !llHdl->chan[ch].rxQ.errSent ) {
				IDBGWRT_3((DBH, "   >>> %s: send Rx ch A signal to application\n", functionName));
				OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
				llHdl->chan[ch].rxQ.errSent++;
			}

			/* enable Break/Abort interrupts again */
			if( breakAbortIEset ){
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
							   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
			}

			IDBGWRT_2((DBH, "   <<< %s: Rx Queue Full\n", functionName));
			return(llHdl->chan[ch].rxERR);
		}

		/* get special condition source */
		READ_SCC_REG( llHdl->ma, ch, M75_R01, rr1 );
		IDBGWRT_3((DBH, "   >>> %s: RR01 = 0x%02X\n", functionName, rr1));

		if( llHdl->chan[ch].rxStatCnt )
			llHdl->chan[ch].rxStatCnt--;

		if(rr1 & (M75_SCC_RR01_ERR_RX_OVR | M75_SCC_RR01_ERR_CRC_FRM)) {
			/* Rx Framing | Parity Error */
			IDBGWRT_ERR((DBH, "   >>> ERR %s: Rx Error: RR1 = 0x%02X\n", functionName, rr1));

			if( !llHdl->chan[ch].rxERR ) /* don't overwrite Rx errors */
				llHdl->chan[ch].rxERR = M75_ERR_RX_ERROR;

			/* read to dummy to keep error and data FIFO aligned */
			retVal = MREAD_D8( llHdl->ma, FIFO_REG_A+(ch<<1) );

			/* send semaphore if BlockRead is waiting for receive data */
			if( llHdl->chan[ch].rxQ.waiting ) {
				IDBGWRT_3((DBH, "   >>> %s: wake read waiter\n", functionName));
				llHdl->chan[ch].rxQ.waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
			}
			/* Send Signal to application if enabled */
			if( llHdl->chan[ch].sig && !llHdl->chan[ch].rxQ.errSent ) {
				IDBGWRT_3((DBH, "   >>> %s: send Rx ch A signal to application\n", functionName));
				OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
				llHdl->chan[ch].rxQ.errSent++;
			}

			/* enable Break/Abort interrupts again */
			if( breakAbortIEset ){
				WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
							   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
			}

			IDBGWRT_2((DBH, "   <<< %s: Rx Framing/Parity Error\n", functionName));
			return(M75_ERR_RX_ERROR);

		} else  {
			/* data available */

			/* put byte into buffer */
			READ_SCC_REG( llHdl->ma, ch, M75_R08, retVal );
			IDBGWRT_5((DBH, "   >>> %s received byte %02x (buffer position %d)\n", functionName, (u_int8)retVal, rxEnt->size));

			rxEnt->frame[rxEnt->size++] = retVal;
			rxEnt->xfering = TRUE; /* mark queue entry as beeing filled */

			/* update buffer */
			if( rxEnt->size == llHdl->chan[ch].rxQ.maxFrameSize ) {
				/* buffer full, finish it up */
				rxEnt->ready   = TRUE;
				rxEnt->xfering = FALSE;
				llHdl->chan[ch].rxQ.totEntries++;
				llHdl->chan[ch].rxQ.last = llHdl->chan[ch].rxQ.last->next;
				IDBGDMP_4((DBH, "Rx Data:", rxEnt->frame, rxEnt->size, 1));
			}

			/* send semaphore if BlockRead is waiting for receive data */
			if( llHdl->chan[ch].rxQ.waiting ) {
				IDBGWRT_3((DBH, "   >>> %s: wake read waiter\n", functionName));
				llHdl->chan[ch].rxQ.waiting = FALSE;
				OSS_SemSignal( llHdl->osHdl, llHdl->chan[ch].rxQ.sem );
			}

			/* Send Signal to application if enabled */
			if( llHdl->chan[ch].sig && rxEnt->size == llHdl->chan[ch].asyRxSigWaterM ) {
				IDBGWRT_3((DBH, "   >>> %s: send Rx ch %s signal to application\n", functionName, (ch==0)?"A":"B"));
				OSS_SigSend( llHdl->osHdl, llHdl->chan[ch].sig );
			}

		}
		READ_SCC_REG( llHdl->ma, ch, M75_R00, rr0 );
		rxEmpty = !( rr0 & M75_SCC_RR00_RX_CHAR_AVAIL );
		IDBGWRT_5((DBH, "   >>> %s: %smore data avail\n", functionName, rxEmpty ? "no " : ""));
	}
	/* enable Break/Abort interrupts again */
	if( breakAbortIEset ){
		WRITE_SCC_REG( llHdl->ma, ch, M75_R15,
					   llHdl->chan[ch].sccRegs.wr15 | M75_SCC_WR15_BREAK_ABORT_IE);
	}

	/* produce an Int on next Rx Char */
	WRITE_SCC_REG( llHdl->ma, ch, M75_R00, M75_SCC_WR00_EN_INT_NXT_RX );

	IDBGWRT_2((DBH, "   <<< %s\n", functionName));
	return(ERR_SUCCESS);
} /* M75_Rx_Frame_Async */

/****************************** M75_Info *************************************/
/** Get information about hardware and driver requirements
 *
 *  The following info codes are supported:
 *
 * \code
 *  Code                      Description
 *  ------------------------  -----------------------------
 *  LL_INFO_HW_CHARACTER      hardware characteristics
 *  LL_INFO_ADDRSPACE_COUNT   nr of required address spaces
 *  LL_INFO_ADDRSPACE         address space information
 *  LL_INFO_IRQ               interrupt required
 *  LL_INFO_LOCKMODE          process lock mode required
 * \endcode
 *
 *  The LL_INFO_HW_CHARACTER code returns all address and
 *  data modes (ORed) which are supported by the hardware
 *  (MDIS_MAxx, MDIS_MDxx).
 *
 *  The LL_INFO_ADDRSPACE_COUNT code returns the number
 *  of address spaces used by the driver.
 *
 *  The LL_INFO_ADDRSPACE code returns information about one
 *  specific address space (MDIS_MAxx, MDIS_MDxx). The returned
 *  data mode represents the widest hardware access used by
 *  the driver.
 *
 *  The LL_INFO_IRQ code returns whether the driver supports an
 *  interrupt routine (TRUE or FALSE).
 *
 *  The LL_INFO_LOCKMODE code returns which process locking
 *  mode the driver needs (LL_LOCK_xxx).
 *
 *  \param infoType	   \IN  info code
 *  \param ...         \IN  argument(s)
 *
 *  \return            \c 0 on success or error code
 */
static int32 M75_Info(
   int32  infoType,
   ...
)
{
    int32   error = ERR_SUCCESS;
    va_list argptr;

    va_start(argptr, infoType );

    switch(infoType) {
		/*-------------------------------+
        |  hardware characteristics      |
        |  (all addr/data modes ORed)   |
        +-------------------------------*/
        case LL_INFO_HW_CHARACTER:
		{
			u_int32 *addrModeP = va_arg(argptr, u_int32*);
			u_int32 *dataModeP = va_arg(argptr, u_int32*);

			*addrModeP = MDIS_MA08;
			*dataModeP = MDIS_MD08 | MDIS_MD16;
			break;
	    }
		/*-------------------------------+
        |  nr of required address spaces |
        |  (total spaces used)           |
        +-------------------------------*/
        case LL_INFO_ADDRSPACE_COUNT:
		{
			u_int32 *nbrOfAddrSpaceP = va_arg(argptr, u_int32*);

			*nbrOfAddrSpaceP = ADDRSPACE_COUNT;
			break;
	    }
		/*-------------------------------+
        |  address space type            |
        |  (widest used data mode)       |
        +-------------------------------*/
        case LL_INFO_ADDRSPACE:
		{
			u_int32 addrSpaceIndex = va_arg(argptr, u_int32);
			u_int32 *addrModeP = va_arg(argptr, u_int32*);
			u_int32 *dataModeP = va_arg(argptr, u_int32*);
			u_int32 *addrSizeP = va_arg(argptr, u_int32*);

			if (addrSpaceIndex >= ADDRSPACE_COUNT)
				error = ERR_LL_ILL_PARAM;
			else {
				*addrModeP = MDIS_MA08;
				*dataModeP = MDIS_MD16;
				*addrSizeP = ADDRSPACE_SIZE;
			}

			break;
	    }
		/*-------------------------------+
        |   interrupt required           |
        +-------------------------------*/
        case LL_INFO_IRQ:
		{
			u_int32 *useIrqP = va_arg(argptr, u_int32*);

			*useIrqP = USE_IRQ;
			break;
	    }
		/*-------------------------------+
        |   process lock mode            |
        +-------------------------------*/
        case LL_INFO_LOCKMODE:
		{
			u_int32 *lockModeP = va_arg(argptr, u_int32*);

			*lockModeP = LL_LOCK_NONE;
			break;
			/* driver locks critical sections */
	    }
		/*-------------------------------+
        |   (unknown)                    |
        +-------------------------------*/
        default:
          error = ERR_LL_ILL_PARAM;
    }

    va_end(argptr);
    return(error);
}

/*******************************  Ident  ***********************************/
/** Return ident string
 *
 *  \return            pointer to ident string
 */
static char* Ident( void )
{
    return( "M75 - M75 low level driver: $Id: m75_drv.c,v 1.5 2009/07/14 17:47:14 cs Exp $" );
}

/********************************* Cleanup *********************************/
/** Close all handles, free memory and return error code
 *
 *	\warning The low-level handle is invalid after this function is called.
 *
 *  \param llHdl      \IN  low-level handle
 *  \param retCode    \IN  return value
 *
 *  \return           \IN   retCode
 */
static int32 Cleanup(
   LL_HANDLE    *llHdl,
   int32        retCode
)
{
	u_int32 ch;
    /*------------------------------+
    |  close handles                |
    +------------------------------*/
	/* clean up desc */
	if (llHdl->descHdl)
		DESC_Exit(&llHdl->descHdl);

	/* clean up debug */
	DBGEXIT((&DBH));

    /*---------------------------------------------+
    |  free memory, remove semaphores and signals  |
    +---------------------------------------------*/
	for(ch = 0; ch<CH_NUMBER; ch++){
		/* remove semaphores */
		if( llHdl->chan[ch].rxQ.sem )
			OSS_SemRemove( llHdl->osHdl, &llHdl->chan[ch].rxQ.sem );
		if( llHdl->chan[ch].txQ.sem )
			OSS_SemRemove( llHdl->osHdl, &llHdl->chan[ch].txQ.sem );

		if( llHdl->chan[ch].sig )
			OSS_SigRemove( llHdl->osHdl,
									&llHdl->chan[ch].sig );

		/* free RxQ */
		if( llHdl->chan[ch].rxQ.startAlloc ){
			OSS_MemFree(llHdl->osHdl,
						llHdl->chan[ch].rxQ.startAlloc,
						llHdl->chan[ch].rxQ.memAlloc);
		}

		/* free TxQ */
		if( llHdl->chan[ch].txQ.startAlloc ){
			OSS_MemFree(llHdl->osHdl,
						llHdl->chan[ch].txQ.startAlloc,
						llHdl->chan[ch].txQ.memAlloc);
		}

	}
    /* free my handle */
    OSS_MemFree(llHdl->osHdl, (int8*)llHdl, llHdl->memAlloc);

    /*------------------------------+
    |  return error code            |
    +------------------------------*/
	return(retCode);
}





