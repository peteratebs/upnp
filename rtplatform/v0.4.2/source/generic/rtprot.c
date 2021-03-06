/*
|  RTPROT.C -
| 
|  EBS -
| 
|   $Author: vmalaiya $
|   $Date: 2006/07/17 15:29:00 $
|   $Name:  $
|   $Revision: 1.3 $
| 
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/

#include "rtprot.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

/*****************************************************************************/
/* Types
 *****************************************************************************/

/*****************************************************************************/
/* Function Prototypes
 *****************************************************************************/

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*----------------------------------------------------------------------*
                             rtp_lrotl
 *----------------------------------------------------------------------*/
/** @memo   Long rotate left.

    @doc    Rotate the value 'rotate' to the left 'nbits'.

    @return The rotated value.
 */
unsigned long rtp_lrotl (
  unsigned long rotate,                 /** The value to be rotated. */
  int nbits                             /** Number of bits to rotate the value. */
  )
{
    return ((((rotate)<<(nbits))|(((rotate)&0xffffffff)>>(32-(nbits)))));
}


/*----------------------------------------------------------------------*
                             rtp_lrotr
 *----------------------------------------------------------------------*/
/** @memo   Long rotate right.

    @doc    Rotate the value 'rotate' to the right 'nbits'.

    @return The rotated value.
 */
unsigned long rtp_lrotr (
  unsigned long rotate,                 /** The value to be rotated. */
  int nbits                             /** Number of bits to rotate the value. */
  )
{
    return ((((rotate)>>(nbits))|(((rotate)&0xffffffff)<<(32-(nbits)))));
}
