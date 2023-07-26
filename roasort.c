/*
 * Copyright (c) 2023 Job Snijders <job@fastly.com>
 * Copyright (c) 2023 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/tree.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct roa_elem {
        int afi;
	unsigned char addr[16];
	int prefixlength;
        int maxlength;
	RB_ENTRY(roa_elem) entry;
};

/*
 * The comparator described in draft-ietf-sidrops-rfc6482bis.
 */
static int
roa_elem_cmp(const struct roa_elem *a, const struct roa_elem *b)
{
       int length, cmp;

       if (a->afi != b->afi)
               return a->afi < b->afi ? -1 : 1;

       length = a->afi == AF_INET ? 4 : 16;

       if ((cmp = memcmp(a->addr, b->addr, length)) != 0)
               return cmp < 0 ? -1 : 1;

       if (a->prefixlength != b->prefixlength)
		return a->prefixlength < b->prefixlength ? -1 : 1;

       if (a->maxlength != b->maxlength)
               return a->maxlength < b->maxlength ? -1 : 1;

       return 0;
}

RB_HEAD(tree, roa_elem) head = RB_INITIALIZER(&head);
RB_PROTOTYPE(tree, roa_elem, entry, roa_elem_cmp)
RB_GENERATE(tree, roa_elem, entry, roa_elem_cmp)

int
main(int argc, char *argv[])
{
	char *line = NULL, *address, *plen, *mlen;
	size_t linesize = 0;
	ssize_t linelen;
	long lval;
	struct addrinfo hint, *res = NULL;
	struct roa_elem *elem, *elemtmp;
	int rc = 0;

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		address = line;

		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';

		if ((elem = malloc(sizeof(struct roa_elem))) == NULL)
			err(1, NULL);

		plen = line;
		line = strsep(&plen, "/");
		if (plen == NULL)
			errx(1, "malformed prefix: %s", line);
		errno = 0;
		lval = atoi(plen);
		if (errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN))
			errx(1, "malformed prefix length: %s", line);
		elem->prefixlength = lval;

		mlen = plen;
		plen = strsep(&mlen, "-");
		if (mlen == NULL)
			elem->maxlength = elem->prefixlength;
		else {
			errno = 0;
			lval = atoi(mlen);
			if (errno == ERANGE && (lval == LONG_MAX
			    || lval == LONG_MIN))
				errx(1, "malformed maxlength: %s", line);
			elem->maxlength = lval;
		}

		memset(&hint, 0, sizeof(struct addrinfo));
		hint.ai_family = PF_UNSPEC;
		hint.ai_flags = AI_NUMERICHOST;

		if (getaddrinfo(address, NULL, &hint, &res))
			errx(1, "malformed IP address: %s", address);

		elem->afi = res->ai_family;

		freeaddrinfo(res);

		if (inet_pton(elem->afi, address, elem->addr) != 1)
			err(1, "inet_pton");

		if (elem->prefixlength > ((elem->afi == AF_INET) ? 32 : 128))
			errx(1, "prefix length too large");

		if (elem->prefixlength > elem->maxlength)
			errx(1, "invalid prefix length / maxlength");

		if (elem->maxlength > ((elem->afi == AF_INET) ? 32 : 128))
			errx(1, "maxlength too large");

		RB_INSERT(tree, &head, elem);
	}

	free(line);
	if (ferror(stdin))
		errx(1, "getline");

	RB_FOREACH_SAFE(elem, tree, &head, elemtmp) {
		char buf[64];

		if (inet_ntop(elem->afi, elem->addr, buf, sizeof(buf)) == NULL)
			err(1, "inet_ntop");

		printf("%s/%i", buf, elem->prefixlength);
		if (elem->prefixlength == elem->maxlength)
			printf("\n");
		else
			printf("-%i\n", elem->maxlength);
		RB_REMOVE(tree, &head, elem);
		free(elem);
	}

	return rc;
}
