/* $OpenBSD: dh.c,v 1.16 2025/01/19 10:24:17 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <openssl/opensslconf.h>	/* for OPENSSL_NO_DH */

#ifndef OPENSSL_NO_DH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

static struct {
	int check;
	char *infile;
	int informat;
	int noout;
	char *outfile;
	int outformat;
	int text;
} cfg;

static const struct option dh_options[] = {
	{
		.name = "check",
		.desc = "Check the DH parameters",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.check,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file (default stdin)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.infile,
	},
	{
		.name = "inform",
		.argname = "format",
		.desc = "Input format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.informat,
	},
	{
		.name = "noout",
		.desc = "No output",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.noout,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outfile,
	},
	{
		.name = "outform",
		.argname = "format",
		.desc = "Output format (DER or PEM (default))",
		.type = OPTION_ARG_FORMAT,
		.opt.value = &cfg.outformat,
	},
	{
		.name = "text",
		.desc = "Print a text form of the DH parameters",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.text,
	},
	{ NULL },
};

static void
dh_usage(void)
{
	fprintf(stderr,
	    "usage: dh [-check] [-in file] [-inform format]\n"
	    "    [-noout] [-out file] [-outform format] [-text]\n\n");
	options_usage(dh_options);
}

int
dh_main(int argc, char **argv)
{
	DH *dh = NULL;
	int i;
	BIO *in = NULL, *out = NULL;
	int ret = 1;

	if (pledge("stdio cpath wpath rpath", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));

	cfg.informat = FORMAT_PEM;
	cfg.outformat = FORMAT_PEM;

	if (options_parse(argc, argv, dh_options, NULL, NULL) != 0) {
		dh_usage();
		goto end;
	}

	in = BIO_new(BIO_s_file());
	out = BIO_new(BIO_s_file());
	if (in == NULL || out == NULL) {
		ERR_print_errors(bio_err);
		goto end;
	}
	if (cfg.infile == NULL)
		BIO_set_fp(in, stdin, BIO_NOCLOSE);
	else {
		if (BIO_read_filename(in, cfg.infile) <= 0) {
			perror(cfg.infile);
			goto end;
		}
	}
	if (cfg.outfile == NULL) {
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
	} else {
		if (BIO_write_filename(out, cfg.outfile) <= 0) {
			perror(cfg.outfile);
			goto end;
		}
	}

	if (cfg.informat == FORMAT_ASN1)
		dh = d2i_DHparams_bio(in, NULL);
	else if (cfg.informat == FORMAT_PEM)
		dh = PEM_read_bio_DHparams(in, NULL, NULL, NULL);
	else {
		BIO_printf(bio_err, "bad input format specified\n");
		goto end;
	}
	if (dh == NULL) {
		BIO_printf(bio_err, "unable to load DH parameters\n");
		ERR_print_errors(bio_err);
		goto end;
	}
	if (cfg.text) {
		DHparams_print(out, dh);
	}
	if (cfg.check) {
		if (!DH_check(dh, &i)) {
			ERR_print_errors(bio_err);
			goto end;
		}
		if (i & DH_CHECK_P_NOT_PRIME)
			printf("p value is not prime\n");
		if (i & DH_CHECK_P_NOT_SAFE_PRIME)
			printf("p value is not a safe prime\n");
		if (i & DH_UNABLE_TO_CHECK_GENERATOR)
			printf("unable to check the generator value\n");
		if (i & DH_NOT_SUITABLE_GENERATOR)
			printf("the g value is not a generator\n");
		if (i == 0)
			printf("DH parameters appear to be ok.\n");
	}
	if (!cfg.noout) {
		if (cfg.outformat == FORMAT_ASN1)
			i = i2d_DHparams_bio(out, dh);
		else if (cfg.outformat == FORMAT_PEM)
			i = PEM_write_bio_DHparams(out, dh);
		else {
			BIO_printf(bio_err, "bad output format specified for outfile\n");
			goto end;
		}
		if (!i) {
			BIO_printf(bio_err, "unable to write DH parameters\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	ret = 0;

 end:
	BIO_free(in);
	BIO_free_all(out);
	DH_free(dh);

	return (ret);
}
#endif
