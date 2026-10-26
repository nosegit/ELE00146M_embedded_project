library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

package filter_pkg is
    -- 7x7 Grid = 49 Coefficients
    -- Using 16-bit signed to allow for negative edge detection weights
    type coeff_array_type is array (0 to 48) of signed(15 downto 0);
end package filter_pkg;

package body filter_pkg is
end package body filter_pkg;