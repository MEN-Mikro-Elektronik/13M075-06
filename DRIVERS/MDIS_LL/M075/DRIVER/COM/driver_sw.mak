#***************************  M a k e f i l e  *******************************
#
#         Author: Christian.Schuster@men.de
#          $Date: 2005/02/07 16:12:46 $
#      $Revision: 1.2 $
#
#    Description: makefile descriptor file for common
#                 modules MDIS 4.0   e.g. low level driver
#
#---------------------------------[ History ]---------------------------------
#
#   $Log: driver_sw.mak,v $
#   Revision 1.2  2005/02/07 16:12:46  cs
#   Added switch M75_SUPPORT_BREAK_ABORT
#
#   Revision 1.1  2004/08/06 11:20:25  cs
#   Initial Revision
#
#
#-----------------------------------------------------------------------------
#   (c) Copyright 2004 by MEN mikro elektronik GmbH, Nuernberg, Germany
#*****************************************************************************

MAK_NAME=m75_sw

MAK_SWITCH=$(SW_PREFIX)MAC_MEM_MAPPED	\
		   $(SW_PREFIX)MAC_BYTESWAP		\
		   $(SW_PREFIX)M75_SW			\
		   $(SW_PREFIX)ID_SW			\
		   $(SW_PREFIX)M75_SUPPORT_BREAK_ABORT \

MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/desc$(LIB_SUFFIX)		\
         $(LIB_PREFIX)$(MEN_LIB_DIR)/id_sw$(LIB_SUFFIX)		\
         $(LIB_PREFIX)$(MEN_LIB_DIR)/oss$(LIB_SUFFIX)		\
         $(LIB_PREFIX)$(MEN_LIB_DIR)/dbg$(LIB_SUFFIX)		\


MAK_INCL=$(MEN_INC_DIR)/m75_drv.h	\
         $(MEN_INC_DIR)/men_typs.h	\
         $(MEN_INC_DIR)/oss.h		\
         $(MEN_INC_DIR)/mdis_err.h	\
         $(MEN_INC_DIR)/maccess.h	\
         $(MEN_INC_DIR)/desc.h		\
         $(MEN_INC_DIR)/mdis_api.h	\
         $(MEN_INC_DIR)/mdis_com.h	\
         $(MEN_INC_DIR)/modcom.h	\
         $(MEN_INC_DIR)/ll_defs.h	\
         $(MEN_INC_DIR)/ll_entry.h	\
         $(MEN_INC_DIR)/dbg.h		\
         $(MEN_MOD_DIR)/m75_int.h	\


MAK_INP1=m75_drv$(INP_SUFFIX)
MAK_INP2=

MAK_INP=$(MAK_INP1) \
        $(MAK_INP2)


