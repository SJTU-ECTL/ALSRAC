module ed(clk, reset, in1, in2, out);

parameter round = 17'd102400;
parameter inWidth = 130;
parameter outWidth = 147;

input clk;
input reset;
input [inWidth - 1: 0] in1;
input [inWidth - 1: 0] in2;
output reg [outWidth: 0] out;

always @(posedge clk)
begin
    if (reset)
        out <= outWidth'd0;
    else begin
        if (in1 > in2)
            out <= out + in1 - in2;
        else
            out <= out + in2 - in1;
    end
end

endmodule
