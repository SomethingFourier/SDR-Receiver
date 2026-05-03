# Power Systems and Details

## Power Sources and Busses
<img width="1196" height="824" alt="image" src="https://github.com/user-attachments/assets/89102f00-722e-434b-8df4-73c340cb9896" />

\
This SDR project has 3 main power sources: USB C, A 5V DC barrel jack, and Power over Ethernet (PoE). The first two of these are 5V level power inputs, while the PoE input is set at 5.65V. This is because a input higher voltage gives more headroom for the voltage regulators to reject as much noise as possible.

Each voltage source passes through an ideal diode (LM66100). This allows multiple power sources to be plugged in at the same time while preventing unwanted backfeeding.
