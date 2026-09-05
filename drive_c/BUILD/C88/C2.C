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
/*	 C88 COMPILER			C2.C	*/

#include "PASS1.H"
#include "NODES.H"
#if CHECK
#include "OBJ.H"
#endif

dolf(skipit)
	char skipit; {
	if (cur > &incbuf[MAXINC][0]) {
		cur=nestfrom[--nested];
		whitesp();
		}
	else {
		if ((nestto[nested]-cur) < 128) refill();
		else cur++;
		if (addparm == 0) cline++;
		newline=1;			/* generate STMT node	*/
		last=cur;
		if (skipit == 0) {
			while (ltype[*cur] == SPACE) cur++;
			if (*cur == '#') preproc();
			whitesp();
			}
		}
	}


preproc() {
	char want;
	cur++;
	whitesp();
	tokit();
	if (heir != RESERVED && heir != UNDEF && heir != OPERAND)
		error_l("bad control");

	else if (match("define"))
		adddef();

	else if (match("include"))
		doinc();

	else if (match("if")) {
		tokit();
		if (heir == CONSTANT || curch == '(' || curch == '-' || curch == '!') {
			wvalue=constexp();
			}

		else {
			error_l("need constant after #if");
			return;
			}
		if (wvalue == 0) skipsome();
		}

	else if (match("else"))
		skipsome();

	else if (match("endif"))
		skipl();

	else if (match("ifdef") || match("ifndef")) {
		want=match("ifdef");
		if (ltype[*cur++] != LETTER)
			error_l("not an identifier");
		else {
			find();
			if ((heir == DEFINED && want) ||
				(heir != DEFINED && want == 0)) {
				skipl();
				return;
				}
			skipsome();
			}
		}

	else if (match("line")) {
		tokit();
		if (heir != CONSTANT)
			error_l("line must be constant");
		else {
			xline=wvalue;
			skipl();
			}
		}

	else if (match("undef")) {
		if (ltype[*cur++] != LETTER)
			error_l("not an identifier");
		else {
			find();
			if (heir != DEFINED) error_l("not defined");
			else (nameat-2)->byte=' ';
			whitesp();
			}
		}
	else if (match("asm")) {
		cur=tokat;		/* backup so statement will find ASM	*/
		}
	else error_l("bad control");
	}


skipsome() {
	char ifnest;
	ifnest=1;
	while (ifnest) {
		skipl();
		dolf(1);
		while (ltype[*cur] == SPACE) cur++;
		if (*cur == CONTZ) return;
		if (*cur == '#' && *(cur+1) != '\r' && *(cur+1) != '\n') {
			tokit();
			tokit();
			if (heir == RESERVED || heir == UNDEF || heir == OPERAND) {
				if (match("if") || match("ifdef") || match("ifndef"))
					ifnest++;
				else if (match("endif")) ifnest--;
				else if (match("else") && ifnest == 1)
					ifnest=0;
				}
			}
		}
	skipl();
	}

skipl() {
	while (*cur != LF && *cur != CONTZ) cur++;
	}

match(mat)
	char *mat; {
	char *str;

	str=string;
	do
		if (*str != *mat++) return 0;
	while (*str++);
	return 1;
	}

adddef() {
	char *newdef,*defat,*dto,i,level,lastdef,numqt,numsqt;

	numqt=numsqt=0;

/*	do not do normal tokit as may be redefining a define	*/
	tokat=cur;
	curch=*cur++;
	if (ltype[curch] == LETTER) {
		find();
		whitesp();
		if (heir == DEFINED) {		/* kill old define	*/
			*(nameat-2)='0';		/* with zero as first letter */
			search();
			}
		}
	else {
		cur--;
		tokit();
		}
	if (heir != UNDEF) {error_l("duplicate define"); return;}
	newname();
	defat=nameat;
	defat->defcl=DEFINED;
	defat->dargs=255;
	newdef=&defat->dval;
	if (*cur == '(' && *(cur-1) != ' ' && *(cur-1) != '\11') {
		defat->dargs=0;
		tokit();
		if (*cur == ')') {
			tokit();
			goto no_args;
			}
		if (maxmem-mfree < 1200) {
			error("out of memory");
			real_exit(2);
			}
		mfree+=1000;
		tokit();
		do {
			if (heir && heir > 4 && heir != OPERAND && heir != UNDEF) {
				error_l("illegal define");
				skipl();
				goto cleanup;
				}
			newname();
			nameat->defcl=DEFPARM;
			nameat->dpnum=++defat->dargs;
			mfree=nameat+2;
			tokit();
			}
		while (ifch(','));
		if (curch != ')') {
			error_l("illegal define");
			skipl();
			goto cleanup;
			}
		}

no_args:
	level=nested;
	lastdef=DEFPARM;
	while (1) {
		if (*cur == '\\' && *(cur+1) == LF) {
			cur++;
			dolf(1);
			continue;
			}

		if (*cur == '\\' && *(cur+2) == LF) {
			cur+=2;
			dolf(1);
			continue;
			}

		do {
			addparm=0;
			if (*cur == CONTZ || (*cur == LF && level == nested)) goto cleanup;
			/* need to set addparm to keep token */
			addparm=cur;
			}
		while(tokone());
		addparm=0;
		if (tokat == 0) goto cleanup;	/* error in tokone	*/

		if (heir == DEFPARM) {
			*newdef++=nameat->dpnum;
			lastdef=DEFPARM;
			}
		else {
			dto=cur;
			if (lastdef == DEFSTR && tokat < dto) newdef--;
			else *newdef++=DEFSTR;
			lastdef=DEFSTR;
			while (tokat < dto) {
				cur=tokat;
				if (numqt+numsqt == 0) whitesp();
				if (cur != tokat) {
					*newdef++=' ';
					tokat=dto=cur;
					}
				else {
					if (*tokat == '"') numqt=!numqt;
					if (*tokat == '\'') numsqt=!numsqt;
					*newdef++=*tokat++;
					}
				}
			cur=dto;
			*newdef++=LF;
			}
		}
cleanup:
	*newdef=DEFEND;
	mfree=newdef+1;
	for (i=0; i < 32; i++)
		while (hash[i] > mfree) hash[i]=hash[i]->word;
	}



/*	add_define  --  add a 'n' option from command tail- a define.	*/

add_define() {
	char *defat,*newdef;

	tokit();			/*	name of variable	*/
	newname();
	defat=nameat;
	defat->defcl=DEFINED;
	defat->dargs=255;
	newdef=&defat->dval;
	*newdef++=DEFSTR;
	if (*cur == 0) {		/* if no value, assume 1	*/
		*newdef++='1';
		}
	else if (*cur == '=') while (*++cur) *newdef++=*cur;
	else {
		os("illegal N option ");
		real_exit(2);
		}

	*newdef++=LF;
	*newdef=DEFEND;
	mfree=newdef+1;
	}



doinc() {
	int  i;
	char inc_search;
	char inctemp[66];

	if (incnext == MAXINC) { error_l("include nesting too deep"); return;}
/*	accept <> around an include name	*/

	if (*cur == '<') inc_search=1;
	else if (*cur == '"') inc_search=0;
	else error_l("bad include");

	i=0;
	cur++;
	while (*cur != '"' && *cur != '>' && i < 200) {
		if (*cur == '\n' || *cur == CONTZ) {
			error_l("unmatched \"");
			return;
			}
		string[i++]=*cur++;
		}
	if (i == 200) { error_l("string too long"); return; }
	cur++;
	string[i]=0;
	if (nested == MAXNEST-1) {
		error_l("defines too deep");
		return;
		}
	skipl();
	nestfrom[nested++]=cur;

/*	supply a drive name of i option was specified	*/

	if (istring[0]) {
		if (string[1] != ':') strcpy(&string[100],string);
		else strcpy(&string[100],&string[2]);
		strcpy(string,istring);		/*	copy in -i prefix	*/
		strcat(string,&string[100]);/*	add name to end */
		}

/*	if MS-DOS V2.0 and use include= to find file */
	extern char _msdos2;
	if (inc_search && _msdos2) {
		findfile(string,inctemp);
		strcpy(string,inctemp);
		}

	if ((incfile[incnext]=open(string,0)) == -1) {
		os("cannot open ");
		os(string);
		ocrlf();
		real_exit(2);
		}
	lastline[incnext]=cline;
	cline=0;
	strcpy(&incname[incnext][0],string);
	cur=nestfrom[nested]=nestto[nested]=&incbuf[incnext][512];
	incnext++;				/* have another include */
	*cur=LF;
	dolf(0);
	}

refill() {
	int  i,forargs,readsize;
	char *newptr,*tptr;

	tptr=nestto[nested];
	if (tptr < (incnext ? &incbuf[incnext-1][512]: &inbuf[2048])) {
		if (*cur != CONTZ) cur++;
		return;
		}
	forargs=0;
	if (addparm) {		/* must keep parms for re-parse	*/
						/* if arguments, allow second chance to fit */
		if (nestto[nested] - cur >= 80) {
			cur++;
			return;
			}
		forargs=cur-addparm+1;
		cur=addparm-1;
		addparm=tokat=incnext ? &incbuf[incnext-1][0]:inbuf;
		}
	readsize=incnext ? 512: 2048;
	if (tptr-cur > 128) {
		if (readsize-tptr+cur > 128) readsize-=(((tptr-cur)>>7)<<7);
		else {
			error_l("line too long");
			real_exit(2);
			}
		}
	newptr=incnext ? &incbuf[incnext-1][0]: inbuf;
	cur++;
	while (cur < tptr) *newptr++=*cur++;
	cur=(incnext ? &incbuf[incnext-1][0]: inbuf)+forargs;
	if (see_exit && incnext == 0) i=see_call(0,newptr,readsize);
	else i=read((incnext ? incfile[incnext-1]: infile),newptr,readsize);
	nestto[nested]=tptr=newptr+i;
	*tptr=CONTZ;
	}


doinit() {
	char isstatic,*varis,*bptr;
	int  value;

	value=0;
	if (addat->nstor == STYPEDEF || original) {
		original=0;
		return 0;
		}
	isstatic=addat->nstor <= SEXTERN;
	if (isstatic) {
		ctlb(1);
		if (is_big) {
			if ((was_ext || addat->nstor == SEXTERN || addat->nstor == SEXTONLY) && 
				(addat->ntype[0] == CSTRUCT || addat->ntype[0] == ARRAY))
				ctlb(addat->nstor + 128);
			else ctlb(addat->nstor);
			}
		else ctlb(addat->nstor);
		ctlw(addat->noff);

/*	make the names of statics within functions distinct	*/
		if (addat->nstor == SSTATIC && funname) {
			bptr=funname;
			while (*bptr) ctlb(*bptr++);
			ctlb('_');
			}
		ctls(addat-addat->nlen);
		}
	varis=&addat->ntype[0];
	locplus=0;	/* local offset from locoff */
	if (ifch('=')) {
		if (*varis == CSTRUCT && curch != '{') {
			error("need '{' for STRUCT initilization");
			return 0;
			}
		if (addat->nstor == SEXTONLY) {
			error("cannot initilize EXTERN");
			return 0;
			}
		value=initsome(varis,1,isstatic,0);
		if (isstatic) ctlb(INITEND);
		}
	else {
		if (isstatic && addat->nstor != SEXTONLY) {
			if (addat->ntype[0] != FUNCTION) {
				ctlb(INITRB);
				ctlw(dsize(varis));
				}
			else ctlb(INITFUN);
			}
		}
	return value;
	}

initsome(varis,num,isstatic,initnode)
	char *varis;
	int  num,isstatic,initnode; {
	int  asize,nodeo[4],inode,lasttree,swant;
	char *memat,sis,at,sinit,itype;

	do {
		if (*varis == ARRAY) {
			asize=(varis+1)->word;
			if (asize < 0 && num < 0) {
				error("missing dimension");
				return 0;
				}
			if (ifch('{')) {
				/*	allow the improper braces if string init of char */
				if (heir == STRNG && (varis+3)->byte == CCHAR)
					initsome(varis,1,isstatic,initnode);
				else initnode=initsome(varis+3,asize,isstatic,initnode);
				notch('}');
				}
			else if (heir == STRNG && (varis+3)->byte == CCHAR) {
				if (isstatic == 0) {
					error("sorry, no string initilization of AUTO");
					return 0;
					}
				ctlb(INITSTR);
				sis=0;
				while(string[sis]) sis++;
				if (asize < 0) {
					(varis+1)->word=sis+1;
					swant=sis;
					}
				else swant=asize-1;
				if (sis > swant) sis=swant;
				if (swant > 255) {
					error("string too long");
					swant=255;
					}
				ctlb(swant);
				i=0;
				while (i < sis)
					ctlb(string[i++]);
				while (sis++ < swant)
					ctlb(0);
				tokit();
				}
			else {
				if (curch == ',' || curch == '}' || curch == ';') {
					if (num < 0) {
						bptr=varis-3;
						(bptr+1)->word=-1-num;
						}
					else {
						asize=dsize(varis)*num;
						if (isstatic) {
							ctlb(INITDB);
							ctlw(asize);
							}
						else locplus+=asize;
						}
					return initnode;
					}
				initnode=initsome(varis+3,asize,isstatic,initnode);
				}
			}
		else if (*varis == CSTRUCT) {
			memat=(varis+1)->word;
			memat=memat->schain;
			sinit=ifch('{');
			if ((curch == '}' || curch == ';') && num < 0) {
				bptr=varis-3;
				(bptr+1)->word=-1-num;
				return initnode;
				}
			while (memat) {
				initnode=initsome(&memat->ntype[0],1,isstatic,initnode);
				memat=memat->nchain;
				ifch(',');
				}
			if (sinit) notch('}');
			}

		else {
			if (curch == ',' || curch == '}' || curch == ';') {
				if (num < 0) {
					bptr=varis-3;
					while (*bptr != ARRAY)
						bptr--;
					(bptr+1)->word=-1-num;
					}
				else {
					asize=dsize(varis)*num;
					if (isstatic) {
						ctlb(INITDB);
						ctlw(asize);
						}
					else locplus+=asize;
					}
				return initnode;
				}
			if (ifch('{')) {
				initnode=initsome(varis,num,isstatic,initnode);
				notch('}');
				return initnode;
				}
			itype=*varis;
			if (itype == PTRTO && !is_big) itype=CUNSG;
			ininit=1;	/* needed so new variable is not allowed in init list*/
			if (isstatic) {
				lasttree=ntree;	/*	re-use space for each expression */
				inode=exprnc();
				ctlb(INITVAL);
				ctlb(itype);
				ctlw(lasttree-1);/*	supply place to chop off tree	*/
				ctlw(ntree-1);
				ctlw(inode);
				ntree=lasttree;	/*	chop of the tree	*/
				}
			else {
				nodeo[0]=typeof(varis);
				nodeo[1]=0;
				nodeo[2]=locoff+locplus;
				locplus+=dsize(varis);
				nodeo[3]=0;
				nodeo[1]=tree4(nodeo);
				nodeo[0]=ASGN;
				nodeo[2]=exprnc();
				if (initnode) {
					nodeo[2]=tree3(nodeo);
					nodeo[1]=initnode;
					nodeo[0]=LST+2;
					}
				initnode=tree3(nodeo);
				}
			ininit=0;
			}
		if (num != 1) ifch(',');
		}
	while (--num);
	return initnode;
	}



/*	FINDFILE.C	*/
/*
	This file contains the routine to locate a file, utilizing the INCLUDE
	environment variable for the directories to search.

	Interface:

		findfile(filename, target_buf)

		where filename is the name of the file to be searched for,
		      target_buf is the buffer to place the path name if found.

		if the file is found, findfile return 1 and the pathname,
		otherwise it returns 0.

	This program uses the environ routines to access the PATH variable.

	Stack requirements:  ~300 bytes

*/

findfile(filename, target_buf)
	char *filename, *target_buf; {
	int fid;
	char paths[256], *p_ptr, *t_ptr;

	/* first check in the local directory */
	strcpy(target_buf, filename);
	fid = open(target_buf, 0);
	if (fid >= 0)  {				/* got it */
		close(fid);
		return (1);
		}
	fid = environ("DSINC", paths, 256);
	if (fid == -1) fid = environ("INCLUDE", paths, 256);
	p_ptr = paths;

	while (*p_ptr != 0) {
		/* copy the directory name */
		t_ptr = target_buf;
		while (*p_ptr != ';' && *p_ptr != 0) {
			*t_ptr++ = *p_ptr++;
			}
		if (*(t_ptr-1) != '/' && *(t_ptr-1) != '\\') *t_ptr++ = '\\';
		*t_ptr = 0;
		if (*p_ptr) p_ptr++;		/* beyond the ';' */
		strcat(target_buf, filename);
		fid = open(target_buf, 0);
		if (fid >= 0)  {				/* got it */
			close(fid);
			return (1);
			}
		}
	strcpy(target_buf, filename);
	return (0);							/* can't find one */
	}



/*  ENVIRON.C	*/

/*
	This file contains the routine for searching the environment
	area for a given string.

	Interface:

		environ(search_str, buffer, buf_len)

		where search_str is the actual string being searched for,
			  buffer is the location to store the string if found,
			  buf_len is the maximum number of characters to store
			          into the buffer (includes terminator).

        -1 is returned if the environment variable isn't found,
        otherwise the length of the string copied is returned.
*/

extern unsigned _pcb;		/* c88 variable with the PSP segment */

int environ(search_str, buffer, buf_len)
	char *search_str, *buffer; int buf_len; {

#asm

	mov		ax, _pcb_;				; get the env segment into ES
	mov		es, ax					; 
	mov		ax, es:[2CH]
	mov		es,ax

	mov		bx, [6][bp]				; buffer
	mov		byte[bx],0
	mov		si, 0					; offset into the environment
	jmp		outer_begin

outer_loop:
	inc		si
outer_begin:
	mov		di, [4][bp]				; search_str
	cmp		byte es:[si],0
	jne		cmp_loop
									; not found
	mov		ax,-1
	pop		bp
	ret

cmp_loop:
	mov		al, byte es:[si]		; pick up the search character
	cmp		al,'='
	je		end_str
	cmp		al, byte[di]
	jne		outer_next
	inc		si
	inc		di
	jmp		cmp_loop

end_str:
	cmp		byte[di], 0
	jne		outer_next
									; got it, copy them bytes!
	mov		cx, [8][bp]				; buf_len
	inc		si						; beyond the '='

move_loop:
	mov		al, byte es:[si]
	or		al,al
	jz		now_return
	dec		cx
	jz		now_return
	mov		byte[bx],al
	inc		si
	inc		bx
	jmp		move_loop

now_return:
	mov		byte[bx],0
	mov		ax, [8][bp]
	sub		ax, cx
	pop		bp
	ret

outer_next:
	cmp		byte es:[si],0				; skip to the next entry
	je		outer_loop
	inc		si
	jmp		outer_next
#
}
