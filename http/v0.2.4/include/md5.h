/*
 ***********************************************************************
 ** md5.h -- header file for implementation of MD5                    **
 ** RSA Data Security, Inc. MD5 Message-Digest Algorithm              **
 ** Created: 2/17/90 RLR                                              **
 ** Revised: 12/27/90 SRD,AJ,BSK,JT Reference C version               **
 ** Revised (for MD5): RLR 4/27/91                                    **
 **   -- G modified to have y&~z instead of y&z                       **
 **   -- FF, GG, HH modified to add in last register done             **
 **   -- Access pattern: round 2 works mod 5, round 3 works mod 3     **
 **   -- distinct additive constant for each step                     **
 **   -- round 4 added, working mod 7                                 **
 ***********************************************************************
 */

/*
 ***********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.  **
 **                                                                   **
 ** License to copy and use this software is granted provided that    **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message-     **
 ** Digest Algorithm" in all material mentioning or referencing this  **
 ** software or this function.                                        **
 **                                                                   **
 ** License is also granted to make and use derivative works          **
 ** provided that such works are identified as "derived from the RSA  **
 ** Data Security, Inc. MD5 Message-Digest Algorithm" in all          **
 ** material mentioning or referencing the derived work.              **
 **                                                                   **
 ** RSA Data Security, Inc. makes no representations concerning       **
 ** either the merchantability of this software or the suitability    **
 ** of this software for any particular purpose.  It is provided "as  **
 ** is" without express or implied warranty of any kind.              **
 **                                                                   **
 ** These notices must be retained in any copies of any part of this  **
 ** documentation and/or software.                                    **
 ***********************************************************************
 */

#ifndef __HTTP_MD5__
#define __HTTP_MD5__ 1

#ifndef HTTP_MD5_DIGEST_LENGTH
#define HTTP_MD5_DIGEST_LENGTH 16
#endif

#define HTTP_MD5_SIZE 16		    // MD-5 response is always 16 bytes
// HTTP_MD5_SIZE is same as HTTP_MD5_DIGEST_LENGTH defined by openSSL

/* typedef a 32-bit type */
typedef unsigned long int HTTP_MD5_UINT4;

typedef unsigned char HTTP_MD5_UINT8;

/* Data structure for MD5 (Message-Digest) computation */
typedef struct HTTP_MD5_ctx
{
  HTTP_MD5_UINT4 i[2];          /* number of _bits_ handled mod 2^64 */
  HTTP_MD5_UINT4 buf[4];        /* scratch buffer */
  HTTP_MD5_UINT8 in[64];         /* input buffer */
  HTTP_MD5_UINT8 digest[16];     /* actual digest after MD5Final call */
} HTTP_MD5_CTX;

void HTTP_MD5_Init(HTTP_MD5_CTX *mdContext);
void HTTP_MD5_Update(HTTP_MD5_CTX *mdContext, const HTTP_MD5_UINT8* inBuf, unsigned int inLen);
void HTTP_MD5_Final(HTTP_MD5_CTX *mdContext);

#endif		// MD5

