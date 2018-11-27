// ============================================================================
//        __
//   \\__/ o\    (C) 2018  Robert Finch, Waterloo
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
//	prng_driver.c
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
#include <fmtk/const.h>
#include <fmtk/device.h>

extern int PeekRand(register int handle);
extern int GetRand(register int handle);
extern void SeedRand(register int handle, register int pos);
extern void DBGDisplayString(char *);

static void prng_init()
{
	InitPRNG();
}

static int prng_stat(int handle)
{
	return (0);
}

static int prng_peek(int handle)
{
	return (PeekRand(handle));
}

static int prng_get(int handle)
{
	return (GetRand(handle));
}

static void prng_put(int handle, int val)
{
}

static void prng_SetPosition(int handle, int pos)
{
	SeedRand(handle,pos);	
}

static void prng_flushi(int handle)
{
}

int prng_CmdProc(int cmd, int cmdParm1, int cmdParm2, int cmdParm3, int cmdParm4)
{
	int val;
	int err = E_Ok;

	switch(cmd) {
	case DVC_GetUnit:
		val = prng_get(cmdParm1);
		*(int *)cmdParm2 = val;
		break;
	case DVC_PutUnit:
		prng_put(cmdParm1, cmdParm2);
		break;
	case DVC_PeekUnit:
		val = prng_peek(cmdParm1);
		*(int *)cmdParm2 = val;
		break;
	case DVC_Open:
		prng_put(cmdParm1);
		if (cmdParm1)
			*(int *)cmdParm1 = 0;
		else
			err = E_Arg;
		break;
	case DVC_Close:
		prng_flushi();
		break;
	case DVC_Status:
		*(int *)cmdParm2 = prng_stat(cmdParm1);
		break;
	case DVC_Nop:
		break;
	case DVC_Setup:
		DBGDisplayStringCRLF("PRNG setup");
		break;
	case DVC_Initialize:
		prng_init();
		break;
	case DVC_SetPosition:
		prng_SetPosition(cmdParm1,cmdParm2);
		break;
	case DVC_FlushInput:
		prng_flushi(cmdParm1);
		break;
	case DVC_IsRemoveable:
		*(int *)cmdParm1 = 0;
		break;
	default:
		return err = E_BadDevOp;
	}
	return (err);
}
