/*
 * Header for MultiMediaCard (MMC)
 *
 * Copyright 2002 Hewlett-Packard Company
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Many thanks to Alessandro Rubini and Jonathan Corbet!
 *
 * Based strongly on code by:
 *
 * Author: Yong-iL Joh <tolkien@mizi.com>
 *
 * Author:  Andrew Christian
 *          15 May 2002
 */

// clang-format off
#ifndef LINUX_MMC_MMC_H
#define LINUX_MMC_MMC_H

#include <linux/types.h>

/* Standard MMC commands (4.1)           type  argument     response */
   /* class 1 */
#define MMC_SWITCH                6   /* ac   [31:0] See below   R1b */
#define MMC_SEND_EXT_CSD          8   /* adtc                    R1  */

/*
 * EXT_CSD fields
 */

#define EXT_CSD_BKOPS_EN		163	/* R/W */
#define EXT_CSD_BKOPS_START		164	/* W */
#define EXT_CSD_HS_TIMING		185	/* R/W */
#define EXT_CSD_BKOPS_STATUS		246	/* RO */
#define EXT_CSD_BKOPS_SUPPORT		502	/* RO */



/*
 * EXT_CSD field definitions
 */

#define EXT_CSD_CMD_SET_NORMAL		(1<<0)

#define EXT_CSD_TIMING_BC	0	/* Backwards compatility */
#define EXT_CSD_TIMING_HS	1	/* High speed */
#define EXT_CSD_TIMING_HS200	2	/* HS200 */
#define EXT_CSD_TIMING_HS400	3	/* HS400 */

/*
 * BKOPS modes
 */
#define EXT_CSD_MANUAL_BKOPS_MASK	0x01
#define EXT_CSD_AUTO_BKOPS_MASK		0x02

/*
 * MMC_SWITCH access modes
 */
#define MMC_SWITCH_MODE_WRITE_BYTE	0x03	/* Set target to value */

#endif /* LINUX_MMC_MMC_H */
// clang-format on
