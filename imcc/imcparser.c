/* A Bison parser, made from imcc.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	CALL	257
# define	GOTO	258
# define	ARG	259
# define	PRINT	260
# define	IF	261
# define	UNLESS	262
# define	NEW	263
# define	END	264
# define	SAVEALL	265
# define	RESTOREALL	266
# define	SUB	267
# define	NAMESPACE	268
# define	ENDNAMESPACE	269
# define	CLASS	270
# define	ENDCLASS	271
# define	SYM	272
# define	LOCAL	273
# define	PARAM	274
# define	INC	275
# define	DEC	276
# define	SHIFT_LEFT	277
# define	SHIFT_RIGHT	278
# define	INTV	279
# define	FLOATV	280
# define	STRINGV	281
# define	DEFINED	282
# define	LOG_XOR	283
# define	RELOP_EQ	284
# define	RELOP_NE	285
# define	RELOP_GT	286
# define	RELOP_GTE	287
# define	RELOP_LT	288
# define	RELOP_LTE	289
# define	GLOBAL	290
# define	ADDR	291
# define	CLONE	292
# define	RESULT	293
# define	RETURN	294
# define	POW	295
# define	SHIFT_RIGHT_U	296
# define	LOG_AND	297
# define	LOG_OR	298
# define	COMMA	299
# define	ESUB	300
# define	LABEL	301
# define	EMIT	302
# define	EOM	303
# define	IREG	304
# define	NREG	305
# define	SREG	306
# define	PREG	307
# define	IDENTIFIER	308
# define	STRINGC	309
# define	INTC	310
# define	FLOATC	311
# define	REG	312
# define	MACRO	313
# define	PARROT_OP	314
# define	VAR	315

#line 1 "imcc.y"

/*
 * imcc.y
 *
 * Intermediate Code Compiler for Parrot.
 *
 * Copyright (C) 2002 Melvin Smith <melvin.smith@mindspring.com>
 *
 * Grammar for the parser.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define _PARSER
#define MAIN
#include "imc.h"
#include "pbc.h"
#include "parser.h"

#define YYDEBUG 1

int         yylex();


/*
 * Choosing instructions for Parrot is pretty easy since
 * many are polymorphic.
 */


static SymReg *keys[IMCC_MAX_REGS];
static int nkeys = 0;

static SymReg ** RR(int n, ...)
{
    va_list ap;
    int i = 0;

    va_start(ap, n);
    while (n--) {
	regs[i++] = va_arg(ap, SymReg *);
    }
    va_end(ap);
    while (i < IMCC_MAX_REGS)
	regs[i++] = 0;
    return regs;
}

/* macros for registers */
#define R0() 		RR(nargs=0)
#define R1(r0) 		RR(nargs=1,r0)
#define R2(r0,r1) 	RR(nargs=2,r0,r1)
#define R3(r0,r1,r2) 	RR(nargs=3,r0,r1,r2)
#define R4(r0,r1,r2,r3) RR(nargs=4,r0,r1,r2,r3)

Instruction * iANY(char * name, char *fmt, SymReg **r, int emit);

/*
 * MK_I: build and emitb instruction by iANY
 *
 * fmt may contain:
 *   op %s, %s # comment
 * or just
 *   op
 *
 */
static Instruction * MK_I(char * fmt, SymReg ** r) {
    char opname[64];
    char *p, *q;
    for (p = opname, q = fmt; *q && *q != ' '; )
	*p++ = *q++;
    *p = 0;
    if (!*q)
	fmt = 0;
    else
	fmt = ++q;
#if OPDEBUG
    fprintf(stderr, "op '%s' format '%s' (%d)\n", opname, fmt?:"",nargs);
#endif
    return iANY(opname, fmt, r, 1);
}

/*
 * special instructions
 *
 * labels and such
 */


static void clear_state()
{
    nargs = 0;
    keyvec = 0;
    memset(regs, 0, sizeof(regs));
}

static Instruction * iLABEL(SymReg * r0) {
    Instruction *i = emitb(_mk_instruction("","%s:", R1(r0), 0));
    i->type = ITLABEL;
    clear_state();
    return i;
}


static Instruction * iSUBROUTINE(SymReg * r0) {
    function = r0->name;
    return iLABEL(r0); /* XXX mark label global */
}


/*
 * substr or X = P[key]
 */
static Instruction * iINDEXFETCH(SymReg * r0, SymReg * r1, SymReg * r2) {
    if(r0->set == 'S' && r1->set == 'S' && r2->set == 'I') {
        SymReg * r3 = mk_const("1", 'I');
        return MK_I("substr %s, %s, %s, 1", R4(r0, r1, r2, r3));
    }
    keyvec |= KEY_BIT(2);
    return MK_I("set %s, %s[%s]", R3(r0,r1,r2));
}

/*
 * substr or P[key] = X
 */

static Instruction * iINDEXSET(SymReg * r0, SymReg * r1, SymReg * r2) {
    if(r0->set == 'S' && r1->set == 'I' && r2->set == 'S') {
        SymReg * r3 = mk_const("1", 'I');
        MK_I("substr %s, %s, %s, %s", R4(r0, r1,r3, r2));
    }
    else if (r0->set == 'P') {
        keyvec |= KEY_BIT(1);
	MK_I("set %s[%s], %s", R3(r0,r1,r2));
    }
    else {
        fataly(EX_SOFTWARE, "iINDEXSET", line,"unsupported indexed set op\n");
    }
    return 0;
}

/* return the index of a PMC class */
static int get_pmc_num(char *pmc_type)
{
    STRING * s = string_make(interpreter, pmc_type,
            (UINTVAL) strlen(pmc_type), NULL, 0, NULL);
    PMC * key = key_new_string(interpreter, s);
    return Parrot_base_classname_hash->vtable->get_integer_keyed(
            interpreter, Parrot_base_classname_hash, key);
}

/* only .PmcType */
SymReg * macro(char *name)
{
    SymReg * r;
    char buf[16];
    int type = get_pmc_num(name);
    sprintf(buf, "%d", type);
    r =  mk_const(str_dup(buf), 'I');
    return r;
}

static Instruction *
multi_keyed(char *name, SymReg ** regs, int nr, int emit)
{
    int i, keys, kv, n;
    char buf[16];
    static int p = 0;
    SymReg *preg[IMCC_MAX_REGS];    /* px,py,pz */
    SymReg *nreg[IMCC_MAX_REGS];
    Instruction * ins, *last;

    /* count keys in keyvec */
    kv = keyvec;
    for (i = keys = 0; i < nr; i++, kv >>= 1)
        if (kv & 1)
            keys++;
    if (keys <= 1)
        return 0;
    /* XXX what to do, if we don't emit instruction? */
    assert(emit);
    /* OP  _p_k    _p_k_p_k =>
     * set      py, p_k
     * set      pz,     p_k
     * new px, .PerlUndef
     * OP  px, py, pz
     * set _p_k_px
     */

    kv = keyvec;
    for (i = n = 0; i < nr; i++, kv >>= 1, n++) {
        if (kv & 1) {
            fataly(EX_SOFTWARE, "multi_keyed", line,"illegal key operand\n");
        }
        /* make a new P symbol */
        while (1) {
            sprintf(buf, "$P%d", ++p);
            if (get_sym(buf) == 0)
                break;
        }
        preg[n] = mk_symreg(buf, 'P');
        kv >>= 1;
        if (kv & 1) {
            /* we have a keyed operand */
            if (regs[i]->set != 'P') {
                fataly(EX_SOFTWARE, "multi_keyed", line,"not an aggregate\n");
            }
            nargs = 3;
            /* don't emit LHS yet */
            if (i == 0) {
                keyvec = 1 << 1;
                nreg[0] = regs[i];
                nreg[1] = regs[i+1];
                nreg[2] = preg[n];
                /* set p_k px */
                ins = iANY(str_dup("set"), 0, nreg, 0);
            }
            else {
                keyvec = 1 << 2;
                nreg[0] = preg[n];
                nreg[1] = regs[i];
                nreg[2] = regs[i+1];
                /* set py|z p_k */
                iANY(str_dup("set"), 0, nreg, 1);
            }
            i++;
        }
        /* non keyed */
        else {
            nargs = 2;
            keyvec = 0;
            if (i == 0) {
                nreg[0] = regs[i];
                nreg[1] = preg[n];
                /* set n, px */
                ins = iANY(str_dup("set"), 0, nreg, 0);
            }
            else {
                nreg[0] = preg[n];
                nreg[1] = regs[i];
                /* set px, n */
                iANY(str_dup("set"), 0, nreg, 1);
            }
        }
    }
    /* make a new undef */
    iNEW(preg[0], str_dup("PerlUndef"), 1);
    /* emit the operand */
    nargs = 3;
    keyvec = 0;
    iANY(name, 0, preg, 1);
    /* emit the LHS op */
    emitb(ins);
    return ins;
}

Instruction * iANY(char * name, char *fmt, SymReg **regs, int emit) {
    char fullname[64];
    int i;
    int dirs = 0;
    int op;
    Instruction * ins;

#if 1
    ins = multi_keyed(name, regs, nargs, emit);
    if (ins)
        return ins;
#endif
    op_fullname(fullname, name, regs, nargs);
    op = interpreter->op_lib->op_code(fullname, 1);
    if (op >= 0) {
        op_info_t * info = &interpreter->op_info_table[op];
	char format[128];
	int len;

        *format = '\0';
        /* info->arg_count is offset by one, first is opcode
         * build instruction format
         * set LV_in / out flags */
        for (i = 0; i < info->arg_count-1; i++) {
            switch (info->dirs[i+1]) {
                case PARROT_ARGDIR_INOUT:
                    /* inout is actually in for imcc, the PMC has to exist
                     * previously, so:
                     * goon
                     */
	    case PARROT_ARGDIR_IN:
                    dirs |= 1 << i ;
		break;

	    case PARROT_ARGDIR_OUT:
                    dirs |= 1 << (16 + i);
		break;

	    default:
		assert(0);
	    };
            if (keyvec & KEY_BIT(i)) {
                len = strlen(format);
                len -= 2;
                format[len] = '\0';
                strcat(format, "[%s], ");
	}
            else
                strcat(format, "%s, ");
	}
	len = strlen(format);
	len -= 2;
	format[len] = '\0';
        if (fmt && *fmt)
            strcpy(format, fmt);
        memset(regs + nargs, 0, sizeof(*regs) * (IMCC_MAX_REGS - nargs));
#if 1
        debug(1,"%s %s\t%s\n", name, format, fullname);
#endif
        /* make the instruction */

        ins = _mk_instruction(name, format, regs, dirs);
        if (emit)
             emitb(ins);
        ins->keys |= keyvec;
        /* fill iin oplib's info */
        ins->opnum = op;
        ins->opsize = info->arg_count;
        /* set up branch flags */
        if (info->jump) {
            if (!strcmp(name, "bsr") || !strcmp(name, "ret")) {
                /* ignore subcalls and ret
                 * because they saveall
                 */
            }
            else {
                /* XXX: assume the jump is relative and to the last arg.
                 * usually true.
                 */
                ins->type = ITBRANCH | (1 << (nargs-1));
                if (!strcmp(name, "branch"))
                    ins->type |= IF_goto;
            }
        }
        else if (!strcmp(name, "set") && nargs == 2) {
            /* set Px, Py: both PMCs have the same address */
            if (regs[0]->set == 'P' && regs[1]->set == 'P')
                ins->type |= ITALIAS;
        }
        else if (!strcmp(name, "set_addr")) {
            /* XXX probably a CATCH block */
            ins->type = ITADDR | IF_r1_branch | ITBRANCH;
        }
    } else {
        fataly(EX_SOFTWARE, "iANY", line,"op not found '%s' (%s<%d>)\n",
                fullname, name, nargs);
    }
    return ins;
}


#line 362 "imcc.y"
#ifndef YYSTYPE
typedef union {
    int t;
    char * s;
    SymReg * sr;
    Instruction *i;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		226
#define	YYFLAG		-32768
#define	YYNTBASE	77

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 315 ? yytranslate[x] : 121)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      62,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    64,     2,     2,     2,    70,    72,     2,
       2,     2,    68,    67,     2,    65,    71,    69,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    76,
       2,    63,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    74,     2,    75,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    73,     2,    66,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,     7,    11,    12,    16,    17,    19,
      20,    25,    27,    30,    33,    35,    39,    41,    43,    47,
      49,    52,    53,    56,    57,    59,    62,    64,    67,    69,
      73,    75,    77,    80,    83,    84,    89,    90,    95,    98,
     101,   104,   107,   110,   113,   116,   119,   122,   124,   126,
     128,   131,   132,   134,   136,   138,   140,   142,   146,   151,
     156,   161,   167,   173,   179,   185,   191,   197,   203,   209,
     215,   221,   227,   233,   239,   245,   251,   257,   264,   271,
     276,   281,   289,   294,   299,   304,   309,   310,   316,   321,
     329,   334,   336,   343,   348,   353,   358,   363,   365,   367,
     369,   371,   373,   375,   377,   379,   381,   383,   384,   386,
     390,   392,   394,   399,   401,   403,   405,   407,   409,   410,
     413,   415,   419,   421,   423,   425,   427,   429,   431,   433,
     435,   437,   439,   441,   443
};
static const short yyrhs[] =
{
      86,     0,    79,     0,    78,    79,     0,    92,    80,    62,
       0,     0,    81,    60,    82,     0,     0,   108,     0,     0,
      48,    78,    84,    49,     0,    62,     0,    85,    62,     0,
      86,    87,     0,    87,     0,    88,    89,    46,     0,    83,
       0,    85,     0,    13,    54,    62,     0,    90,     0,    89,
      90,     0,     0,    91,    95,     0,     0,    93,     0,    93,
      94,     0,    94,     0,    94,    62,     0,    47,     0,    92,
      96,    62,     0,   101,     0,   104,     0,    14,    54,     0,
      15,    54,     0,     0,    19,    97,    99,    54,     0,     0,
      20,    98,    99,    54,     0,    20,   118,     0,    39,   112,
       0,     5,   112,     0,    40,   112,     0,     3,    54,     0,
       4,    54,     0,    21,   112,     0,    22,   112,     0,     6,
     112,     0,    11,     0,    12,     0,    10,     0,    60,   108,
       0,     0,    25,     0,    26,     0,    27,     0,   100,     0,
      54,     0,   106,    63,   112,     0,   106,    63,    64,   112,
       0,   106,    63,    65,   112,     0,   106,    63,    66,   112,
       0,   106,    63,   112,    67,   112,     0,   106,    63,   112,
      65,   112,     0,   106,    63,   112,    68,   112,     0,   106,
      63,   112,    41,   112,     0,   106,    63,   112,    69,   112,
       0,   106,    63,   112,    70,   112,     0,   106,    63,   112,
      71,   112,     0,   106,    63,   112,    23,   112,     0,   106,
      63,   112,    24,   112,     0,   106,    63,   112,    42,   112,
       0,   106,    63,   112,    43,   112,     0,   106,    63,   112,
      44,   112,     0,   106,    63,   112,    29,   112,     0,   106,
      63,   112,    72,   112,     0,   106,    63,   112,    73,   112,
       0,   106,    63,   112,    66,   112,     0,   106,    63,   112,
      74,   113,    75,     0,   112,    74,   113,    75,    63,   112,
       0,   106,    63,     9,   100,     0,   106,    63,    28,   112,
       0,   106,    63,    28,   112,    74,   113,    75,     0,   106,
      63,    38,   112,     0,   106,    63,    37,    54,     0,   106,
      63,    36,   120,     0,    36,   120,    63,   112,     0,     0,
       9,   102,   106,    45,   103,     0,    28,   106,    45,   112,
       0,    28,   106,    45,   112,    74,   113,    75,     0,    38,
     106,    45,   112,     0,    59,     0,     7,   112,   105,   112,
       4,    54,     0,     7,   112,     4,    54,     0,     8,   112,
       4,    54,     0,     7,   112,    45,    54,     0,     8,   112,
      45,    54,     0,    30,     0,    31,     0,    32,     0,    33,
       0,    34,     0,    35,     0,    61,     0,   118,     0,    61,
       0,   118,     0,     0,   109,     0,   109,    45,   110,     0,
     110,     0,   111,     0,   107,    74,   113,    75,     0,    54,
       0,   112,     0,    59,     0,    61,     0,   117,     0,     0,
     114,   115,     0,   116,     0,   115,    76,   116,     0,   112,
       0,   118,     0,   119,     0,    50,     0,    51,     0,    52,
       0,    53,     0,    58,     0,    56,     0,    57,     0,    55,
       0,    52,     0,    55,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   398,   403,   404,   407,   409,   409,   411,   414,   418,
     418,   423,   425,   428,   429,   432,   439,   440,   443,   449,
     450,   453,   453,   457,   458,   461,   462,   463,   466,   469,
     472,   474,   475,   476,   477,   477,   478,   478,   480,   481,
     482,   483,   484,   486,   488,   489,   490,   491,   492,   493,
     494,   495,   498,   500,   501,   502,   505,   509,   511,   512,
     513,   514,   515,   516,   517,   518,   519,   520,   521,   522,
     523,   524,   525,   526,   527,   528,   529,   530,   531,   532,
     533,   534,   536,   538,   540,   541,   542,   542,   544,   545,
     547,   550,   553,   556,   558,   560,   562,   567,   569,   570,
     571,   572,   573,   577,   578,   580,   581,   584,   585,   588,
     589,   592,   593,   597,   599,   600,   603,   604,   607,   607,
     611,   612,   615,   618,   619,   622,   623,   624,   625,   626,
     629,   630,   631,   634,   635
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "CALL", "GOTO", "ARG", "PRINT", "IF", 
  "UNLESS", "NEW", "END", "SAVEALL", "RESTOREALL", "SUB", "NAMESPACE", 
  "ENDNAMESPACE", "CLASS", "ENDCLASS", "SYM", "LOCAL", "PARAM", "INC", 
  "DEC", "SHIFT_LEFT", "SHIFT_RIGHT", "INTV", "FLOATV", "STRINGV", 
  "DEFINED", "LOG_XOR", "RELOP_EQ", "RELOP_NE", "RELOP_GT", "RELOP_GTE", 
  "RELOP_LT", "RELOP_LTE", "GLOBAL", "ADDR", "CLONE", "RESULT", "RETURN", 
  "POW", "SHIFT_RIGHT_U", "LOG_AND", "LOG_OR", "COMMA", "ESUB", "LABEL", 
  "EMIT", "EOM", "IREG", "NREG", "SREG", "PREG", "IDENTIFIER", "STRINGC", 
  "INTC", "FLOATC", "REG", "MACRO", "PARROT_OP", "VAR", "'\\n'", "'='", 
  "'!'", "'-'", "'~'", "'+'", "'*'", "'/'", "'%'", "'.'", "'&'", "'|'", 
  "'['", "']'", "';'", "program", "pasmcode", "pasmline", "pasm_inst", 
  "@1", "pasm_args", "emit", "@2", "nls", "subs", "sub", "sub_start", 
  "statements", "statement", "@3", "labels", "_labels", "label", 
  "instruction", "labeled_inst", "@4", "@5", "type", "classname", 
  "assignment", "@6", "newtype", "if_statement", "relop", "target", "lhs", 
  "vars", "_vars", "_var_or_i", "var_or_i", "var", "keylist", "@7", 
  "_keylist", "key", "rc", "reg", "const", "string", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    77,    78,    78,    79,    81,    80,    80,    82,    84,
      83,    85,    85,    86,    86,    87,    87,    87,    88,    89,
      89,    91,    90,    92,    92,    93,    93,    93,    94,    95,
      96,    96,    96,    96,    97,    96,    98,    96,    96,    96,
      96,    96,    96,    96,    96,    96,    96,    96,    96,    96,
      96,    96,    99,    99,    99,    99,   100,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   102,   101,   101,   101,
     101,   103,   104,   104,   104,   104,   104,   105,   105,   105,
     105,   105,   105,   106,   106,   107,   107,   108,   108,   109,
     109,   110,   110,   111,   111,   111,   112,   112,   114,   113,
     115,   115,   116,   117,   117,   118,   118,   118,   118,   118,
     119,   119,   119,   120,   120
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     2,     3,     0,     3,     0,     1,     0,
       4,     1,     2,     2,     1,     3,     1,     1,     3,     1,
       2,     0,     2,     0,     1,     2,     1,     2,     1,     3,
       1,     1,     2,     2,     0,     4,     0,     4,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     1,     1,     1,
       2,     0,     1,     1,     1,     1,     1,     3,     4,     4,
       4,     5,     5,     5,     5,     5,     5,     5,     5,     5,
       5,     5,     5,     5,     5,     5,     5,     6,     6,     4,
       4,     7,     4,     4,     4,     4,     0,     5,     4,     7,
       4,     1,     6,     4,     4,     4,     4,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     0,     1,     3,
       1,     1,     4,     1,     1,     1,     1,     1,     0,     2,
       1,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     0,    23,    11,    16,    17,     1,    14,    21,     0,
      28,    23,     2,     5,    24,    26,    12,    13,    21,    19,
      23,    18,     3,     0,     0,     0,    25,    27,    15,    20,
      51,    22,    10,     4,   107,     0,     0,     0,     0,     0,
       0,    86,    49,    47,    48,     0,     0,    34,    36,     0,
       0,     0,     0,     0,     0,     0,   125,   126,   127,   128,
     132,   130,   131,   129,   107,   103,     0,    30,    31,     0,
       0,   117,   104,   124,   113,   115,   116,     6,     0,     8,
     108,   110,   111,   114,   123,    42,    43,   116,    40,   123,
      46,     0,     0,     0,    32,    33,     0,     0,    38,    44,
      45,   103,     0,   104,   133,   134,     0,     0,    39,    41,
      50,    29,     0,   118,   118,     0,     0,    97,    98,    99,
     100,   101,   102,     0,     0,     0,     0,     0,    52,    53,
      54,    56,     0,    55,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    57,     0,     0,     0,
     109,    93,    95,     0,    94,    96,     0,    35,    37,    88,
      85,    90,    79,    80,    84,    83,    82,    58,    59,    60,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   118,     0,   122,   119,
     120,   112,     0,    91,    87,   118,   118,    68,    69,    73,
      64,    70,    71,    72,    62,    76,    61,    63,    65,    66,
      67,    74,    75,     0,     0,     0,    92,     0,     0,    77,
      78,   121,    89,    81,     0,     0,     0
};

static const short yydefgoto[] =
{
     224,    11,    12,    24,    25,    77,     4,    23,     5,     6,
       7,     8,    18,    19,    20,    13,    14,    15,    31,    66,
      96,    97,   132,   133,    67,    93,   194,    68,   124,    69,
      78,    79,    80,    81,    82,    83,   147,   148,   189,   190,
      71,    89,    73,   106
};

static const short yypact[] =
{
     -12,   -48,   -33,-32768,-32768,   -45,   -12,-32768,-32768,   -44,
  -32768,   -26,-32768,   -40,   -33,   -36,-32768,-32768,   -18,-32768,
     -33,-32768,-32768,   -22,   -32,   -27,-32768,-32768,-32768,-32768,
     153,-32768,-32768,-32768,   165,   -17,   -16,   177,   177,   177,
     177,-32768,-32768,-32768,-32768,   -11,    -3,-32768,    20,   177,
     177,   189,   -39,   189,   177,   177,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,   165,    -9,   -20,-32768,-32768,   -28,
      -8,-32768,     5,-32768,-32768,-32768,     9,-32768,    13,-32768,
       7,-32768,-32768,-32768,    21,-32768,-32768,-32768,-32768,-32768,
  -32768,    58,     1,   189,-32768,-32768,    22,    22,-32768,-32768,
  -32768,-32768,    12,-32768,-32768,-32768,     0,    32,-32768,-32768,
  -32768,-32768,     3,-32768,-32768,   165,    43,-32768,-32768,-32768,
  -32768,-32768,-32768,    48,   177,    50,    54,    65,-32768,-32768,
  -32768,-32768,    62,-32768,    63,   177,   177,   177,    66,   177,
     -39,    67,   177,   177,   177,   177,    57,    36,   177,    44,
  -32768,-32768,-32768,   128,-32768,-32768,    74,-32768,-32768,    60,
  -32768,-32768,-32768,    61,-32768,-32768,-32768,-32768,-32768,-32768,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177,
     177,   177,   177,   177,   177,   177,-32768,    73,-32768,    90,
  -32768,-32768,    83,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,    64,   177,   177,-32768,    94,    95,-32768,
  -32768,-32768,-32768,-32768,   138,   171,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,   166,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     170,-32768,-32768,   160,-32768,   159,-32768,   168,-32768,-32768,
  -32768,-32768,    86,    42,-32768,-32768,-32768,-32768,-32768,   -49,
  -32768,   122,-32768,    72,-32768,   -30,  -111,-32768,-32768,   -25,
  -32768,   -19,-32768,    55
};


#define	YYLAST		250


static const short yytable[] =
{
      70,     1,   102,   149,   107,   125,     9,    88,    90,    91,
      92,    72,   138,   104,    10,    84,   105,    16,    21,    99,
     100,    10,    -7,    -9,   108,   109,    27,    32,    28,    98,
      33,   139,   103,    34,   103,   112,     2,    85,    86,   140,
     141,   142,   111,    94,   127,    84,   126,   128,   129,   130,
       3,    95,   115,    56,    57,    58,    59,   135,    60,    61,
      62,    63,   116,   136,    87,  -116,   113,   143,   144,   145,
      56,    57,    58,    59,   103,   213,   131,   137,    63,  -123,
     170,   171,   146,  -105,   217,   218,   172,   114,   117,   118,
     119,   120,   121,   122,   153,  -106,    84,   151,   173,   174,
     175,   176,   152,   123,   154,   159,   160,   161,   155,   163,
     156,   187,   166,   167,   168,   169,   157,   158,   188,   191,
     131,   165,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   192,   193,   195,   196,   214,   216,   225,   219,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,   215,    45,    46,   222,
     223,   226,    47,    48,    49,    50,    17,    22,    29,    30,
     162,    51,    26,   134,   220,   188,   110,   150,     0,    52,
     221,    53,    54,    55,     0,   164,     0,     0,     0,     0,
       0,     0,     0,    56,    57,    58,    59,     0,    60,    61,
      62,    63,     0,    64,    65,    56,    57,    58,    59,    74,
      60,    61,    62,    63,    75,     0,    76,    56,    57,    58,
      59,     0,    60,    61,    62,    63,     0,     0,    87,    56,
      57,    58,    59,     0,     0,     0,     0,    63,     0,     0,
     101
};

static const short yycheck[] =
{
      30,    13,    51,   114,    53,     4,    54,    37,    38,    39,
      40,    30,     9,    52,    47,    34,    55,    62,    62,    49,
      50,    47,    62,    49,    54,    55,    62,    49,    46,    48,
      62,    28,    51,    60,    53,    63,    48,    54,    54,    36,
      37,    38,    62,    54,    93,    64,    45,    25,    26,    27,
      62,    54,    45,    50,    51,    52,    53,    45,    55,    56,
      57,    58,     4,    63,    61,    74,    74,    64,    65,    66,
      50,    51,    52,    53,    93,   186,    54,    45,    58,    74,
      23,    24,   112,    74,   195,   196,    29,    74,    30,    31,
      32,    33,    34,    35,   124,    74,   115,    54,    41,    42,
      43,    44,    54,    45,    54,   135,   136,   137,    54,   139,
      45,    75,   142,   143,   144,   145,    54,    54,   148,    75,
      54,    54,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,     4,    59,    74,    74,    63,    54,     0,    75,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    76,    14,    15,    75,
      75,     0,    19,    20,    21,    22,     6,    11,    18,    20,
     138,    28,    14,    97,   214,   215,    64,   115,    -1,    36,
     215,    38,    39,    40,    -1,   140,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    50,    51,    52,    53,    -1,    55,    56,
      57,    58,    -1,    60,    61,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    -1,    61,    50,    51,    52,
      53,    -1,    55,    56,    57,    58,    -1,    -1,    61,    50,
      51,    52,    53,    -1,    -1,    -1,    -1,    58,    -1,    -1,
      61
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 1:
#line 399 "imcc.y"
{ yyval.i = 0; }
    break;
case 4:
#line 407 "imcc.y"
{ yyval.i = 0; }
    break;
case 5:
#line 409 "imcc.y"
{clear_state();}
    break;
case 6:
#line 410 "imcc.y"
{ yyval.i = iANY(yyvsp[-1].s,0,regs,1); free(yyvsp[-1].s); }
    break;
case 7:
#line 411 "imcc.y"
{ yyval.i = 0;}
    break;
case 9:
#line 419 "imcc.y"
{ yyval.i = 0;}
    break;
case 10:
#line 420 "imcc.y"
{ emit_flush(); clear_tables();yyval.i=0;}
    break;
case 15:
#line 433 "imcc.y"
{
          yyval.i = 0;
	  allocate();
	  emit_flush();
	  clear_tables();
        }
    break;
case 16:
#line 439 "imcc.y"
{ yyval.i=0; }
    break;
case 17:
#line 440 "imcc.y"
{ yyval.i=0; }
    break;
case 18:
#line 444 "imcc.y"
{ yyval.i = 0;
          iSUBROUTINE(mk_address(yyvsp[-1].s, U_add_uniq_sub));
        }
    break;
case 21:
#line 453 "imcc.y"
{ clear_state(); }
    break;
case 22:
#line 454 "imcc.y"
{ yyval.i = yyvsp[0].i; }
    break;
case 23:
#line 457 "imcc.y"
{ yyval.i = NULL; }
    break;
case 28:
#line 466 "imcc.y"
{ yyval.i = iLABEL(mk_address(yyvsp[0].s, U_add_uniq_label)); }
    break;
case 29:
#line 470 "imcc.y"
{ yyval.i = yyvsp[-1].i; }
    break;
case 32:
#line 475 "imcc.y"
{ push_namespace(yyvsp[0].s); }
    break;
case 33:
#line 476 "imcc.y"
{ pop_namespace(yyvsp[0].s); }
    break;
case 34:
#line 477 "imcc.y"
{ is_def=1; }
    break;
case 35:
#line 477 "imcc.y"
{ mk_ident(yyvsp[0].s, yyvsp[-1].t);is_def=0; }
    break;
case 36:
#line 478 "imcc.y"
{ is_def=1; }
    break;
case 37:
#line 478 "imcc.y"
{ yyval.i = MK_I("restore",
		                            R1(mk_ident(yyvsp[0].s, yyvsp[-1].t)));is_def=0; }
    break;
case 38:
#line 480 "imcc.y"
{ yyval.i = MK_I("restore", R1(yyvsp[0].sr)); }
    break;
case 39:
#line 481 "imcc.y"
{ yyval.i = MK_I("restore", R1(yyvsp[0].sr)); }
    break;
case 40:
#line 482 "imcc.y"
{ yyval.i = MK_I("save", R1(yyvsp[0].sr)); }
    break;
case 41:
#line 483 "imcc.y"
{ yyval.i = MK_I("save", R1(yyvsp[0].sr)); }
    break;
case 42:
#line 484 "imcc.y"
{ yyval.i = MK_I("bsr",
                                              R1(mk_address(yyvsp[0].s, U_add_once)));}
    break;
case 43:
#line 486 "imcc.y"
{ yyval.i = MK_I("branch",
                                              R1(mk_address(yyvsp[0].s, U_add_once)));}
    break;
case 44:
#line 488 "imcc.y"
{ yyval.i = MK_I("inc",R1(yyvsp[0].sr)); }
    break;
case 45:
#line 489 "imcc.y"
{ yyval.i = MK_I("dec",R1(yyvsp[0].sr)); }
    break;
case 46:
#line 490 "imcc.y"
{ yyval.i = MK_I("print",R1(yyvsp[0].sr)); }
    break;
case 47:
#line 491 "imcc.y"
{ yyval.i = MK_I("saveall" ,R0()); }
    break;
case 48:
#line 492 "imcc.y"
{ yyval.i = MK_I("restoreall" ,R0()); }
    break;
case 49:
#line 493 "imcc.y"
{ yyval.i = MK_I("end" ,R0()); }
    break;
case 50:
#line 494 "imcc.y"
{ yyval.i = iANY(yyvsp[-1].s,0,regs, 1); free(yyvsp[-1].s); }
    break;
case 51:
#line 495 "imcc.y"
{ yyval.i = 0;}
    break;
case 52:
#line 499 "imcc.y"
{ yyval.t = 'I'; }
    break;
case 53:
#line 500 "imcc.y"
{ yyval.t = 'N'; }
    break;
case 54:
#line 501 "imcc.y"
{ yyval.t = 'S'; }
    break;
case 55:
#line 502 "imcc.y"
{ yyval.t = 'P'; free(yyvsp[0].s); }
    break;
case 57:
#line 510 "imcc.y"
{ yyval.i = MK_I("set", R2(yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 58:
#line 511 "imcc.y"
{ yyval.i = MK_I("not", R2(yyvsp[-3].sr, yyvsp[0].sr));}
    break;
case 59:
#line 512 "imcc.y"
{ yyval.i = MK_I("neg", R2(yyvsp[-3].sr, yyvsp[0].sr));}
    break;
case 60:
#line 513 "imcc.y"
{ yyval.i = MK_I("bnot", R2(yyvsp[-3].sr, yyvsp[0].sr));}
    break;
case 61:
#line 514 "imcc.y"
{ yyval.i = MK_I("add", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 62:
#line 515 "imcc.y"
{ yyval.i = MK_I("sub", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 63:
#line 516 "imcc.y"
{ yyval.i = MK_I("mul", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 64:
#line 517 "imcc.y"
{ yyval.i = MK_I("pow", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 65:
#line 518 "imcc.y"
{ yyval.i = MK_I("div", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 66:
#line 519 "imcc.y"
{ yyval.i = MK_I("mod", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 67:
#line 520 "imcc.y"
{ yyval.i = MK_I("concat", R3(yyvsp[-4].sr,yyvsp[-2].sr,yyvsp[0].sr)); }
    break;
case 68:
#line 521 "imcc.y"
{ yyval.i = MK_I("shl", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 69:
#line 522 "imcc.y"
{ yyval.i = MK_I("shr", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 70:
#line 523 "imcc.y"
{ yyval.i = MK_I("lsr", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 71:
#line 524 "imcc.y"
{ yyval.i = MK_I("and", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 72:
#line 525 "imcc.y"
{ yyval.i = MK_I("or", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 73:
#line 526 "imcc.y"
{ yyval.i = MK_I("xor", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 74:
#line 527 "imcc.y"
{ yyval.i = MK_I("band", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 75:
#line 528 "imcc.y"
{ yyval.i = MK_I("bor", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 76:
#line 529 "imcc.y"
{ yyval.i = MK_I("bxor", R3(yyvsp[-4].sr, yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 77:
#line 530 "imcc.y"
{ yyval.i = iINDEXFETCH(yyvsp[-5].sr, yyvsp[-3].sr, yyvsp[-1].sr); }
    break;
case 78:
#line 531 "imcc.y"
{ yyval.i = iINDEXSET(yyvsp[-5].sr, yyvsp[-3].sr, yyvsp[0].sr); }
    break;
case 79:
#line 532 "imcc.y"
{ yyval.i = iNEW(yyvsp[-3].sr, yyvsp[0].s, 1); }
    break;
case 80:
#line 533 "imcc.y"
{ yyval.i = MK_I("defined",R2(yyvsp[-3].sr,yyvsp[0].sr)); }
    break;
case 81:
#line 534 "imcc.y"
{ keyvec=KEY_BIT(2);
                                     yyval.i = MK_I("defined", R3(yyvsp[-6].sr, yyvsp[-3].sr, yyvsp[-1].sr));}
    break;
case 82:
#line 536 "imcc.y"
{ yyval.i = MK_I("clone",R2(yyvsp[-3].sr, yyvsp[0].sr));
    }
    break;
case 83:
#line 538 "imcc.y"
{ yyval.i = MK_I("set_addr",
                                          R2(yyvsp[-3].sr, mk_address(yyvsp[0].s,U_add_once))); }
    break;
case 84:
#line 540 "imcc.y"
{ yyval.i = MK_I("find_global",R2(yyvsp[-3].sr,yyvsp[0].sr)); }
    break;
case 85:
#line 541 "imcc.y"
{ yyval.i = MK_I("store_global",R2(yyvsp[-2].sr,yyvsp[0].sr)); }
    break;
case 86:
#line 542 "imcc.y"
{ expect_pasm = 1; }
    break;
case 87:
#line 543 "imcc.y"
{ yyval.i = iNEW(yyvsp[-2].sr, yyvsp[0].s, 1); }
    break;
case 88:
#line 544 "imcc.y"
{ yyval.i = MK_I("defined", R2(yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 89:
#line 545 "imcc.y"
{ keyvec=KEY_BIT(2);
                                       yyval.i = MK_I("defined", R3(yyvsp[-5].sr, yyvsp[-3].sr, yyvsp[-1].sr));}
    break;
case 90:
#line 547 "imcc.y"
{ yyval.i = MK_I("clone", R2(yyvsp[-2].sr, yyvsp[0].sr)); }
    break;
case 92:
#line 554 "imcc.y"
{ yyval.i=MK_I(yyvsp[-3].s,R3(yyvsp[-4].sr,yyvsp[-2].sr,
                                          mk_address(yyvsp[0].s,U_add_once))); }
    break;
case 93:
#line 556 "imcc.y"
{yyval.i= MK_I("if", R2(yyvsp[-2].sr,
                                          mk_address(yyvsp[0].s, U_add_once))); }
    break;
case 94:
#line 558 "imcc.y"
{yyval.i= MK_I("unless",R2(yyvsp[-2].sr,
                                          mk_address(yyvsp[0].s, U_add_once))); }
    break;
case 95:
#line 560 "imcc.y"
{ yyval.i= MK_I("if", R2(yyvsp[-2].sr,
                                          mk_address(yyvsp[0].s, U_add_once))); }
    break;
case 96:
#line 562 "imcc.y"
{ yyval.i= MK_I("unless", R2(yyvsp[-2].sr,
                                          mk_address(yyvsp[0].s, U_add_once))); }
    break;
case 97:
#line 568 "imcc.y"
{ yyval.s = "eq"; }
    break;
case 98:
#line 569 "imcc.y"
{ yyval.s = "ne"; }
    break;
case 99:
#line 570 "imcc.y"
{ yyval.s = "gt"; }
    break;
case 100:
#line 571 "imcc.y"
{ yyval.s = "ge"; }
    break;
case 101:
#line 572 "imcc.y"
{ yyval.s = "lt"; }
    break;
case 102:
#line 573 "imcc.y"
{ yyval.s = "le"; }
    break;
case 107:
#line 584 "imcc.y"
{ yyval.sr = NULL; }
    break;
case 108:
#line 585 "imcc.y"
{ yyval.sr = yyvsp[0].sr; }
    break;
case 109:
#line 588 "imcc.y"
{ yyval.sr = regs[0]; }
    break;
case 111:
#line 592 "imcc.y"
{ regs[nargs++] = yyvsp[0].sr; }
    break;
case 112:
#line 593 "imcc.y"
{ regs[nargs++] = yyvsp[-3].sr;
                                          keyvec |= KEY_BIT(nargs);
                                          regs[nargs++] = yyvsp[-1].sr; yyval.sr = yyvsp[-3].sr; }
    break;
case 113:
#line 598 "imcc.y"
{ yyval.sr = mk_address(yyvsp[0].s, U_add_once); }
    break;
case 115:
#line 600 "imcc.y"
{ yyval.sr = macro(yyvsp[0].s); free(yyvsp[0].s); }
    break;
case 118:
#line 607 "imcc.y"
{ nkeys=0; }
    break;
case 119:
#line 608 "imcc.y"
{ yyval.sr = link_keys(nkeys, keys); }
    break;
case 120:
#line 611 "imcc.y"
{ keys[nkeys++] = yyvsp[0].sr; }
    break;
case 121:
#line 612 "imcc.y"
{ keys[nkeys++] = yyvsp[0].sr; yyval.sr =  keys[0]; }
    break;
case 125:
#line 622 "imcc.y"
{ yyval.sr = mk_symreg(yyvsp[0].s, 'I'); }
    break;
case 126:
#line 623 "imcc.y"
{ yyval.sr = mk_symreg(yyvsp[0].s, 'N'); }
    break;
case 127:
#line 624 "imcc.y"
{ yyval.sr = mk_symreg(yyvsp[0].s, 'S'); }
    break;
case 128:
#line 625 "imcc.y"
{ yyval.sr = mk_symreg(yyvsp[0].s, 'P'); }
    break;
case 129:
#line 626 "imcc.y"
{ yyval.sr = mk_pasm_reg(yyvsp[0].s); }
    break;
case 130:
#line 629 "imcc.y"
{ yyval.sr = mk_const(yyvsp[0].s, 'I'); }
    break;
case 131:
#line 630 "imcc.y"
{ yyval.sr = mk_const(yyvsp[0].s, 'N'); }
    break;
case 132:
#line 631 "imcc.y"
{ yyval.sr = mk_const(yyvsp[0].s, 'S'); }
    break;
case 133:
#line 634 "imcc.y"
{ yyval.sr = mk_symreg(yyvsp[0].s, 'S'); }
    break;
case 134:
#line 635 "imcc.y"
{ yyval.sr = mk_const(yyvsp[0].s, 'S'); }
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 637 "imcc.y"



int yyerror(char * s)
{
    fprintf(stderr, "last token = [%s]\n", yylval.s);
    fprintf(stderr, "(error) line %ld: %s\n", line, s );
    fprintf(stderr, "Didn't create output asm.\n" );
    exit(EX_UNAVAILABLE);
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
*/
