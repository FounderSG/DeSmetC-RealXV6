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
/*	c6.c					part 6 of medium c compiler		*/

#include "PASS1.H"
#include "NODES.H"

tokit() {
	do 
		;
		while (tokone());
	}

tokone() {

	tokat=cur;
	curch=*cur++;
	heir=0;

	switch (ltype[curch]) {
		case LETTER:find();
					if (heir == DEFINED) {
						if (mfree < nameat) {
							error_l("recursive define");
							return 0;
							}

						addnest();
						whitesp();
						return 1;
						}
					break;
		case DIGIT: number();
					break;
		case 4:		if (*cur == '=') {
						cur++;
						heir=19;
						bvalue=NE;
						break;
						}
					heir=24;
					bvalue=NOT;
					break;
		case 5:		isstring();
					break;
		case 6:		heir=23;
					opeq(MOD);
					break;
		case 7:		if (*cur == '&') {
						cur++;
						heir=15;
						break;
						}
					heir=18;
					opeq(AND);
					break;
		case 8:		charac();
					break;
		case 9:		heir=23;
					opeq(MUL);
					break;
		case 10:	if (*cur == '+') {
						cur++;
						heir=24;
						bvalue=PREI;
						break;
						}
					heir=22;
					opeq(ADD);
					break;
		case 11:	if (nestto[nested] == cur-1) {
						cur=nestfrom[--nested];
						whitesp();
						return 1;
						}
					break;
		case 12:	if (*cur == '-') {
						cur++;
						heir=24;
						bvalue=PRED;
						break;
						}
					if (*cur == '>') {
						cur++;
						heir=25;
						break;
						}
					heir=22;
					opeq(SUB);
					break;
		case 13:	if (ltype[*cur] == DIGIT) number();
					else heir=25;
					break;
		case 14:	heir=23;
					opeq(DIV);
					break;
		case 15:	if (*cur == '<') {
						cur++;
						heir=21;
						opeq(SHL);
						break;
						}
					if (*cur == '=') {
						cur++;
						bvalue=LE;
						}
					else bvalue=LT;
					heir=20;
					break;
		case 16:	if (*cur == '=') {
						cur++;
						heir=19;
						bvalue=EQ;
						break;
						}
					heir=12;
					bvalue=ASGN;
					break;
		case 17:	if (*cur == '>') {
						cur++;
						heir=21;
						opeq(SHR);
						break;
						}
					if (*cur == '=') {
						cur++;
						bvalue=GE;
						}
					else bvalue=GT;
					heir=20;
					break;
		case 18:	heir=17;
					opeq(XOR);
					break;
		case 19:	if (*cur == '|') {
						cur++;
						heir=14;
						}
					else heir=16;
					opeq(OR);
					break;
		case 20:	if (incnext == 0) {
						cur--;	/* forever CONTZ */
						break;
						}
					incnext--;			/* end of this include	*/
					close(incfile[incnext]);
					cline=lastline[incnext];
					cur=nestfrom[--nested];
					return 1;
		case 21:	cur--;
					dolf(0);
					return 1;
		default: ;
		}
	if (eopt) {
		obnum(heir);oc(' ');oc(curch);oc(' ');
		obnum(bvalue);oc(' ');ohw(nameat);oc(' ');ohw(cur);
		oc(' ');obnum(nested);ocrlf();
		}
	whitesp();
	return 0;
	}

/*	if name not found, set nameat to structure with type OPERAND,
	nstor of SUNSTOR and nchain to zero. set heir to UNDEF. */
/*	if name found, set heir to type and nameat to address of type. */
/*	if reserved, set bvalue to rvalue. */


find() {
	cur--;
	i=hashno=0;
	while ((ltype[*cur] <= DIGIT) && i < 31) {
		hashno+=*cur;
		string[i++]=*cur++;
		}
	hashno+=i;
	if (i == 31) while (ltype[*cur] <= DIGIT) cur++;
	string[i]=0;
	if (eopt) {
		os(string);
		oc(' ');
		}
	hashno&=31;
	search();
	}

search() {

	bptr=hash[hashno];
	while (bptr) {
		j=-1;
		wp=bptr;
		bptr+=2;
		while (*bptr++ == string[++j]);
		if (j > i) {
			nameat=wp;
			nameat+=i+3;
			heir=*nameat;
			if (heir == RESERVED) bvalue=nameat->rvalue;
			return;
			}
		bptr=*wp;
		}
	heir=UNDEF;
	}


newname() {
	wp=mfree;
	*wp=hash[hashno];
	hash[hashno]=wp;
	j=0;
	nameat=wp+1;
	if (in_stru) string[0]|=0x80;	/* bit on for member and stag */
	do
		*nameat++=string[j++];
	while (j <= i);
	heir=UNDEF;
	nameat->opcl=OPERAND;
	nameat->nchain=0;
	nameat->nlen=i+1;
	nameat->ntype[0]=CINT;
	}

opeq(val)
	int  val; {
	if (*cur == '=') {
		cur++;
		heir=12;
		bvalue=val+AADD-ADD;
		}
	else bvalue=val;
	}

addnest() {
	unsigned argfrom[MAXNEST],argto[MAXNEST];
	char *defat,*xdefat;
	char narg;

	defat=nameat;
	narg=0;

/*	dargs of 255 means none. 0 means need empty parens. more is count.	*/
	if (defat->dargs != 255) {
		tokit();
		if (curch != '(') {
			error("missing arguments");
			return;
			}
		if (*cur == ')') tokit();	/* empty arg case	*/
		else {
			addparm=cur;
			do {
				if (++narg == MAXNEST) {
					error("wrong number of arguments");
					addparm=0;
					return;
					}
re_scan:		argfrom[narg]=cur;
				while (*cur != ',' && *cur != ')') {
					if (*cur == LF) {
						if (cur > &incbuf[MAXINC][0]) {
							dolf(0);
							goto re_scan;
							}
						error("parameters cannot span lines");
						return;
						}
					if (*cur == '(') skipp();
					if (*cur == '"') while (*++cur != '"');
					if (*cur == '\'') while (*++cur != '\'');
					cur++;
					}
				argto[narg]=cur;
				tokit();
				}
			while (curch == ',');
			addparm=0;
			}
		if (narg != defat->dargs) {
			error("wrong number of arguments");
			return;
			}
		}
	xdefat=defat=&defat->dval;
	nestfrom[nested]=cur;
	i=0;
	while (*xdefat != DEFEND) {
		i++;
		if (*xdefat == DEFSTR) while (*++xdefat != LF) ;
		xdefat++;
		}
	if (nested+i >= MAXNEST) {
		error("define too deep");
		return;
		}
	nested+=i;
	i=0;
	while (*defat != DEFEND) {
		if (*defat == DEFSTR) {
			nestfrom[nested-i]=++defat;
			nestto[nested-i]=0;
			while (*defat++ != LF) ;
			}
		else {
			nestfrom[nested-i]=argfrom[*defat];
			nestto[nested-i]=argto[*defat++];
			}
		i++;
		}
	cur=nestfrom[nested];
	}
	
/*	skip a parenthesized	expression.	*/

skipp() {
	while (*++cur != ')') 
		if (*cur == '(') skipp();
	}


number() {
	char ch,n;
	heir=CONSTANT;
	dvalue=0;
	if (curch == '0' && *cur != 'l' && *cur != 'L' && *cur != '.') {
		if (*cur == 'X' || *cur == 'x') {
			n = 0;
			while (1) {
				ch=*++cur;
				if (ch >='a') ch-=32;
				if (ch >='A' && ch <= 'F') ch-='7';
				else if (ltype[ch] == DIGIT) ch-='0';
				else break;
				dvalue=(dvalue<<4)+ch;
				if (n++ == 7) *((long *)(&fvalue)+1) = dvalue;
				}
			if (n > 8 && n != 16) error("illegal double constant");
			else if (n == 16) {
				*((long *)(&fvalue)) = dvalue;
				heir = FCONSTANT;
				return;
				}
			}
		else {
			while ((ch=*cur-'0') < 10) {
				cur++;
				dvalue=(dvalue<<3)+ch;
				}
			}
		if (dvalue > 65535 || dvalue < 0) heir=LCONSTANT;
		}
	else {
		if (*tokat == '.') cur=tokat;
		dvalue=curch-'0';
		while ((ch=*cur-'0') <= 9) {
			dvalue=dvalue*10+ch;
			cur++;
			}
		if (*cur == '.' || *cur == 'e' || *cur == 'E') {
			cur=tokat+_finput(tokat,&fvalue,100);
			heir=FCONSTANT;
			return;
			}
		if (dvalue > 32767) heir=LCONSTANT;
		}
	if (*cur == 'l' || *cur == 'L') {
		heir=LCONSTANT;
		cur++;
		}
	wvalue=dvalue;
	}

isstring() {
	i=0;
	heir=STRNG;
	while (*cur != '"' && i < 200) {
		if (*cur == '\n' || *cur == CONTZ) {
			error("unmatched \"");
			cur--;
			break;
			}
		string[i++]=getach();
		}
	if (i == 200) { error("string too long"); return; }
	cur++;
	string[i]=0;
	string[i+1]=0xff;
	}

charac() {
	heir=CONSTANT;
	if (*cur == '\'') {
		wvalue=0;
		}
	else {
		wvalue=getach();
		if (*cur != '\'' && *cur != LF) {
			wvalue<<=8;
			wvalue+=getach();
			}
		}
	if (*cur++ != '\'') error("missing '");
	}

getach() {
	char ch,och,max;
getagain:
	ch=*cur++;
	if (ch == '\\') {
		och=ch=*cur++;
		if (ch >= 'a') ch-=32;
		switch (ch) {
			case '\r':	cur++;
			case '\n':	if (addparm == 0) cline++;
						goto getagain;
			case 'N':	ch=LF;
						break;
			case 'T':	ch=9;
						break;
			case 'B':	ch=8;
						break;
			case 'R':	ch=13;
						break;
			case 'F':	ch=12;
						break;
			default:	if (ch >= '0' && ch <= '9') {
							max=2;
							ch-='0';
							while (*cur >= '0' && *cur <= '9' && max--)
								ch=(ch<<3)+*cur++-'0';
							}
						else ch=och;
			}
		}
	return ch;
	}

whitesp() {
	char ch;
	while (1) {
		if (ltype[*cur] == SPACE)
			cur++;
		else if (*cur == '/' && (cur+1)->byte == '*') {
			cur+=2;
			while(1) {
				ch=*cur;
				while (ch != LF && ch != '*' && ch != 26)
					ch=*++cur;
				if (ch == LF) dolf(1);
				else if (ch == 26) {
					cur--;
					error("EOF within comment");
					return;
					}
				else if (*++cur == '/') {
					cur++;
					break;
					}
				}
			}
		else return;
		}
	}
