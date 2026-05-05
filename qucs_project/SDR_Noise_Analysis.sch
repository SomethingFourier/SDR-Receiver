<Qucs Schematic 26.1.0>
<Properties>
  <View=-17,-46,1518,1062,1.10493,0,171>
  <Grid=10,10,1>
  <DataSet=SDR_Noise_Analysis.dat>
  <DataDisplay=SDR_Noise_Analysis.dpl>
  <OpenDisplay=0>
  <Script=SDR_Noise_Analysis.m>
  <RunScript=0>
  <showFrame=0>
  <FrameText0=Title>
  <FrameText1=Drawn By:>
  <FrameText2=Date:>
  <FrameText3=Revision:>
</Properties>
<Symbol>
</Symbol>
<Components>
  <GND * 1 820 420 0 0 0 0>
  <GND * 1 200 460 0 0 0 0>
  <SpLib X1 1 810 380 -26 48 0 0 "lib/OPA161x.LIB" 0 "OPA161X" 0 "opamp5t" 0 "" 0 "1;2;3;4;5" 0>
  <SpiceInclude SpiceInclude1 5 1080 520 -34 18 0 0 "lib/OPA161x.LIB" 1 "" 0 "" 0 "" 0 "" 0>
  <R R2 1 920 380 -26 15 0 0 "47 Ohm" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <R R3 1 1040 260 -26 15 0 0 "10 kOhm" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <R R1 1 540 400 -26 15 0 0 "50 Ohm" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <R R4 1 1320 430 15 -26 0 1 "50 Ohm" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <GND * 1 1320 460 0 0 0 0>
  <GND * 1 390 460 0 0 0 0>
  <Vdc V2 1 200 430 18 -26 0 1 "5 V" 1>
  <.AC AC1 0 310 100 0 34 0 0 "log" 1 "1 Hz" 1 "200 kHz" 1 "500" 1 "no" 0>
  <GND * 1 1050 360 0 0 0 2>
  <.NOISE NOISE1 0 130 50 0 56 0 0 "log" 1 "1 Hz" 1 "200 kHz" 1 "500" 1 "v(I_diff)" 1 "V1" 1 "" 0>
  <NutmegEq NutmegEq3 4 560 550 -29 18 0 0 "NOISE1" 1 "k=1.3806e-23" 1 "T=300" 1 "Rs=50" 1 "noise_src_density=4 * k * T * Rs" 1 "snr_in=(abs(v_sig_in)^2) / noise_src_density" 1 "snr_in_db=10 * log10(snr_in)" 1 "snr_out=(abs(v_sig_out)^2) / onoise_spectrum" 1 "snr_out_db=10 * log10(snr_out)" 1 "noise_factor=snr_in / snr_out" 1 "NF=10*log10(noise_factor)" 1>
  <NutmegEq NutmegEq2 4 860 540 -29 18 0 0 "AC1" 1 "v_sig_out=V(I_diff)" 1 "v_sig_in=v(in)" 1 "transfer=v_sig_out / v_sig_in" 1>
  <Vdc V3 1 300 430 18 -26 0 1 "2.5 V" 1>
  <GND * 1 300 460 0 0 0 0>
  <Vac V1 1 390 430 18 -26 0 1 "-10 uV" 1 "100 kHz" 0 "0" 0 "0" 0 "0" 0 "0" 0>
  <SpLib X3 1 1040 400 -26 -70 1 0 "lib/OPA161x.LIB" 0 "OPA161X" 0 "opamp5t" 0 "" 0 "1;2;3;4;5" 0>
  <sTr Tr1 1 630 430 -29 -144 0 2 "0.5" 1 "0.5" 1>
  <C C1 1 1210 400 -26 17 0 0 "10 uF" 1 "" 0 "neutral" 0>
  <.CUSTOMSIM CUSTOM1 1 40 450 0 34 0 0 "*ac dec 84 1 200k\n*let v_sig_out = V(I_diff)\n*let v_sig_in = V(in)\n*let transfer = v_sig_out / v_sig_in\n\nset sqrnoise\n\nnoise v(I_diff) V1 dec 84 1 200K\n*let k = 1.3806e-23\n*let T = 300\n*let Rs = 50\n*let noise_src_density = 4 * k * T * Rs\n*let snr_in = (abs(ac1.v_sig_in)^2) / noise_src_density\n*let snr_in_db = 10 * log10(snr_in)\n*let snr_out = (abs(ac1.v_sig_out)^2) / noise1.onoise_spectrum\n*let snr_out_db = 10 * log10(snr_out)\n*let noise_factor = snr_in / snr_out\n*let NF = 10*log10(noise_factor)\nprint onoise_total\nprint inoise_total" 1 "noise1.onoise_spectrum;noise1.inoise_spectrum;noise2.onoise_total;noise2.inoise_total" 0 "" 0>
</Components>
<Wires>
  <770 400 770 470 "" 0 0 0 "">
  <770 470 870 470 "" 0 0 0 "">
  <870 380 870 470 "" 0 0 0 "">
  <870 380 890 380 "" 0 0 0 "">
  <1000 260 1010 260 "" 0 0 0 "">
  <390 400 510 400 "" 0 0 0 "">
  <570 400 600 400 "" 0 0 0 "">
  <1100 400 1120 400 "" 0 0 0 "">
  <950 380 1000 380 "" 0 0 0 "">
  <660 420 680 420 "" 0 0 0 "">
  <680 420 680 440 "" 0 0 0 "">
  <660 440 680 440 "" 0 0 0 "">
  <1000 260 1000 380 "" 0 0 0 "">
  <1070 260 1120 260 "" 0 0 0 "">
  <1120 260 1120 400 "" 0 0 0 "">
  <660 360 770 360 "" 0 0 0 "">
  <660 500 1000 500 "" 0 0 0 "">
  <1000 420 1000 500 "" 0 0 0 "">
  <1120 400 1180 400 "" 0 0 0 "">
  <1240 400 1320 400 "" 0 0 0 "">
  <390 460 600 460 "" 0 0 0 "">
  <820 340 820 340 "VCC" 830 310 0 "">
  <570 400 570 400 "in" 580 370 0 "">
  <1320 400 1320 400 "I_diff" 1330 370 0 "">
  <200 400 200 400 "VCC" 210 370 0 "">
  <660 440 660 440 "VDDA2" 680 450 0 "">
  <300 400 300 400 "VDDA2" 310 370 0 "">
  <1050 440 1050 440 "VCC" 1060 450 0 "">
</Wires>
<Diagrams>
  <Rect 500 210 240 160 3 #c0c0c0 1 00 1 -1 0.5 1 1 -1 1 1 1 -1 1 1 315 0 225 1 0 0 "" "" "">
	<"ngspice/ac1.ac.v(v_sig_out)" #0000ff 0 3 0 0 0>
  </Rect>
  <Rect 830 210 240 160 3 #c0c0c0 1 00 1 -1 0.5 1 1 -1 1 1 1 -1 1 1 315 0 225 1 0 0 "" "" "">
	<"ngspice/ac.noise1.inoise_spectrum" #ff0000 0 3 0 0 0>
  </Rect>
  <Rect 1150 200 240 160 3 #c0c0c0 1 00 1 -1 0.5 1 1 -1 1 1 1 -1 1 1 315 0 225 1 0 0 "" "" "">
	<"ngspice/ac.noise1.onoise_spectrum" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
</Paintings>
