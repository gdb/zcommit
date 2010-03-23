/* Modifications for tzc by Darrell Kindred <dkindred@cmu.edu>, April 1997:
 *   - cache the kerberos credentials, so we can continue to check auth
 *     even if the user re-kinits.
 */

/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZCheckAuthentication function.
 *
 *	Created by:	Robert French
 *
 *	/mit/zephyr/src/CVS/zephyr/lib/zephyr/ZCkAuth.c,v
 *	ghudson
 *
 *	Copyright (c) 1987,1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* /mit/zephyr/src/CVS/zephyr/lib/zephyr/ZCkAuth.c,v 1.21 1995/06/30 22:03:53 ghudson Exp */


#if 0
#include <internal.h>
#else
#include <zephyr/zephyr.h>
#define ZAUTH_UNSET (-3)      /* from internal.h */
#include <stdio.h>	      /* for NULL */
#endif

/* Check authentication of the notice.
   If it looks authentic but fails the Kerberos check, return -1.
   If it looks authentic and passes the Kerberos check, return 1.
   If it doesn't look authentic, return 0

   When not using Kerberos, return true if the notice claims to be authentic.
   Only used by clients; the server uses its own routine.
 */
Code_t ZCheckAuthentication(notice, from)
    ZNotice_t *notice;
    struct sockaddr_in *from;
{	
#ifdef ZEPHYR_USES_KERBEROS
    int result;
    ZChecksum_t our_checksum;
    static CREDENTIALS cred;
    static int got_cred = 0;

    /* If the value is already known, return it. */
    if (notice->z_checked_auth != ZAUTH_UNSET)
	return (notice->z_checked_auth);

    if (!notice->z_auth)
	return (ZAUTH_NO);
	
    if (!got_cred &&
	(result = krb_get_cred(SERVER_SERVICE, SERVER_INSTANCE, 
			       __Zephyr_realm, &cred)) != 0)
      return (ZAUTH_NO);

    got_cred = 1;

#ifdef NOENCRYPTION
    our_checksum = 0;
#else /* NOENCRYPTION */
    our_checksum = des_quad_cksum(notice->z_packet, NULL, 
                                notice->z_default_format+
                                strlen(notice->z_default_format)+1-
                                notice->z_packet, 0, cred.session);
#endif /* NOENCRYPTION */
    /* if mismatched checksum, then the packet was corrupted */
    return ((our_checksum == notice->z_checksum) ? ZAUTH_YES : ZAUTH_FAILED);

#else /* ZEPHYR_USES_KERBEROS */
    return (notice->z_auth ? ZAUTH_YES : ZAUTH_NO);
#endif
} 
