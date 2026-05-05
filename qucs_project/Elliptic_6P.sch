<Qucs Schematic 26.1.0>
<Properties>
  <View=-40,-365,1842,803,0.901169,0,0>
  <Grid=10,10,1>
  <DataSet=Elliptic_6P.dat>
  <DataDisplay=Elliptic_6P.dpl>
  <OpenDisplay=0>
  <Script=Elliptic_6P.m>
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
  <Vac Vin 1 320 300 18 -26 0 1 "1 V" 1 "1 kHz" 0 "0" 0 "0" 0 "0" 0 "0" 0>
  <GND * 1 320 380 0 0 0 0>
  <R R1 1 430 270 -26 15 0 0 "R1_val" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <C C1 1 500 300 17 -26 0 1 "C1_val" 1 "" 0 "neutral" 0>
  <C C2 1 760 190 -26 17 0 0 "C2_val" 1 "" 0 "neutral" 0>
  <.AC AC1 1 320 440 0 34 0 0 "log" 1 "70 MHz" 1 "200 MHz" 1 "500" 1 "no" 0>
  <.SW MonteCarlo 1 470 430 0 56 0 0 "AC1" 1 "lin" 1 "run" 1 "1" 1 "100" 1 "50" 1>
  <C C7 1 1390 300 17 -26 0 1 "C7_val" 1 "" 0 "neutral" 0>
  <C C5 1 1160 190 -26 17 0 0 "C5_val" 1 "" 0 "neutral" 0>
  <C C4 1 950 300 17 -26 0 1 "C4_val" 1 "" 0 "neutral" 0>
  <C C3 1 860 190 -26 17 0 0 "C3_val" 1 "" 0 "neutral" 0>
  <C C6 1 1280 190 -26 17 0 0 "C6_val" 1 "" 0 "neutral" 0>
  <C C8 1 1600 270 -26 17 0 0 "C8_val" 1 "" 0 "neutral" 0>
  <R R2 1 1720 300 15 -26 0 1 "R2_val" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <INDQ LQ1 1 600 300 17 -26 0 1 "L1_val" 1 "50" 1 "300 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ2 1 760 270 -26 17 0 0 "L2_val" 1 "60" 1 "250 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ3 1 860 270 -26 17 0 0 "L3_val" 1 "65" 1 "150 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ4 1 1050 300 17 -26 0 1 "L4_val" 1 "72" 1 "500 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ5 1 1160 270 -26 17 0 0 "L5_val" 1 "55" 1 "150 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ6 1 1280 270 -26 17 0 0 "L6_val" 1 "38" 1 "250 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ7 1 1490 300 17 -26 0 1 "L7_val" 1 "50" 1 "250 MHz" 0 "Linear" 0 "26.85" 0>
  <INDQ LQ8 1 1680 270 -26 17 0 0 "L8_val" 1 "50" 1 "250 MHz" 0 "Linear" 0 "26.85" 0>
  <NutmegEq VoutDB 5 350 620 -29 18 0 0 "AC1" 1 "VoutdB=db(V(out) / v(in))" 1 "Voutph=phase(V(out)) * 180/(2*pi)" 1>
  <SpicePar Inputs 1 380 -190 -27 18 0 0 "tol=0.05" 1 "C1_nom=51p" 1 "C2_nom=20p" 1 "C3_nom=43p" 1 "C4_nom=91p" 1 "C5_nom=39p" 1 "C6_nom=68p" 1 "C7_nom=82p" 1 "C8_nom=10p" 1 "L1_nom=27n" 1 "L2_nom=33n" 1 "L3_nom=68n" 1 "L4_nom=16n" 1 "L5_nom=22n" 1 "L6_nom=36n" 1 "L7_nom=18n" 1 "L8_nom=160n" 1 "R1_nom=50" 1 "R2_nom=50" 1>
  <SpicePar CalcValues 1 510 -190 -27 18 0 0 "C1_val=gauss(C1_nom, tol, 1)" 1 "C2_val=gauss(C2_nom, tol, 1)" 1 "C3_val=gauss(C3_nom, tol, 1)" 1 "C4_val=gauss(C4_nom, tol, 1)" 1 "C5_val=gauss(C5_nom, tol, 1)" 1 "C6_val=gauss(C6_nom, tol, 1)" 1 "C7_val=gauss(C7_nom, tol, 1)" 1 "C8_val=gauss(C8_nom, tol, 5)" 1 "L1_val=gauss(L1_nom, tol, 1)" 1 "L2_val=gauss(L2_nom, tol, 1)" 1 "L3_val=gauss(L3_nom, tol, 1)" 1 "L4_val=gauss(L4_nom, tol, 1)" 1 "L5_val=gauss(L5_nom, tol, 1)" 1 "L6_val=gauss(L6_nom, tol, 1)" 1 "L7_val=gauss(L7_nom, tol, 1)" 1 "L8_val=gauss(L8_nom, tol, 1)" 1 "R1_val=gauss(R1_nom, tol, 1)" 1 "R2_val=gauss(R2_nom, tol, 1)" 1 "run=1" 1>
</Components>
<Wires>
  <320 270 400 270 "" 0 0 0 "">
  <320 330 320 380 "" 0 0 0 "">
  <460 270 500 270 "" 0 0 0 "">
  <500 330 600 330 "" 0 0 0 "">
  <320 330 500 330 "" 0 0 0 "">
  <1190 270 1220 270 "" 0 0 0 "">
  <950 330 1050 330 "" 0 0 0 "">
  <500 270 600 270 "" 0 0 0 "">
  <730 190 730 270 "" 0 0 0 "">
  <950 270 1050 270 "" 0 0 0 "">
  <1130 190 1130 270 "" 0 0 0 "">
  <1190 190 1220 190 "" 0 0 0 "">
  <1220 190 1220 270 "" 0 0 0 "">
  <890 270 920 270 "" 0 0 0 "">
  <830 190 830 270 "" 0 0 0 "">
  <890 190 920 190 "" 0 0 0 "">
  <920 190 920 270 "" 0 0 0 "">
  <790 190 810 190 "" 0 0 0 "">
  <810 190 810 270 "" 0 0 0 "">
  <790 270 810 270 "" 0 0 0 "">
  <810 270 830 270 "" 0 0 0 "">
  <920 270 950 270 "" 0 0 0 "">
  <1340 270 1390 270 "" 0 0 0 "">
  <1220 270 1250 270 "" 0 0 0 "">
  <1310 270 1340 270 "" 0 0 0 "">
  <1250 190 1250 270 "" 0 0 0 "">
  <1310 190 1340 190 "" 0 0 0 "">
  <1340 190 1340 270 "" 0 0 0 "">
  <1390 330 1490 330 "" 0 0 0 "">
  <1390 270 1490 270 "" 0 0 0 "">
  <1630 270 1650 270 "" 0 0 0 "">
  <1710 270 1720 270 "" 0 0 0 "">
  <600 330 950 330 "" 0 0 0 "">
  <600 270 730 270 "" 0 0 0 "">
  <1050 330 1390 330 "" 0 0 0 "">
  <1050 270 1130 270 "" 0 0 0 "">
  <1490 330 1720 330 "" 0 0 0 "">
  <1490 270 1570 270 "" 0 0 0 "">
  <500 270 500 270 "in" 530 240 0 "">
  <1720 270 1720 270 "out" 1730 230 0 "">
</Wires>
<Diagrams>
  <Rect 690 673 369 233 3 #c0c0c0 1 00 1 7e+07 5e+07 5e+08 1 -65.7717 20 6.19944 1 -1 0.5 1 315 0 225 1 1 0 "Frequency (Hz)" "Magnitude (dB)" "">
	<"ngspice/ac.voutdb" #0000ff 0 3 0 0 0>
	  <Mkr 1.39218e+08/1 256 -264 3 0 0>
  </Rect>
  <Rect 1130 673 369 233 3 #c0c0c0 1 00 1 1e+07 1 1e+09 1 nan 50 107.987 1 -1 0.5 1 315 0 225 1 1 0 "Frequency (Hz)" "Phase (deg.)" "">
	<"ngspice/ac.voutph" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
</Paintings>
