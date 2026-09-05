/*
 *  Released under the GNU GPL.  See http://www.gnu.org/licenses/gpl.txt
 *
 *  This program is part of the DeSmet C Compiler
 *
 *  DeSmet C is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundatation; either version 2 of the License, or any
 *  later version.
 *
 *  DeSmet C is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 */
/*	PASS1.H			INCLUDE FOR PASS1	*/

#define	CHECK	1			/*	true if generating checkout compiler */
#define LIMITED 0

/*	LTYPE VALUES	*/

#define LETTER 1
#define DIGIT 2
#define SPACE 3

#define DEFSTR	254
#define DEFEND	255

/*	symbol table types	*/

#define RESERVED	1		/* BYTE CONTAINING RESERVED VALUE */
	struct {char rescl,rvalue;};

#define STAG		2		/* forward chain, size of structure */
	struct {char stagcl; int schain,staglen;};

#define DEFINED		3		/* #args, (arg # or DEFSTR chars... LF)
								...DEFEND	*/
	struct {char defcl,dargs,dval;};

#define DEFPARM		4		/* number of argument	*/
	struct {char dpcl,dpnum;};

#define OPERAND		26		/* op class, forward chain of members,
							 length of name, storage, offset or ordinal */
	struct {char opcl; int nchain; char nlen,nstor; int noff; char ntype[1];};


/*	named heir values */

#define CONSTANT	27
#define	LCONSTANT	28
#define FCONSTANT	29
#define STRNG		30
#define UNDEF		99

/*	storage classes. in nstor field.	*/

#define SEXTONLY	0
#define SSTATIC		1
#define SEXTERN		2
#define SUNSTOR		3
#define SMEMBER		4
#define SAUTO		5
#define SPARM		6
#define STYPEDEF	7

#define LF			10
#define	CR			13
#define CONTZ		26

/*	the reserved words */
#define RCHAR		1
#define RINT		2
#define RSTRUCT		3
#define RUNION		4
#define RLONG		5
#define RSHORT		6
#define RUNSG		7
#define RFLOAT		8
#define RDOUBLE		9
#define RAUTO		10
#define RREG		11
#define RTYPEDEF	12
#define RSTATIC		13
#define REXTERN		14
#define RENUM		15
#define RSIZEOF		16
#define RBREAK		17
#define RCONTINUE	18
#define RDO			19
#define RFOR		20
#define RGOTO		21
#define RIF			22
#define RRETURN		23
#define RSWITCH		24
#define RWHILE		25
#define RCASE		26
#define RDEFAULT	27
#define RELSE		28
#define RASM		29


	unsigned tree,control,infile,ntree,*intree;
	unsigned maxmem,minmem,hiwater;
	int  cline,last,xline,retnum;
	int  *wp,findover,nerr,nwarn,locoff,locplus;
	int  ordinal;
	int  pbytes,wvalue;
	char curch,*cur,*tokat,heir,bvalue,*funname,*original,*mfree,*hash[33];
	char c_type[1],int_type[1],u_type[1],l_type[1],f_type[1],p_type[2];
	char inbuf[2225],treebuf[512],ctlbuf[512];
	char string[202],nested,*bptr,i,j,ininit,was_ext;
	char *addat,*addtype,addstor,*addloc,*addparm,hashno,defhash,laststmt;
	char *inctl,*initnode,*nameat,in_stru,funtype,longconst;
	char is_big,eopt,istring[40],sopt,topt,xopt,wopt,have_asm,see_exit;
	long dvalue;
	unsigned fvalue[4],dsize(),fun_ret_len;
	int  objnum;
	char *inobj,newline,aopt,copt,objname[65];
	extern char ltype[128];
#if	CHECK
	char objbuf[512];
#endif

#define MAXINC	3
	unsigned incfile[MAXINC],lastline[MAXINC];
	char incnext,incname[MAXINC][65],incbuf[MAXINC][689];

#define MAXNEST 60
	int  nestfrom[MAXNEST],nestto[MAXNEST];

	struct stag {char cstype, *stat;} stagat;
	struct {char sttype; struct stag *ststagat; } *struct_type;

	union  { char byte; int word; } ;
	struct { unsigned loword,hiword; } ;
