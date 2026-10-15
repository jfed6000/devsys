#include "op.h"
#include "actions.h"

direct int opsdone;
int labelnum = 0;
action *acslots[128];

static char empty[1] = {'\0'};	/* the empty string */

extern char *bratab[];

#ifdef DEBUG
static char *p1 = "<1>";
static char *p2 = "<2>";
static char *zero = "<any>";

char *
strinst (val)
char *val;
{
	switch ((int) val) {
		case 0:		return zero;
		case 1:		return p1;
		case 2:		return p2;
		default:	return val;
	}
}


dbgaction (act)
action *act;
{
    register instr *ap, *rp;

    debug("\n    %-17s  %s\n","action","replacement");
    for (ap = act->actp, rp = act->repp; ap; ap = ap->nxtins) {
        debug("    %-8s %-8s", strinst(ap->mnp), strinst(ap->opp));
        if (rp) {
            debug(", %-8s %-8s",(rp->mnp ? strinst(rp->mnp) : "<same>"),
                                (rp->opp ? strinst(rp->opp) : "<same>"));
            rp = rp->nxtins;
        }
        debug("\n");
    }
}
#endif

static instr *
mkInstr(line)
char *line;
{
	instr *i = grab(sizeof(instr));
	register char *c = line;
	char *idx, *mnem, *oper;
	int count;

	i->nxtins = NULL;
	while (*c) {

		if (idx = index(c, ' ')) {
			count = (idx - c);
		} else {	/* this is an error, no possible operand */
			error ("incomplete instruction");
		}

		if (count == 1 && *c == '*') {
			i->mnp = NULL;	/* "don't care" */
		} else if (*c == '^') switch (*(c+1)) {
			case '0':
				i->mnp = NULL;
				break;
			case '1':
				i->mnp = (char *)1;
				break;
			case '2':
				i->mnp = (char *)2;
				break;
			default:
				error ("bad opcode reference");
				break;
		} else {
			/* devsys: the instructions' table of mnemonics, so
			 * each is kept once, not once per pattern (D61) */
			char m[MNEMSIZE];

			if (count >= MNEMSIZE)
				error ("bad mnemonic");
			strncpy (m, c, count);
			m[count] = '\0';
			mnem = mnemtab[mnemix (m)];
#if 0 && defined DEBUG
			debug("\nmnem: \"%s\"", mnem);
#endif
			i->mnp = mnem;
		}

		c += count + 1;
		if (idx = index(c, ' ')) {
			count = (idx - c);
		} else {	/* this is the last operand */
			count = strlen(c) - 1;	/* eat the newline */
		}


		if (count == 0) {
			i->opp = empty;	/* empty operands use the same string */
		} else if (count == 1 && *c == '*') {
			i->opp = NULL;	/* "don't care" */
		} else if (*c == '^') switch (*(c+1)) {
			case '0':
				i->opp = NULL;
				break;
			case '1':
				i->opp = (char *)1;
				break;
			case '2':
				i->opp = (char *)2;
				break;
			default:
				error ("bad operand reference");
				break;
		} else {
			oper = grab (count+1);
			strncpy (oper, c, count);
			*(oper+count) = '\0';
#if 0 && defined DEBUG
			debug("\noper: \"%s\"", oper);
#endif
			i->opp = oper;
		}
		c += count + 1;

		if (strlen(c) < 2) { /* we're at the end */
			return i;
		} else {	/* make another instruction to fill */
			instr *i2 = grab (sizeof (instr));

			i2->nxtins = i;
			i = i2;
		}
	}
	return i;
}

loadfile(patfile)
FILE *patfile;
{
	register action *a, **slot;
	char line[LINESIZE];
	int which = 1;

	while (fgets (line, LINESIZE, patfile)) {
		char *c = line;

		if (*c == '#') {
			continue;	/* comment line */
		}
		if (*c == '\n') {
			which = 1;
			continue;
		}

		switch (which) {
			case 1:	{
				instr *i = mkInstr (line);
				a = grab (sizeof (action)); /* new action */
				a->actp = i;
				break;
			}
			case 2:	{
				instr *i = mkInstr (line);
				a->repp = i;
#ifdef DEBUG
				dbgaction(a);
#endif
				if (!a->actp->mnp || index(a->actp->mnp, '*') || index(a->actp->mnp, '?')) {
					slot = &acslots[0];
				} else {
					slot = &acslots[hash (a->actp->mnp)];
				}
				a->nxtact = *slot;
				*slot = a;

				break;
			}
		}

		which++;
	}
}

optinit()
{
	char	path[255];
	char	*dir = CONFDIR;
	char	*pat = "%s/level%d.patterns";
	char	*err = "can't open %s\n";
	FILE	*patfile;

	sprintf(path, pat, dir, 1);
	if (!(patfile = fopen (path, "r")))
		error (err, path);

	loadfile(patfile);
	fclose(patfile);

	if (l2flag) {
		sprintf(path, pat, dir, 2);
		if (!(patfile = fopen (path, "r")))
			error (err, path);

		loadfile(patfile);
		fclose(patfile);
	}
}


#define isconbra(a) (((a) & BRAMASK) > (BRANCH|SUBR))

/* devsys: the instructions are banked records (op.h): a mapped pointer
 * is used only until the next INS/LAB/CHN/ARG, so values go through
 * locals and texts are compared from copies */

/* the operand text of instruction i, copied to buf; NULL for a branch
 * (its operand is a label, which no pattern operand matches) and for
 * the list head */
static char *
opnd(i, buf)
H i;
char *buf;
{
    if (i == ilist || (I_ITYPE(i) & BRANCH))
        return NULL;
    return argcopy(i, buf);
}

/* Fix: Added return type void */
void
optim(i)
H i;
{
    H i2, t, lab;
    int it, c2;
    action *a;
    instr *ap, *mp;
    char labc, c, found, fallback = FALSE;
    char mbuf[MNEMSIZE+1], pbuf[ARGSIZE+1], abuf[ARGSIZE+1], *s, *ps;

    i = I_PRED(i);
    for (;;) {
loop:   if (i == ilist || (i2 = I_SUCC(i)) == ilist)
            return;
        if (I_LLIST(i2)) {    /* look for jump to next instruction */
            it = I_ITYPE(i);
            if ((it & BRANCH)
                        && (it & BRAMASK) != (BRANCH|SUBR)) {
                lab = I_ARG(i);
                /* does it jump to one of i2's labels ? */
                if (L_DEST(lab) == i2) {
                    /* transfer i's labels to i2 */
                    movlab(i,i2);
                    remins(i);
                    i = I_PRED(i2);
                    ++opsdone;
                    goto loop; /* TODO: use continue? */
                } else if ((it & CODEBRK)
                            && I_LLIST(i) == 0
                            && (i = I_PRED(i)) != ilist
                            && isconbra(I_ITYPE(i))
                            && (lab = I_ARG(i), L_DEST(lab) == i2)) {
                    fixcond(i);
                }
            }
            return;
        }
#ifdef DEBUG
        debug("actions search:\n");
#endif
        if (!(a = acslots[hash(MNEMOF(i2))])) {
            a = acslots[0];
            fallback = TRUE;
        }

restart:
        for (found = FALSE; a ; a = a->nxtact) {
#ifdef DEBUG
            debug("  matching action %p.\n",a);
#endif
            i = i2;
            labc = c = 0;
            for (ap = a->actp; ap; i = I_PRED(i), ap = ap->nxtins, ++c) {
                if (i == ilist) break;
                if (!labc && I_LLIST(i)) labc = c; /* has label(s) at pos c */
                if (labc && labc < c) {
#ifdef DEBUG
                    debug("    inside label prevents match.\n");
#endif
                    break;
                }
                if (ap->mnp && match(MNEMOF(i),ap->mnp) == 0) break;
                if (ap->opp) {
                    if ((s = opnd(i, abuf)) == NULL) break;
                    if ((int)ap->opp == 1) {
                        t = I_PRED(i);
                        if (t == ilist) break;
                        if ((ps = opnd(t, pbuf)) == NULL) break;
                        if (match(s,ps) == 0) break;
                    } else if (match(s,ap->opp) == 0) break;
                }
#ifdef DEBUG
                debug("    matched: %s %s <=> %s %s\n",MNEMOF(i),abuf,
                               strinst(ap->mnp), strinst(ap->opp));
#endif
            }
            if (ap == NULL) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            if (!fallback) {
                fallback = TRUE;
                a = acslots[0];
                goto restart;
            }
            return;
        }
#ifdef DEBUG
        debug("  all matched action %p.\n",a);
#endif

        /* all matched - replace */
		i = i2;
		for (ap = a->repp, mp = a->actp; ap; ap = ap->nxtins,mp = mp->nxtins) {
			char *ts;
			t = I_PRED(i);
			if (ap->mnp) {
				if ((int)ap->mnp == 1) /* do nothing, same as NULL */;
				else if ((int)ap->mnp == 2) {
					c2 = I_MNEM(t);
					INS(i)->mnem = c2;
				} else if (ts = index(ap->mnp, '\\')) {	// mnp has \#
					if (*(ts+1) > '0' && *(ts+1) < '3') {
						strcpy(mbuf, MNEMOF(i));
						strcpy(pbuf, MNEMOF(t));
						matchcpy (ap->mnp,
						          mbuf, mp->mnp,
						          pbuf,
						          (mp->nxtins) ? mp->nxtins->mnp : 0);
						INS(i)->mnem = mnemix(mbuf);
					} else error("bad opcode reference");
				} else {
					INS(i)->mnem = mnemix(ap->mnp);
				}
            }
            if (ap->opp) {
				if ((int)ap->opp == 1) /* do nothing, same as NULL */;
				else if ((int)ap->opp == 2) {
					argcopy(t, pbuf);
					setarg(i, pbuf);
				} else if (ts = index(ap->opp, '\\')) {	// opp has \#
					if (*(ts+1) > '0' && *(ts+1) < '3') {
						argcopy(i, abuf);
						if (opnd(t, pbuf) == NULL) *pbuf = '\0';
						matchcpy (ap->opp,
						          abuf, mp->opp,
						          pbuf,
						          (mp->nxtins) ? mp->nxtins->opp : 0);
						setarg(i, abuf);
					} else error("bad operand reference");
				} else {
					setarg(i, ap->opp);
				}
            }
            i = I_PRED(i);
            --c;
        }

        /*
         * delete the instructions we don't need any more, moving any labels
         * we find to the new first instruction
         */
        for (i2 = i, i = I_SUCC(i); c; --c) {
            if (I_LLIST(i2)) movlab(i2,i);
            i2 = I_PRED(i2);
            remins(I_SUCC(i2));
            ++opsdone;
        }
        return;
    }
}


fixcond(i)
H i;
{
    H s, lab;
    int it;

    s = I_SUCC(i);
    it = I_ITYPE(s);
    it = ((I_ITYPE(i) & BRAMASK) ^ 1) | (it & ~(CODEBRK|BRAMASK));
    INS(i)->itype = it;
    INS(i)->mnem = mnemix(bratab[it & BRATYPE]);
    lab = I_ARG(i);
    remref(i,lab);
    lab = I_ARG(s);
    INS(i)->arg = lab;
    insref(i,lab);
    remins(I_SUCC(i));
    ++opsdone;
}


/* Fix: Added return type void */
void
finddupl(i)
H i;
{
    H ip1, lbp, rlp1, rlp2, lmovp, t, u, lab;
    int it;
    char *mbuf, abuf[ARGSIZE+1];

    if (I_PRED(i) == ilist) return;
    /* move labels on "bra"s to here */
    lmovp = 0;
    /* travel down label list */
    for (lbp = I_LLIST(i); lbp; lbp = L_NEXTL(lbp))
        /* travel down reference list */
        for (rlp1 = L_RLIST(lbp); rlp1; rlp1 = C_SUCC(rlp1)) {
            ip1 = C_PRED(rlp1);
            it = I_ITYPE(ip1);
            /* is reference labeled code-break, not blocked ? */
            if (I_LLIST(ip1)
                    && (it & CODEBRK)
                    && !(it & BLOCKDUP)) {
#ifdef DEBUG
                debug("labeled branch adjusted: ");
                prtins(ip1);
#endif
                rlp2 = newchain();
                CHN(rlp2)->succ = lmovp;
                lmovp = rlp2;
                CHN(lmovp)->pred = ip1;
            }
        }
    while (lmovp) {
        ip1 = C_PRED(lmovp);
        movlab(ip1,i);
        if ((ip1 = I_PRED(ip1)) != ilist && ((it = I_ITYPE(ip1)) & BRANCH)) {
            if (it & CODEBRK)
                remins(I_SUCC(ip1));
            else if (isconbra(it)) {
                lab = I_ARG(ip1);
                t = L_DEST(lab);
                u = I_SUCC(ip1);
                if (t == I_SUCC(u))
                    fixcond(ip1);
            }
        }
        rlp2 = C_SUCC(lmovp);
        freechain(lmovp);
        lmovp = rlp2;
    }
    t = I_PRED(i);
    if (!(I_ITYPE(t) & CODEBRK)) {
        /* travel down label list */
        for (lbp = I_LLIST(i); lbp; lbp = L_NEXTL(lbp))
            /* travel down reference list */
            for (rlp1 = L_RLIST(lbp); rlp1; rlp1 = C_SUCC(rlp1)) {
                t = C_PRED(rlp1);
                if (I_ITYPE(t) & CODEBRK) {
                    /* match code before branches to another label */
                    /* of instruction with code before this branch */
                    u = L_NEXTL(lbp);
                    duplrefs(u,t);
                }
            }
        /* match code before label with code before branches */
        t = I_LLIST(i);
        duplrefs(t,i);
    }
    if ((it = I_ITYPE(i)) & CODEBRK) {
        if (it & BRANCH) {
            lab = I_ARG(i);
            /* does branch label know its destination ? */
            if (ip1 = L_DEST(lab)) {
                /* code before branch match code before label ? */
                t = I_PRED(i);
                u = I_PRED(ip1);
                if (!duplsrch(t,u)) {
                    /* match code before other branches with */
                    /* code before this branch */
                    t = I_LLIST(ip1);
                    duplrefs(t,i);
                }
            } else
                /* match code before other branches to same */
                /* label with code before this branch */
                duplrefs(lab,i);
        } else {
            mbuf = MNEMOF(i);
            argcopy(i, abuf);
            /* travel down instruction list */
            for (ip1 = I_PRED(i); ip1 != ilist; ip1 = I_PRED(ip1)) {
                it = I_ITYPE(ip1);
                if (!(it & CODEBRK)
                        || (it & (BLOCKDUP|BRANCH)))
                    continue;
                if (strcmp(mbuf,MNEMOF(ip1)) == 0
                        && (t = I_ARG(ip1), strcmp(abuf,ARG(t)) == 0)
                        && (t = I_PRED(ip1), u = I_PRED(i), duplsrch(t,u)))
                    break;
            }
        }
    }
}


/* do instructions ip1 and ip2 count as the same? (duplsrch) */
static
sameins(ip1, ip2)
H ip1, ip2;
{
    H l1, l2, d1, d2;
    int t1, t2;
    char abuf[ARGSIZE+1];

    t1 = I_ITYPE(ip1);
    t2 = I_ITYPE(ip2);
    if ((t1 & BRAMASK) != (t2 & BRAMASK) || (t1 & BLOCKDUP))
        return 0;
    if (t1 & BRANCH) {
        l1 = I_ARG(ip1);
        l2 = I_ARG(ip2);
        if (d1 = L_DEST(l1)) {
            d2 = L_DEST(l2);
            return d1 == d2;
        }
        return l1 == l2;
    }
    if (strcmp(MNEMOF(ip1), MNEMOF(ip2)) != 0)
        return 0;
    argcopy(ip1, abuf);
    l2 = I_ARG(ip2);
    return strcmp(abuf, ARG(l2)) == 0;
}


duplsrch(ip1,ip2)
H ip1,ip2;
{
    char bralbl[LABELSIZE+1];
    H ip1b,ip2b,lbp;
    int count;

    count = 0;
    if ((ip1b = ip1) == (ip2b = ip2)) return 0;
    while (ip1 != ilist  &&  ip2 != ilist
            &&  ip1 != ip2b  &&  ip2 != ip1b
            &&  sameins(ip1, ip2)) {
        ip1 = I_PRED(ip1);
        ip2 = I_PRED(ip2);
        ++count;
    }
#ifdef DEBUG
    debug("duplsrch: count = %d\n",count);
    debug("    %04x ",I_ITYPE(ip1)); prtins(ip1);
    debug("    %04x ",I_ITYPE(ip2)); prtins(ip2);
#endif
    ip1 = I_SUCC(ip1);
    if (count) {
        ip2b = ip2 = I_SUCC(ip2);
        /* save first instruction's label list */
        if ((lbp = I_LLIST(ip1)) == 0)
            if ((lbp = I_LLIST(ip2)) == 0) {
                lbp = inslabel(newbra(bralbl));
                LAB(lbp)->dest = ip2;
                INS(ip2)->llist = lbp;
            }
        while (count-- >= 0) {
            movlab(ip1,ip2);
            ip1 = I_SUCC(ip1);
            remins(I_PRED(ip1));
            ++opsdone;
            ip2 = I_SUCC(ip2);
        }
        strcpy(bralbl, LAB(lbp)->lname);
        insins(ip1,"lbra",bralbl,NULL);
        optim(ip1);
        /* travel down first instruction's label list */
        lbp = I_LLIST(ip2b);
        duplrefs(lbp,ip2b);
        return 1;
    }
    return 0;
}

duplrefs(lbp,ip)
H lbp;
H ip;
{
    H ip1, rlp, t, u;
    int it;

    /* travel down label list */
    for ( ; lbp; lbp = L_NEXTL(lbp))
        /* travel down reference list */
        for (rlp = L_RLIST(lbp); rlp; rlp = C_SUCC(rlp)) {
            ip1 = C_PRED(rlp);
            it = I_ITYPE(ip1);
            if ((it & CODEBRK)
                   && (it & BLOCKDUP) == 0
                   && (t = I_PRED(ip1), u = I_PRED(ip), duplsrch(t,u)))
                break;
        }
}
