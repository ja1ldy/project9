/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * MIT-MAGIC-COOKIE-1 authorization scheme
 * Author:  Keith Packard, MIT X Consortium
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <X11/X.h>
#include "os.h"
#include "osdep.h"
#include "dixstruct.h"

static struct auth {
    struct auth	*next;
    unsigned short	len;
    char	*data;
    XID		id;
} *mit_auth;

static int
cookie_equal(const char *a, const char *b, unsigned short length)
{
    volatile unsigned char diff = 0;
    unsigned int i;
    if ((!a || !b) && length)
        return 0;
    for (i = 0; i < length; ++i)
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff == 0;
}

int
MitAddCookie (
    unsigned short	data_length,
    const char		*data,
    XID			id)
{
    struct auth	*new;

    if (data_length != 16 || !data)
        return 0;
    new = malloc(sizeof (struct auth));
    if (!new)
	return 0;
    new->data = malloc((unsigned) data_length);
    if (!new->data) {
	free(new);
	return 0;
    }
    new->next = mit_auth;
    mit_auth = new;
    memmove(new->data, data, (int) data_length);
    new->len = data_length;
    new->id = id;
    return 1;
}

XID
MitCheckCookie (
    unsigned short	data_length,
    const char		*data,
    ClientPtr		client,
    const char		**reason)
{
    struct auth	*auth;

    for (auth = mit_auth; auth; auth=auth->next) {
        if (data_length == auth->len &&
	   cookie_equal (data, auth->data, data_length))
	    return auth->id;
    }
    *reason = "Invalid MIT-MAGIC-COOKIE-1 key";
    return (XID) -1;
}

int
MitResetCookie (void)
{
    struct auth	*auth, *next;

    for (auth = mit_auth; auth; auth=next) {
	next = auth->next;
	free(auth->data);
	free(auth);
    }
    mit_auth = 0;
    return 0;
}

XID
MitToID (
	unsigned short	data_length,
	char		*data)
{
    struct auth	*auth;

    for (auth = mit_auth; auth; auth=auth->next) {
	if (data_length == auth->len &&
	    cookie_equal (data, auth->data, data_length))
	    return auth->id;
    }
    return (XID) -1;
}

int
MitFromID (
	XID		id,
	unsigned short	*data_lenp,
	char		**datap)
{
    struct auth	*auth;

    for (auth = mit_auth; auth; auth=auth->next) {
	if (id == auth->id) {
	    *data_lenp = auth->len;
	    *datap = auth->data;
	    return 1;
	}
    }
    return 0;
}

int
MitRemoveCookie (
	unsigned short	data_length,
	const char		*data)
{
    struct auth	*auth, *prev;

    prev = 0;
    for (auth = mit_auth; auth; prev = auth, auth=auth->next) {
	if (data_length == auth->len &&
	    cookie_equal (data, auth->data, data_length))
 	{
	    if (prev)
		prev->next = auth->next;
	    else
		mit_auth = auth->next;
	    free(auth->data);
	    free(auth);
	    return 1;
	}
    }
    return 0;
}

