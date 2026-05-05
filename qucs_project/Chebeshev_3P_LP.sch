<Qucs Schematic 26.1.0>
<Properties>
  <View=10,-305,1540,764,1.33124,341,370>
  <Grid=10,10,1>
  <DataSet=Chebeshev_3P_LP.dat>
  <DataDisplay=Chebeshev_3P_LP.dpl>
  <OpenDisplay=0>
  <Script=Chebeshev_3P_LP.m>
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
  <C C1 1 490 300 17 -26 0 1 "C1_val" 1 "" 0 "neutral" 0>
  <.SW MonteCarlo 1 470 430 0 56 0 0 "AC1" 1 "lin" 1 "run" 1 "1" 1 "100" 1 "50" 1>
  <R R2 1 810 300 15 -26 0 1 "R2_val" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <C C2 1 680 300 17 -26 0 1 "C2_val" 1 "" 0 "neutral" 0>
  <SpicePar CalcValues 1 510 50 -27 18 0 0 "C1_val=gauss(C1_nom, tol, 1)" 1 "C2_val=gauss(C2_nom, tol, 1)" 1 "L1_val=gauss(L1_nom, tol, 1)" 1 "R1_val=gauss(R1_nom, tol, 1)" 1 "R2_val=gauss(R2_nom, tol, 1)" 1 "run=1" 1>
  <.AC AC1 1 320 440 0 34 0 0 "log" 1 "10 MHz" 1 "100 MHz" 1 "500" 1 "no" 0>
  <NutmegEq VoutDB 5 350 620 -29 18 0 0 "AC1" 1 "VoutdB=db(V(out) / V(in))" 1 "Voutph=phase(V(out)) * 180/(2*pi)" 1>
  <INDQ LQ1 1 610 270 -26 17 0 0 "L1_val" 1 "25" 1 "100 MHz" 0 "Linear" 0 "26.85" 0>
  <SpicePar Inputs 1 370 50 -27 18 0 0 "tol=0.05" 1 "C1_nom=91p" 1 "C2_nom=91p" 1 "L1_nom=300n" 1 "R1_nom=50" 1 "R2_nom=50" 1>
</Components>
<Wires>
  <320 270 400 270 "" 0 0 0 "">
  <320 330 320 380 "" 0 0 0 "">
  <460 270 490 270 "" 0 0 0 "">
  <320 330 490 330 "" 0 0 0 "">
  <680 270 810 270 "" 0 0 0 "">
  <640 270 680 270 "" 0 0 0 "">
  <490 270 580 270 "" 0 0 0 "">
  <490 330 680 330 "" 0 0 0 "">
  <680 330 810 330 "" 0 0 0 "">
  <490 270 490 270 "in" 520 240 0 "">
  <810 270 810 270 "out" 820 240 0 "">
</Wires>
<Diagrams>
  <Rect 690 673 369 233 3 #c0c0c0 1 00 1 7e+07 5e+07 5e+08 1 -65.7717 20 6.19944 1 -1 0.5 1 315 0 225 1 1 0 "Frequency (Hz)" "Magnitude (dB)" "">
	<"ngspice/ac.voutdb" #0000ff 0 3 0 0 0>
	  <Mkr 2.22174e+07/3.02041 110 -265 3 0 0>
  </Rect>
  <Rect 1130 673 369 233 3 #c0c0c0 1 00 1 1e+07 1 1e+09 1 nan 50 107.987 1 -1 0.5 1 315 0 225 1 1 0 "Frequency (Hz)" "Phase (deg.)" "">
	<"ngspice/ac.voutph" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
</Paintings>
