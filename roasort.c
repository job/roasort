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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/tree.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct roa_elem {
        int afi;
	unsigned char addr[16];
	int prefixlength;
	int maxlength;
	int i;
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
	char *line = NULL, *oline, *address, *plen, *mlen;
	size_t linesize = 0;
	ssize_t linelen;
	long lval;
	struct addrinfo hint, *res = NULL;
	struct roa_elem *elem, *elemtmp;
	int i = 0, rc = 0;

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		address = line;

		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';

		if ((oline = strdup(line)) == NULL)
			err(1, NULL);

		if ((elem = malloc(sizeof(struct roa_elem))) == NULL)
			err(1, NULL);

		plen = line;
		line = strsep(&plen, "/");
		if (plen == NULL)
			errx(1, "malformed prefix: %s", oline);
		errno = 0;
		lval = atoi(plen);
		if (errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN))
			err(1, "invalid prefix length: %s", oline);
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
				errx(1, "invalid maxlength: %s", oline);
			elem->maxlength = lval;
			if (elem->prefixlength == elem->maxlength)
				rc = 1;
		}

		memset(&hint, 0, sizeof(struct addrinfo));
		hint.ai_family = PF_UNSPEC;
		hint.ai_flags = AI_NUMERICHOST;

		if (getaddrinfo(address, NULL, &hint, &res))
			errx(1, "invalid IP address: %s", oline);

		elem->afi = res->ai_family;

		freeaddrinfo(res);

		if (inet_pton(elem->afi, address, elem->addr) != 1)
			err(1, "inet_pton");

		if (elem->prefixlength > ((elem->afi == AF_INET) ? 32 : 128))
			errx(1, "invalid prefix length, too large: %s", oline);

		if (elem->prefixlength > elem->maxlength)
			errx(1, "invalid prefix length / maxlength: %s", oline);

		if (elem->maxlength > ((elem->afi == AF_INET) ? 32 : 128))
			errx(1, "invalid maxlength, too large: %s", oline);

		free(oline);

		elem->i = i++;

		if (RB_INSERT(tree, &head, elem) != NULL)
			rc = 1;
	}

	free(line);
	if (ferror(stdin))
		err(1, "getline");

	i = 0;
	RB_FOREACH_SAFE(elem, tree, &head, elemtmp) {
		char buf[64];

		if (elem->i != i++)
			rc = 1;

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
