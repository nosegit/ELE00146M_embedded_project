#Pin configuration for ZedBoard PMOD header JB1
set_property PACKAGE_PIN W12 [get_ports {spi_rtl_ss_io[0]}]
set_property PACKAGE_PIN W11 [get_ports spi_rtl_io0_io]
set_property PACKAGE_PIN V10 [get_ports spi_rtl_io1_io]
set_property PACKAGE_PIN W8 [get_ports spi_rtl_sck_io]

#Set Voltage
set_property IOSTANDARD LVCMOS33 [get_ports {spi_rtl_*}]

#Slave Select (SS) -> MUST BE PULLUP
set_property PULLUP true [get_ports {spi_rtl_ss_io[0]}]

#Other pins (SCK, MOSI, MISO) -> PULLDOWN
set_property PULLDOWN true [get_ports spi_rtl_io0_io]
set_property PULLDOWN true [get_ports spi_rtl_io1_io]
set_property PULLDOWN true [get_ports spi_rtl_sck_io]