 ## HB-F1VX has MSX-MUSIC built straight into the firmware, you can safely initialize it using a shorter initialization format. This script sets up a multi-instrument loop utilizing the 
 
 1. Sample Code 1: internal hardware voice definitions in BASIC:

10 CLS:CALL MUSIC
20 CALL BGM(1)
30 V3$="@3 O4 T130 L4 C E G >C <" : ' Piano Melody
40 V8$="@8 O3 T130 L4 E G >C <E" : ' Organ Harmony
50 V14$="@14 O2 T130 L4 C G C G"  : ' Bass Line
60 PLAY #1,V3$, #2,V8$, #3,V14$
70 PRINT "Press Space to stop music."
80 IF INKEY$<>" " THEN GOTO 80
90 PLAY #1,"", #2,"", #3,""

2. Sample Code 2 (Using Hardware Rhythm Mode)
Using Hardware Rhythm ModeIf you prefer automated drums over a bass line, switch the chip initialization parameters to activate the built-in percussion sound generators (Bass Drum, Snare, Tom-Tom, Top Cymbal, and Hi-Hat) mapped directly onto channel #10.

10 CALL MUSIC(1) : ' Mode 1 activates 6 melody channels + 5 rhythm instruments
20 CALL BGM(1)
30 M$="@7 O4 T120 C D E C" : ' Trumpet Melody on Channel 1
40 R$="BD SD BD SD"       : ' Hardware drums on Channel 10
50 PLAY #1,M$, #10,R$


3. Sample Code 3 (Dynamic MML Sequence Array Code)
10 CLS:KEY OFF:CLEAR 2000
20 ' Initialize MSX-MUSIC in Mode 1 (Rhythm Enabled)
30 CALL MUSIC(1)
40 CALL BGM(1) : ' Play in background while code loops
50 MB=4 : ' Maximum Bars/Phrases in this chunk
60 DIM F1$(4), F2$(4), F3$(4), RH$(4)
70 ' 
80 ' --- BAR 1 DATA ---
90 F1$(1)="@3 O4 T130 L4 C E G C"      : ' Ch1 Piano Lead
100 F2$(1)="@7 O4 T130 L4 R R E G"      : ' Ch2 Trumpet Counter
110 F3$(1)="@13 O2 T130 L4 C R C R"     : ' Ch3 Synth Bass
120 RH$(1)="BD4 HH4 SD4 HH4"            : ' Ch10 Hardware Rhythm
130 ' 
140 ' --- BAR 2 DATA ---
150 F1$(2)="@3 O4 L4 D F A >D"
160 F2$(2)="@7 O4 L4 R R F A"
170 F3$(2)="@13 O2 L4 D R D R"
180 RH$(2)="BD4 HH4 SD4 HH4"
190 ' 
200 ' --- BAR 3 DATA ---
210 F1$(3)="@3 O4 L4 E G B >E"
220 F2$(3)="@7 O4 L4 R R G B"
230 F3$(3)="@13 O2 L4 E R E R"
240 RH$(3)="BD4 HH4 SD4 HH4"
250 ' 
260 ' --- BAR 4 DATA (Resolution) ---
270 F1$(4)="@3 O5 L2 C < B"
280 F2$(4)="@7 O4 L2 A G"
290 F3$(4)="@13 O2 L4 C G C R"
300 RH$(4)="BD4 CY4 BD2"
310 ' 
320 ' --- MAIN SEQUENCER LOOP ---
330 FOR B = 1 TO MB
340   PRINT "Playing Bar:"; B
350   ' Send current bar fragment to the respective FM channels
360   PLAY #1, F1$(B), #2, F2$(B), #3, F3$(B), #10, RH$(B)
370   ' 
380   ' Wait until the current bar has finished buffering before loading next
390   IF PLAY(1) OR PLAY(2) OR PLAY(3) OR PLAY(10) THEN GOTO 390
400 NEXT B
410 ' 
420 ' --- LOOP OR END ---
430 PRINT "Song Complete. Restarting... (Press CTRL+STOP to exit)"
440 GOTO 330
