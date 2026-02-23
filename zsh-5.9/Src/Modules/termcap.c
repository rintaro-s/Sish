/*
 * termcap.c - termcap manipulation through curses
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1997 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

/*
 * We need to include the zsh headers later to avoid clashes with
 * the definitions on some systems, however we need the configuration
 * file to decide whether we should avoid curses.h, which clashes
 * with several zsh constants on some systems (e.g. SunOS 4).
 */
#include "../../config.h"

#include "termcap.mdh"
#include "termcap.pro"

/**/
#ifdef HAVE_TGETENT

/* Modern ncurses (Ubuntu 24.04+, Debian Testing) already defines
 * boolcodes, numcodes, and strcodes in term.h as extern variables.
 * We do NOT define them here; instead, we rely on ncurses providing them.
 *
 * For older systems that don't have these in ncurses, they would be
 * provided by the zsh configure system. If missing, we skip using them.
 */

/**/
static int
ztgetflag(char *s)
{
    char **b;

    /* ncurses can tell if an existing boolean capability is *
     * off, but other curses variants can't, so we fudge it. *
     * This feature of ncurses appears to have gone away as  *
     * of NCURSES_MAJOR_VERSION == 5, so don't rely on it.   */
    switch (tgetflag(s)) {
    case -1:
	break;
    case 0:
/* Only try to use boolcodes if it was provided by ncurses */
#ifdef boolcodes
	for (b = (char **)boolcodes; *b; ++b)
	    if (s[0] == (*b)[0] && s[1] == (*b)[1])
		return 0;
#endif
	break;
    default:
	return 1;
    }
    return -1;
}

/* echotc: output a termcap */

/**/
static int
bin_echotc(char *name, char **argv, UNUSED(Options ops), UNUSED(int func))
{
    char *s, buf[2048], *t, *u;
    int num, argct;

    s = *argv++;
    if (termflags & TERM_BAD)
	return 1;
    if ((termflags & TERM_UNKNOWN) && (isset(INTERACTIVE) || !init_term()))
	return 1;
    /* if the specified termcap has a numeric value, display it */
    if ((num = tgetnum(s)) != -1) {
	printf("%d\n", num);
	return 0;
    }
    /* if the specified termcap is boolean, and set, say so  */
    switch (ztgetflag(s)) {
    case -1:
	break;
    case 0:
	puts("no");
	return 0;
    default:
	puts("yes");
	return 0;
    }
    /* get a string-type capability */
    u = buf;
    t = tgetstr(s, &u);
    if (t == (char *)-1 || !t || !*t) {
	/* capability doesn't exist, or (if boolean) is off */
	zwarnnam(name, "no such capability: %s", s);
	return 1;
    }
    /* count the number of arguments required */
    for (argct = 0, u = t; *u; u++)
	if (*u == '%') {
	    if (u++, (*u == 'd' || *u == '2' || *u == '3' || *u == '.' ||
		      *u == '+'))
		argct++;
	}
    /* check that the number of arguments provided is correct */
    if (arrlen(argv) != argct) {
	zwarnnam(name, (arrlen(argv) < argct) ? "not enough arguments" :
		 "too many arguments");
	return 1;
    }
    /* output string, through the proper termcap functions */
    if (!argct)
	tputs(t, 1, putraw);
    else {
	/* This assumes arguments of <lines> <columns> for cap 'cm' */
	num = (argv[1]) ? atoi(argv[1]) : atoi(*argv);
	tputs(tgoto(t, num, atoi(*argv)), 1, putraw);
    }
    return 0;
}

static struct builtin bintab[] = {
    BUILTIN("echotc", 0, bin_echotc, 1, -1, 0, NULL, NULL),
};

/**/
static HashNode
gettermcap(UNUSED(HashTable ht), const char *name)
{
    int len, num;
    char *tcstr, buf[2048], *u, *nameu;
    Param pm = NULL;

    /* This depends on the termcap stuff in init.c */
    if (termflags & TERM_BAD)
	return NULL;
    if ((termflags & TERM_UNKNOWN) && (isset(INTERACTIVE) || !init_term()))
	return NULL;

    
    nameu = dupstring(name);
    unmetafy(nameu, &len);

    pm = (Param) hcalloc(sizeof(struct param));
    pm->node.nam = nameu;
    pm->node.flags = PM_READONLY;
    u = buf;

    /* logic in the following cascade copied from echotc, above */

    if ((num = tgetnum(nameu)) != -1) {
	pm->gsu.i = &nullsetinteger_gsu;
	pm->u.val = num;
	pm->node.flags |= PM_INTEGER;
	return &pm->node;
    }

    pm->gsu.s = &nullsetscalar_gsu;
    switch (ztgetflag(nameu)) {
    case -1:
	break;
    case 0:
	pm->u.str = dupstring("no");
	pm->node.flags |= PM_SCALAR;
	return &pm->node;
    default:
	pm->u.str = dupstring("yes");
	pm->node.flags |= PM_SCALAR;
	return &pm->node;
    }
    if ((tcstr = tgetstr(nameu, &u)) != NULL && tcstr != (char *)-1) {
	pm->u.str = dupstring(tcstr);
	pm->node.flags |= PM_SCALAR;
    } else {
	/* zwarn("no such capability: %s", name); */
	pm->u.str = dupstring("");
	pm->node.flags |= PM_UNSET;
    }
    return &pm->node;
}

/**/
static void
scantermcap(UNUSED(HashTable ht), ScanFunc func, int flags)
{
    Param pm = NULL;
    int num;
    char **capcode, *tcstr, buf[2048], *u;

/* numcodes is provided by ncurses if available; we don't define it here */

/* strcodes is provided by ncurses if available; we don't define it here */

    pm = (Param) hcalloc(sizeof(struct param));
    u = buf;

    pm->node.flags = PM_READONLY | PM_SCALAR;
    pm->gsu.s = &nullsetscalar_gsu;

    /* Only scan boolcodes if ncurses provided them */
#ifdef boolcodes
    for (capcode = (char **)boolcodes; *capcode; capcode++) {
	if ((num = ztgetflag(*capcode)) != -1) {
	    pm->u.str = num ? dupstring("yes") : dupstring("no");
	    pm->node.nam = dupstring(*capcode);
	    func(&pm->node, flags);
	}
    }
#endif

    pm->node.flags = PM_READONLY | PM_INTEGER;
    pm->gsu.i = &nullsetinteger_gsu;

    /* Only scan numcodes if ncurses provided them */
#ifdef numcodes
    for (capcode = (char **)numcodes; *capcode; capcode++) {
	if ((num = tgetnum(*capcode)) != -1) {
	    pm->u.val = num;
	    pm->node.nam = dupstring(*capcode);
	    func(&pm->node, flags);
	}
    }
#endif

    pm->node.flags = PM_READONLY | PM_SCALAR;
    pm->gsu.s = &nullsetscalar_gsu;

    /* Only scan strcodes if ncurses provided them */
#ifdef strcodes
    for (capcode = (char **)strcodes; *capcode; capcode++) {
	if ((tcstr = (char *)tgetstr(*capcode,&u)) != NULL &&
	    tcstr != (char *)-1) {
	    pm->u.str = dupstring(tcstr);
	    pm->node.nam = dupstring(*capcode);
	    func(&pm->node, flags);
	}
    }
#endif
}

static struct paramdef partab[] = {
    SPECIALPMDEF("termcap", PM_READONLY, NULL, gettermcap, scantermcap)
};

/**/
#endif /* HAVE_TGETENT */

static struct features module_features = {
#ifdef HAVE_TGETENT
    bintab, sizeof(bintab)/sizeof(*bintab),
#else
    NULL, 0,
#endif
    NULL, 0,
    NULL, 0,
#ifdef HAVE_TGETENT
    partab, sizeof(partab)/sizeof(*partab),
#else
    NULL, 0,
#endif
    0
};

/**/
int
setup_(UNUSED(Module m))
{
    return 0;
}

/**/
int
features_(Module m, char ***features)
{
    *features = featuresarray(m, &module_features);
    return 0;
}

/**/
int
enables_(Module m, int **enables)
{
    return handlefeatures(m, &module_features, enables);
}

/**/
int
boot_(UNUSED(Module m))
{
#ifdef HAVE_TGETENT
    zsetupterm();
#endif
    return  0;
}

/**/
int
cleanup_(Module m)
{
#ifdef HAVE_TGETENT
    zdeleteterm();
#endif
    return setfeatureenables(m, &module_features, NULL);
}

/**/
int
finish_(UNUSED(Module m))
{
    return 0;
}
