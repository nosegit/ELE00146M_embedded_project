library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.filter_pkg.all;

entity filter_core is
    Port ( 
        clk           : in std_logic;
        reset_n       : in std_logic;
        
        -- Control Signals from AXI Wrapper
        data_valid_1  : in std_logic; -- Pulse for Reg 2 Write
        data_valid_2  : in std_logic; -- Pulse for Reg 3 Write (Trigger Shift)
        
        -- Configuration Registers
        control_reg   : in std_logic_vector(31 downto 0); -- Reg 0
        coefficients  : in coeff_array_type;              -- Reg 5-29 (Mapped)
        
        -- Data Input Registers
        data_in_1     : in std_logic_vector(31 downto 0); -- Reg 2 (Rows 0-3)
        data_in_2     : in std_logic_vector(31 downto 0); -- Reg 3 (Rows 4-6)
        
        -- Output Result
        result_out    : out std_logic_vector(31 downto 0) -- Reg 4
    );
end filter_core;

architecture Behavioral of filter_core is

    -- 7x7 Window Grid 
    -- CHANGED: 16-bit signed is enough to hold (0 to 255) as a positive number.
    type window_grid_type is array (0 to 6, 0 to 6) of signed(15 downto 0);
    signal window : window_grid_type := (others => (others => (others => '0')));

    -- Temporary buffer for the top half of the column
    signal temp_col_upper : std_logic_vector(31 downto 0);
    
    -- Math Signals
    signal mac_result : signed(31 downto 0);

begin

    process(clk)
        variable accum       : signed(31 downto 0);
        variable mult_factor : signed(16 downto 0); -- Keeps 17 bits for unsigned->signed conversion safety
        variable shift_amt   : integer;
        variable temp_div    : signed(48 downto 0); 
    begin
        if rising_edge(clk) then
            if reset_n = '0' then
                mac_result <= (others => '0');
                temp_col_upper <= (others => '0');
            else
                
                ------------------------------------------------------------
                -- 1. DATA LOADING & SHIFT REGISTER
                ------------------------------------------------------------
                -- Step A: Load Upper Half (Rows 0,1,2,3)
                if data_valid_1 = '1' then
                    temp_col_upper <= data_in_1;
                end if;

                -- Step B: Load Lower Half + SHIFT EVERYTHING
                if data_valid_2 = '1' then
                    -- Shift all columns to the left
                    for r in 0 to 6 loop
                        for c in 0 to 5 loop
                            window(r,c) <= window(r, c+1);
                        end loop;
                    end loop;

                    -- Insert New Column (Unpack Packed Integers)
                    -- Data In 1 (Stored)
                    -- CHANGED: Resize to 16 bits
                    window(0,6) <= resize(signed('0' & temp_col_upper(7 downto 0)), 16);
                    window(1,6) <= resize(signed('0' & temp_col_upper(15 downto 8)), 16);
                    window(2,6) <= resize(signed('0' & temp_col_upper(23 downto 16)), 16);
                    window(3,6) <= resize(signed('0' & temp_col_upper(31 downto 24)), 16);
                    
                    -- Data In 2 (Current)
                    window(4,6) <= resize(signed('0' & data_in_2(7 downto 0)), 16);
                    window(5,6) <= resize(signed('0' & data_in_2(15 downto 8)), 16);
                    window(6,6) <= resize(signed('0' & data_in_2(23 downto 16)), 16);
                end if;

                ------------------------------------------------------------
                -- 2. MAC CALCULATION (Convolution)
                ------------------------------------------------------------
                accum := (others => '0');
                
                -- Only calculate if enabled (Bit 0)
                if control_reg(0) = '1' then 
                    for r in 0 to 6 loop
                        for c in 0 to 6 loop
                            -- Multiply Window (16-bit) * Coeff (16-bit)
                            -- Standard 16x16 multiplication fits easily in DSP slices
                            accum := accum + (window(r,c) * coefficients(r*7 + c));
                        end loop;
                    end loop;
                end if;

                ------------------------------------------------------------
                -- 3. APPROXIMATION DIVIDER (High Performance Feature)
                ------------------------------------------------------------
                -- Logic: Result = (Sum * Multiplier) >> Shift
                -- Multiplier is in Control Reg [31:16] (Unsigned 16-bit)
                -- We convert to Signed 17-bit (Positive) to allow signed math
                
                mult_factor := signed('0' & control_reg(31 downto 16));
                shift_amt   := to_integer(unsigned(control_reg(12 downto 8)));
                
                -- Multiply (Result becomes wide)
                temp_div := accum * mult_factor;
                
                -- Shift Right (Divide)
                accum := resize(shift_right(temp_div, shift_amt), 32);

                ------------------------------------------------------------
                -- 4. POST PROCESSING
                ------------------------------------------------------------
                -- Absolute Value (If Bit 2 is set)
                if control_reg(2) = '1' then
                    accum := abs(accum);
                end if;

                -- Optional Clamp (If Bit 3 is set)
                if control_reg(3) = '1' then
                    if accum < 0 then
                        accum := (others => '0');
                    elsif accum > 255 then
                        accum := to_signed(255, 32);
                    end if;
                end if;

                result_out <= std_logic_vector(accum);
                
            end if;
        end if;
    end process;

end Behavioral;