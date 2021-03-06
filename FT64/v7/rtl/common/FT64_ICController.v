// ============================================================================
//        __
//   \\__/ o\    (C) 2017-2019  Robert Finch, Waterloo
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
//	FT64_ICController.v
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
`include ".\FT64_config.vh"

module FT64_ICController(clk_i, asid, pc0, pc1, pc2, hit0, hit1, hit2, bstate,
	thread_en, ihitL2,
	L1_adr, L1_wr0, L1_wr1, L1_wr2, L1_en, L1_invline, icnxt, icwhich, ziccnt, ld_L1_dati);
parameter ABW = 64;
parameter AMSB = ABW-1;
parameter RSTPC = 64'hFFFFFFFFFFFC0100;
input clk_i;
input [7:0] asid;
input [AMSB:0] pc0;
input [AMSB:0] pc1;
input [AMSB:0] pc2;
input hit0;
input hit1;
input hit2;
input [4:0] bstate;
input thread_en;
input ihitL2;
output reg [71:0] L1_adr = RSTPC;
output reg L1_wr0;
output reg L1_wr1;
output reg L1_wr2;
output reg [9:0] L1_en;
output reg L1_invline;
output reg icnxt;
output reg [1:0] icwhich;
output ziccnt;
output ld_L1_dati;

parameter TRUE = 1'b1;
parameter FALSE = 1'b0;
parameter IDLE = 4'd0;
parameter IC1 = 4'd1;
parameter IC2 = 4'd2;
parameter IC3 = 4'd3;
parameter IC_WaitL2 = 4'd4;
parameter IC5 = 4'd5;
parameter IC6 = 4'd6;
parameter IC7 = 4'd7;
parameter IC_Next = 4'd8;
parameter IC9 = 4'd9;
parameter IC10 = 4'd10;
parameter IC3a = 4'd11;

reg [3:0] icstate = IDLE, picstate;
`include ".\FT64_busStates.vh"

wire [71:0] pc0plus6 = pc0 + 72'd6;
wire [71:0] pc0plus12 = pc0 + 72'd12;

assign ziccnt = icstate==IDLE;
assign ld_L1_dati = icstate==IC_WaitL2 && ihitL2 && picstate==IC3a;

BUFH uclkb (.I(clk_i), .O(clk));

always @(posedge clk)
begin
L1_wr0 <= FALSE;
L1_wr1 <= FALSE;
L1_wr2 <= FALSE;
L1_en <= 10'h000;
L1_invline <= FALSE;
icnxt <= FALSE;
// Instruction cache state machine.
// On a miss first see if the instruction is in the L2 cache. No need to go to
// the BIU on an L1 miss.
// If not the machine will wait until the BIU loads the L2 cache.

// Capture the previous ic state, used to determine how long to wait in
// icstate #4.
picstate <= icstate;
case(icstate)
IDLE:
	begin
		// If the bus unit is busy doing an update involving L1_adr or L2_adr
		// we have to wait.
		if (bstate != B_ICacheAck && bstate != B_ICacheNack && bstate != B_ICacheNack2) begin
			if (!hit0) begin
				L1_adr <= {asid,pc0[AMSB:5],5'h0};
				L1_invline <= TRUE;
				icwhich <= 2'b00;
				icstate <= IC2;
			end
			else if (!hit1 && `WAYS > 1) begin
				if (thread_en) begin
					L1_adr <= {asid,pc1[AMSB:5],5'h0};
				end
				else begin
					L1_adr <= {asid,pc0plus6[AMSB:5],5'h0};
				end
				L1_invline <= TRUE;
				icwhich <= 2'b01;
				icstate <= IC2;
			end
			else if (!hit2 && `WAYS > 2) begin
				if (thread_en) begin
					L1_adr <= {asid,pc2[AMSB:5],5'h0};
				end
				else begin
					L1_adr <= {asid,pc0plus12[AMSB:5],5'h0};
				end
				L1_invline <= TRUE;
				icwhich <= 2'b10;
				icstate <= IC2;
			end
		end
	end
IC2:     icstate <= IC3;
IC3:     icstate <= IC3a;
IC3a:     icstate <= IC_WaitL2;
// If data was in the L2 cache already there's no need to wait on the
// BIU to retrieve data. It can be determined if the hit signal was
// already active when this state was entered in which case waiting
// will do no good.
// The IC machine will stall in this state until the BIU has loaded the
// L2 cache. 
IC_WaitL2: 
	if (ihitL2 && picstate==IC3a) begin
		L1_en <= 10'h3FF;
		L1_wr0 <= TRUE;
		L1_wr1 <= TRUE && `WAYS > 1;
		L1_wr2 <= TRUE && `WAYS > 2;
//		L1_adr <= L2_adr;
		// L1_dati is loaded dring an L2 icache load operation
//		if (picstate==IC3a)
//		L1_dati <= L2_dato;
		icstate <= IC5;
	end
	else if (bstate!=B_ICacheNack)
		;
	else begin
		L1_en <= 10'h3FF;
		L1_wr0 <= TRUE;
		L1_wr1 <= TRUE && `WAYS > 1;
		L1_wr2 <= TRUE && `WAYS > 2;
//		L1_adr <= L2_adr;
		// L1_dati set below while loading cache line
		//L1_dati <= L2_dato;
		icstate <= IC5;
	end

IC5: 	icstate <= IC6;
IC6:  icstate <= IC7;
IC7:	icstate <= IC_Next;
IC_Next:
  begin
   icstate <= IDLE;
   icnxt <= TRUE;
	end
default:
	begin
   	icstate <= IDLE;
  end
endcase
end

endmodule
