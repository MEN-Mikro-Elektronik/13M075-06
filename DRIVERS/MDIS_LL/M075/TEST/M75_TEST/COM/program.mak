#***************************  M a k e f i l e  *******************************
#
#         Author: Christian.Schuster@men.de
#          $Date: 2004/12/29 18:34:39 $
#      $Revision: 1.2 $
#
#    Description: makefile descriptor file for M75_TEST
#
#---------------------------------[ History ]---------------------------------
#
#   $Log: program.mak,v $
#   Revision 1.2  2004/12/29 18:34:39  cs
#   cosmetics
#
#   Revision 1.1  2004/08/06 11:20:33  cs
#   Initial Revision
#
#
#-----------------------------------------------------------------------------
#   (c) Copyright 2004 by MEN mikro elektronik GmbH, Nuernberg, Germany
#*****************************************************************************

MAK_NAME=m75_test

MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/mdis_api$(LIB_SUFFIX)    \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/usr_oss$(LIB_SUFFIX)     \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/usr_utl$(LIB_SUFFIX)     \


MAK_INCL=$(MEN_INC_DIR)/m75_drv.h     \
         $(MEN_INC_DIR)/men_typs.h    \
         $(MEN_INC_DIR)/mdis_api.h    \
         $(MEN_INC_DIR)/mdis_err.h    \
         $(MEN_INC_DIR)/usr_oss.h     \
		 $(MEN_INC_DIR)/usr_utl.h     \

MAK_INP1=m75_test$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)
