// ============================================================================
//        __
//   \\__/ o\    (C) 2012-2017  Robert Finch, Waterloo
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
// C64 - 'C' derived language compiler
//  - 64 bit CPU
//
// This source file is free software: you can redistribute it and/or modify 
// it under the terms of the GNU Lesser General Public License as published 
// by the Free Software Foundation, either version 3 of the License, or     
// (at your option) any later version.                                      
//                                                                          
// This source file is distributed in the hope that it will be useful,      
// but WITHOUT ANY WARRANTY; without even the implied warranty of           
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            
// GNU General Public License for more details.                             
//                                                                          
// You should have received a copy of the GNU General Public License        
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    
//                                                                          
// ============================================================================
//
#include "stdafx.h"

/*
 *	68000 C compiler
 *
 *	Copyright 1984, 1985, 1986 Matthew Brandt.
 *  all commercial rights reserved.
 *
 *	This compiler is intended as an instructive tool for personal use. Any
 *	use for profit without the written consent of the author is prohibited.
 *
 *	This compiler may be distributed freely for non-commercial use as long
 *	as this notice stays intact. Please forward any enhancements or questions
 *	to:
 *
 *		Matthew Brandt
 *		Box 920337
 *		Norcross, Ga 30092
 */
/*******************************************************
	Modified to support Raptor64 'C64' language
	by Robert Finch
	robfinch@opencores.org
*******************************************************/
extern char *errtext(int);

int inComment = FALSE;
int      my_errno[80];
int      numerrs;
char     inpline[520];
int             total_errors = 0;
extern char     *lptr;          /* shared with preproc */
extern std::ifstream *inclfile[10];  /* shared with preproc */
extern int      inclline[10];   /* shared with preproc */
extern int      incldepth;      /* shared with preproc */
char            *linstack[20];  /* stack for substitutions */
char            chstack[20];    /* place to save lastch */
int             lstackptr = 0;  /* substitution stack pointer */
static char numstr[100];
static char *numstrptr;
static char backup_token = 0;

int isalnum(char c)
{       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9');
}

int isidch(char c) { return isalnum(c) || c == '_' || c == '$'; }
int my_isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c=='\r'; }
int isdigit(char c) { return (c >= '0' && c <= '9'); }

void initsym()
{
	lptr = inpline;
  inpline[0] = 0;
  numerrs = 0;
  total_errors = 0;
  lineno = 0;
}

int getline(int listflag)
{
	int rv;

	if (lineno > 0 && mixedSource && firstLineOfFunc) {
		if (inpline && strlen(inpline)>2) {
		gd->lptr = my_strdup(inpline);
		firstLineOfFunc = false;
		}
	}
    if(lineno > 0 && listflag) {
        lfs.printf("%6d\t%s",lineno,inpline);
		while(numerrs) {
			numerrs--;
			if (numerrs < 80)
				lfs.printf(" *** error %d: %s\n",my_errno[numerrs],errtext(my_errno[numerrs]));
		}
        numerrs = 0;
    }
    ++lineno;
	memset(inpline, 0, sizeof(inpline));
    ifs->getline(inpline,512);
	strcat_s(inpline,sizeof(inpline),"\n");
	rv = ifs->gcount()==0;
	//printf("line:%.60s\n", inpline);
    if( rv && incldepth > 0 ) {
        ifs->close();
        ifs = inclfile[--incldepth];
        lineno = inclline[incldepth];
        return getline(0);
    }
    if( rv )
        return 1;
    lptr = inpline;
    if(inpline[0] == '#' && !inComment)
        return preprocess();
    return 0;
}

/*
 *      getch - basic get character routine.
 */
int getch()
{

	while( (lastch = *lptr++) == '\0') {
        if( lstackptr > 0 ) {
            lptr = linstack[--lstackptr];
            lastch = chstack[lstackptr] & 0x7F;
            goto j1;
        }
        if(getline(incldepth == 0))
            return lastch = -1;
    }
j1:
	lastch &= 0x7F;
    if (numstrptr >= &numstr[0] && numstrptr < &numstr[98]) {
       *numstrptr = lastch;
       numstrptr++;
    }
    return lastch;
}
 
//
// getid - get an identifier.
//
// identifiers are any isidch conglomerate
// that doesn't start with a numeric character.
// this set INCLUDES keywords.
//
void getid()
{
	int    i;
    i = 0;
    lastid[0] = '_';
    while(isidch(lastch)) {
		if(i < 120) {
			lastkw[i] = lastch;
			i++;
			lastid[i] = lastch;
        }
		getch();
    }
    lastkw[i] = '\0';
    lastid[i+1] = '\0';
    lastst = id;
}
 
/*
 *      getsch - get a character in a quoted string.
 *
 *      this routine handles all of the escape mechanisms
 *      for characters in strings and character constants.
 */
int     getsch()        /* return an in-quote character */
{       register int    i, j;
        if(lastch == '\n')
                return -1;
        if(lastch != '\\' || !parseEsc) {
                i = lastch;
                getch();
                return i;
                }
        getch();        /* get an escaped character */
        if(isdigit(lastch)) {
                i = 0;
                for(j = i = 0;j < 3;++j) {
                        if(lastch <= '7' && lastch >= '0')
                                i = (i << 3) + lastch - '0';
                        else
                                break;
                        getch();
                        }
                return i;
                }
        i = lastch;
        getch();
        switch(i) {
                case '\n':
                        getch();
                        return getsch();
                case 'b':
                        return '\b';
                case 'f':
                        return '\f';
                case 'n':
                        return '\n';
                case 'r':
                        return '\r';
				case 't':
						return '\t';
                default:
                        return i;
                }
}

void getFilename()
{
	int i;
	char j;

    for(i = 0;i < MAX_STRLEN;++i) {
            if(lastch == '>')
                    break;
            if((j = getsch()) == -1)
                    break;
            else
                    laststr[i] = j;
            }
    laststr[i] = 0;
    lastst = sconst;
    if(lastch != '>')
            error(ERR_SYNTAX);
    else
            getch();
}

int64_t radix36(char c)
{
	if(isdigit(c))
            return c - '0';
    if(c >= 'a' && c <= 'z')
            return c - 'a' + 10;
    if(c >= 'A' && c <= 'Z')
            return c - 'A' + 10;
    return -1;
}
 
/*
 *      getbase - get an integer in any base.
 */
void getbase(int64_t b)
{       
        register int64_t i0, i1, i2;
        register int64_t i, j;
        int k;

        i = 0;
        i0 = 0;
        i1 = 0;
        while(isalnum(lastch)) {
                if((j = radix36(lastch)) < b) {
                        i = i * b + j;
                        i2 = i0;
                        for (k = 0; k < b; k++) {
                            i0 = i0 + i2;
                            if (i0 & 0x1000000000000000LL) {
                               i0 = i0 - 0x1000000000000000LL;
                               i1 = i1 + 1;
                            }
                        }
                        i0 = i0 + j;
                        if (i0 & 0x1000000000000000LL) {
                            i0 = i0 - 0x1000000000000000LL;
                            i1 = i1 + 1;
                        }
                        getch();
                        }
                else break;
                }
		if (lastch=='L' || lastch=='U')	// ignore a 'L'ong suffix and 'U'nsigned
			getch();
        ival = i;
/*
        rval.exp = 0x804E;
        rval.man1 = (i1 >> 32) & 0xffffL;
        rval.man2 = (i1 >> 16) & 0xffffL;
        rval.man3 = i1 & 0xffffL;
        rval.man4 = (i0 >> 16) & 0ffffL;
        rval.man5 = i0 & 0xffffL;
        // normalize the number
        while (!(rval.man1 & 0x8000)) {
             rval.exp--;
             rval.man1 = (rval.man1 << 1) | (rval.man2 & 0x8000) ? 1 : 0;
             rval.man2 = (rval.man2 << 1) | (rval.man3 & 0x8000) ? 1 : 0;
             rval.man3 = (rval.man3 << 1) | (rval.man4 & 0x8000) ? 1 : 0;
             rval.man4 = (rval.man4 << 1) | (rval.man5 & 0x8000) ? 1 : 0;
             rval.man5 = (rval.man5 << 1);
        }
*/
        lastst = iconst;
}
 
//
//      getfrac - get fraction part of a floating number.
//
void getfrac()
{
	double frmul;
	double fract = 0.0;
    frmul = 1.0;
    while(isdigit(lastch)) {
        fract = fract * 10.0 + (lastch - '0');
        getch();
        frmul *= 10.0;
    }
	fract = fract / frmul;
	rval += fract;
}
 
/*
 *      getexp - get exponent part of floating number.
 *
 *      this algorithm is primative but usefull.  Floating
 *      exponents are limited to +/-255 but most hardware
 *      won't support more anyway.
 */
void getexp()
{       
		double  expo, exmul;
		
        expo = 1.0;
        if(lastst != rconst)
                rval = (double)ival;
        if(lastch == '-') {
                exmul = 0.1;
                getch();
                }
		else if (lastch=='+') {
			getch();
                exmul = 10.0;
		}
        else {
                exmul = 10.0;
		}
        getbase(10);
        if(ival > 32767)
                error(ERR_FPCON);
        else
                while(ival--) {
                        expo *= exmul;
				}
        rval *= expo;
}
 
/*
 *      getnum - get a number from input.
 *
 *      getnum handles all of the numeric input. it accepts
 *      decimal, octal, hexidecimal, and floating point numbers.
 */
void getnum()
{       register int    i;
        i = 0;
        
        ival = 0;
        rval = 0.0;
        numstrptr = &numstr[0];
         *numstrptr = lastch;
         numstrptr++; 
        if(lastch == '0') {
                getch();
                if (lastch=='.')
                     goto j1;
                if(lastch == 'x' || lastch == 'X') {
                        getch();
                        getbase(16);
                        }
                else getbase(8);
                }
        else    {
                getbase(10);
j1:
                if(lastch == '.') {
                        getch();
                        rval = (double)ival;    /* float the integer part */
                        getfrac();      /* add the fractional part */
                        lastst = rconst;
                        }
                if(lastch == 'e' || lastch == 'E') {
                        getch();
                        getexp();       /* get the exponent */
						// This must be reset because getting the exponent
						// calls getbase() which will set lastst=iconst
						lastst = rconst;
                        }
				
				if (lastst==rconst && (lastch=='Q' || lastch=='q' ||
					lastch=='D' || lastch=='d' || lastch=='s' || lastch=='S' || lastch=='T' || lastch=='t')) {
						float_precision = tolower(lastch);
						getch();
				}
				else
					float_precision = 't';
				
				// Ignore 'U' unsigned suffix
				if (lastch=='U' || lastch=='u') {
					getch();
				}
				}
    numstrptr[-1]='\0';
    numstrptr = NULL;
//    dd_real::read(numstr,rval);
//    printf("leave getnum=%s\n", numstr);
				
}

void SkipSpaces()
{
    while( my_isspace(lastch) ) {
        getch(); 
    }
}
/*
 *      NextToken - get next symbol from input stream.
 *
 *      NextToken is the basic lexical analyzer.  It builds
 *      basic tokens out of the characters on the input
 *      stream and sets the following global variables:
 *
 *      lastch:         A look behind buffer.
 *      lastst:         type of last symbol read.
 *      laststr:        last string constant read.
 *      lastid:         last identifier read.
 *      ival:           last integer constant read.
 *      rval:           last real constant read.
 *
 *      NextToken should be called for all your input needs...
 */
void NextToken()
{
  int i, j;
  SYM *sp;
  int tch;
restart:        /* we come back here after comments */
  if (backup_token) {
     backup_token = 0;
     lastch = '(';
     lastst = openpa;
     return;
  }
	SkipSpaces();
  if( lastch == -1) {
    lastst = my_eof;
    dfs.printf("Returning EOF from NextToken.\n");
  }
  else if(isdigit(lastch)) {
    getnum();
  }
  else if(isidch(lastch)) {
    getid();

		if( (sp = defsyms.Find(lastid,false)) != NULL) {
			tch = lastch;
			if (!(lastch==')' && sp->value.s[0]=='(')) {
				if (lstackptr < 19) {
					linstack[lstackptr] = lptr;
					chstack[lstackptr++] = tch;
					lptr = sp->value.s;
  			}
				getch();
				goto restart;
			}
		}

  }
  else {
  switch(lastch) {
  case '+':
    getch();
    if(lastch == '+') {
            getch();
            lastst = autoinc;
            }
    else if(lastch == '=') {
            getch();
            lastst = asplus;
            }
    else lastst = plus;
    break;
                case '-':
                        getch();
                        if(lastch == '-') {
                                getch();
                                lastst = autodec;
                                }
                        else if(lastch == '=') {
                                getch();
                                lastst = asminus;
                                }
                        else if(lastch == '>') {
                                getch();
                                lastst = pointsto;
                                }
                        else  
							lastst = minus;
                        break;
                case '*':
                        getch();
                        if(lastch == '=') {
                                getch();
                                lastst = astimes;
                                }
                        else lastst = star;
                        break;
                case '/':
                        getch();
                        if(lastch == '=') {
                          getch();
                          lastst = asdivide;
                        }
                        else if(lastch == '*') {
                          inComment = TRUE;
                          getch();
                                for(;;) {
                                        if(lastch == '*') {
                                                getch();
                                                if(lastch == '/') {
                                                        getch();
														inComment = FALSE;
                                                        goto restart;
                                                        }
                                                }
                                        else
                                                getch();
                                        }
                                }
						else if (lastch == '/') {

							inComment = TRUE;
							for(;;) {
								getch();
								if (lastch=='\n' || lastch==-1) {
									//getch();
									inComment = FALSE;
									goto restart;
								}
							}

						}
                        else lastst = divide;
                        break;
                case '^':
                        getch();
                        if( lastch == '=') {
							lastst = asxor;
							getch();
                        }
						else
							lastst = uparrow;
                        break;
                case ';':
                        getch();
                        lastst = semicolon;
                        break;
                case ':':
                        getch();
						if (lastch==':') {
							lastst = double_colon;
							getch();
						}
						else
							lastst = colon;
                        break;
                case '=':
                        getch();
                        if(lastch == '=') {
                                getch();
                                lastst = eq;
                                }
                        else lastst = assign;
                        break;
                case '>':
                        getch();
                        if(lastch == '=') {
                            getch();
                            lastst = geq;
                        }
                        else if(lastch == '>') {
                            getch();
                            if(lastch == '=') {
                                getch();
                                lastst = asrshift;
                            }
                            else lastst = rshift;
                        }
                        else lastst = gt;
                        break;
                case '<':
                        getch();
                        if(lastch == '=') {
                                getch();
                                lastst = leq;
                                }
                        else if(lastch == '<') {
                                getch();
                                if(lastch == '=') {
                                        getch();
                                        lastst = aslshift;
                                        }
                                else lastst = lshift;
                                }
						else if (lastch == '>') {
							getch();
							lastst = neq;
						}
                        else lastst = lt;
                        break;
                case '\'':
                        getch();
                        ival = getsch();        /* get a string char */
                        if(lastch != '\'')
                                error(ERR_SYNTAX);
                        else
                                getch();
                        lastst = cconst;
                        break;
                case '\"':
                        getch();
                        for(i = 0;i < MAX_STRLEN;++i) {
                                if(lastch == '\"')
                                        break;
                                if((j = getsch()) == -1)
                                        break;
                                else
                                        laststr[i] = j;
                                }
                        laststr[i] = 0;
                        lastst = sconst;
                        if(lastch != '\"')
                                error(ERR_SYNTAX);
                        else
                                getch();
                        break;
                case '!':
                        getch();
                        if(lastch == '=') {
                                getch();
                                lastst = neq;
                                }
                        else lastst = nott;
                        break;
                case '%':
                        getch();
                        if(lastch == '=') {
                                getch();
                                lastst = asmodop;
                                }
                        else lastst = modop;
                        break;
                case '~':
                        getch();
                        lastst = cmpl;
                        break;
                case '.':
                        getch();
                        lastst = dot;
						if (lastch=='.') {
							getch();
							if (lastch=='.') {
								getch();
								lastst = ellipsis;
								strcpy_s(lastid, sizeof(lastid), "...");
							}
						}
                        break;
                case ',':
                        getch();
                        lastst = comma;
                        break;
                case '&':
                        getch();
                        if( lastch == '&') {
                                lastst = land;
                                getch();
                                }
                        else if( lastch == '=') {
                                lastst = asand;
                                getch();
                                }
                        else
                                lastst = bitandd;
                        break;
                case '|':
                        getch();
                        if(lastch == '|') {
                                lastst = lor;
                                getch();
                                }
                        else if( lastch == '=') {
                                lastst = asor;
                                getch();
                                }
                        else
                                lastst = bitorr;
                        break;
                case '(':
                        getch();
                        lastst = openpa;
                        break;
                case ')':
                        getch();
                        lastst = closepa;
                        break;
                case '[':
                        getch();
                        lastst = openbr;
                        break;
                case ']':
                        getch();
                        lastst = closebr;
                        break;
                case '{':
                        getch();
                        lastst = begin;
                        break;
                case '}':
                        getch();
                        lastst = end;
                        break;
                case '?':
                        getch();
                        lastst = hook;
                        break;
                case '\\':
                        getch();
                        goto restart;
                default:
                        getch();
                        error(ERR_ILLCHAR);
                        goto restart;   /* get a real token */
                }
              }
  if(lastst == id)
    IdentifyKeyword();
//	printf("token: %d",lastst);
//	if (lastst==id)
//		printf("lastid=%s| ", lastid);
}

void needpunc(enum e_sym p,int clue)
{
	if( lastst == p)
        NextToken();
	else {
		//printf("%d %s\n", lineno, inpline);
		printf("*************clue:%d************\n",clue);
        error(ERR_PUNCT);
	}
}

void backup() {
    backup_token = 1;
}
