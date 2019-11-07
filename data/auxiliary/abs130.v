module abs130(a, b, y);

parameter WIDTH = 130;

input [WIDTH-1:0] a, b;
output reg [WIDTH-1:0] y;

always @(*)
begin
    if (a > b)
        y = a - b;
    else
        y = b - a;
end

endmodule
