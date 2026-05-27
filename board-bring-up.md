## Before Powering On
- ✅ Test resistance from all voltage busses to ground.
- ✅ Find a small fuse to use when powering on for the first time

## Power
- ✅ break JP7, 9 10
- ✅ Test DC in, and check diode functionality
- ✅ Test USB 5V in and U13
- ✅ Break JP18, 26, 27,28,29,30
- ✅ re connect JP 10 and begin testing voltage Regulators
- ✅ Check power LED D1
- ✅ Reconnect JP7
- ✅ Check Voltage out and ripple on the 3.3V Regulator
- ✅ Check Voltage out and ripple on the 3.3V_ADC Regulator
- ✅ Check Voltage out and ripple on the 3.3V_SI5351 Regulator
- ✅ Check Voltage out and ripple on the 3.3V_CLK Regulator
- ✅ Check Voltage out and ripple on the 4.5V Regulator
- ✅ Check Voltage out and ripple on the 2.4V Regulator
- ✅ Break JP 3 and 4

**Do not resolder power buses until specifically needed**

## Pico
- ✅ Re connect JP 18
- ✅ Connect USB and Check for recognition
- ✅ If not recognized check crystal output at C14
- ✅ if not working check 1.1V regulator for voltage and ripple
- ✅ if not working make sure the reset line is high
- ✅ Check to make sure the boot button is not default low
- ✅ Load a program to flash LEDs
- ✅ Test Reset and boot buttons
- ✅ Test other LEDs and GPIO
- ✅ Test Select and Enter buttons
- ✅ Reconnect JP27
- ✅ I2C scan check for si5351

## CMOS OCS X1
- ✅ reconnect JP28 for power
- ✅ check at pin 3 or r34 for 24.576 MHz
- ✅ Check to see that previous 24.576M at U10 Si5351 pin 2

## SI5351
- ✅ write code for testing clock 0 output
- ✅ check pin 10 tp I_LO for clock 0 out
- ⬜ write code and check clk 1 and clk2 out at Q_Lo and JP 31

- ## Test mux and filters
- ⬜ break Jp 21 and JP22
- ✅ resistance check 4.8 V bus to ground (large value expected)
- ✅ resistance check 2.4 V bus to ground (large value expected)
- ✅ reconnect JP 30 (4.8V bus)
- ✅ reconnect JP29 (2.4V bus)
- ✅ write code to toggle GPIO 8 which is the Charles mux
- ✅ Test continuity of charles mux between 4 and 3 or 1 on U17 and 3 and 4 or 1 on U19
- ✅ write code to toggle GPIO 9 which is the Jaqi mux
- ✅ test continuity of 1 or 3 and JP 12
- ⬜ feed a 1 mV 5MHz + signal into RF2
- ⬜ Check at input and output of LNA at C23 and C57 expect gain of 18 to 21 db
- ⬜ freq sweep input from 5 to 170 MHz Check output of lna expect all to be amplified
- ⬜ measure same sweep at output of charles expect lowpass
- ⬜ Test Jaci inject a 5 to 40 MHz Sweep expect lowpass cutoff at 30 MHz

## Test Diode ring mixer
- ⬜ inject 46MHz signal into RF2
- ⬜ Set SI5351 CLK2 at 121 MHz
- ⬜ Check output of Diode ring mixer pin 2 U5 should be at 30 MHz
- ⬜ Check output of Diode ring mixer JP 12 should be at 30 MHz (Set GPIO 9 to low)
- ⬜ test at range of freq checking at jp12
- ⬜ reconnect jp 22 and 21
- ⬜ Check output of transformer two scope probes measure for inverted + and - at pins 16 and 14 of T1
- ⬜ Set GPIO 9 to high to swap jaci mux to rf 1
- ⬜ Inject signal in RF 1 Check at t1 pins 16 and 14 sweep DC to 40 lc at 30 MHz

## Test Tayloe
- ⬜ measure output at jp 3 and 4 expect ~2.4 V
- ⬜ Set up code for SI5351 CLK0 and CLK1 at 60 MHz in quadrature Measure at i and q
- ⬜ Inject 25.001 MHz into Rf 1
- ⬜ Check output of tayloe detector at jp 3 and 4 expect a 1k output signal

## ADC
- ⬜ Reconnect jp 26 ADC 3.3V buss
- ⬜ write adc config code
- ⬜ inject signal into adc using 3.5 mm jack and check I2S output
- ⬜ reconnect JP 3 and 4
- ⬜ inject signal into RF1 at 25.001 MHz check i2s out
- ⬜ reconnect jp 21 and 22
- ⬜ Set GPIO 8 and 9 to low test full RF 2 signal chain by injecting 146 MHz and vary slightly for output
