
;   ZX Spectrum Emulator.
;
;   Copyright (C) 2017-2019 Ivan Kosarev.
;   ivan@kosarev.info
;
;   Published under the MIT license.

;   This test is supposed to make visible possible defects in ULA
;   emulation with regard to how it renders border areas. It
;   proceeds in two steps. First, it makes sure that the ISR code
;   gets control at a specific frame tick. Then, on every frame
;   it draws 16 lines on the top border right above the screen
;   area so that the first line starts by an 'OUT (0xfe), A'
;   instruction which last tick is the beginning of the 48th
;   scanline (48 * 224 = 10752) and every subsequent line starts
;   by another 'OUT (0xfe), A' instruction 225 ticks later,
;   meaning every new line is placed on the next scanline and
;   shifted right by one CPU tick.
;
;   Here's how the test syncs calling the ISR with activation of
;   the ~INT signal. The first time the ISR gets control, we know
;   ~INT became active at one of the four ticks (T states) of the
;   HALT's M1 cycle:
;
;   -------  ------ M1 -------
;   T3 | T4  T1 | T2 | T3 | T4
;                           ^^ ~INT goes active here or
;
;   - M1 -------  ------ M1 -------
;   T2 | T3 | T4  T1 | T2 | T3 | T4
;                           ^^ ~INT goes active here or
;
;   ------ M1 -------  ------ M1 -------
;   T1 | T2 | T3 | T4  T1 | T2 | T3 | T4
;                           ^^ ~INT goes active here or
;
;        ------ M1 -------  ------ M1 -------
;        T1 | T2 | T3 | T4  T1 | T2 | T3 | T4
;                           ^^ ~INT goes active here.
;
;   Then, on every frame the ISR takes one tick longer than usual
;   until the number of executed M1 cycles of the HALT
;   instruction is changed, which can only happen if we reached
;   the T4 of the previous M1 cycle:
;
;            ------ M1 -------  ------ M1 -------
;            T1 | T2 | T3 | T4  T1 | T2 | T3 | T4
;                           ^^ ~INT goes active here.
;
;   At this point we know ~INT will be sampled at T4 of the first
;   M1 cycle, so the CPU will start the 19 ticks long
;   sequence calling the IM2 ISR immediately after that T4 tick.
;   This means the ISR will get control at tick 20.

    org 0x8000

    di                      ; No interrupts as we set up things.

    ld sp, 0xffff           ; Set up stack. We want it to be in
                            ; the non-contended memory.

    ld a, high(isr_addr)    ; Set up the ISR address.
    ld i, a
    im 2

    ld ix, reset_m1_count   ; Make sure we are at a specific tick
                            ; in frame. For this, start with
                            ; waiting for an interrupt and
                            ; counting M1 cycles beginning an EI
                            ; followed by HALT and till the next
                            ; interrupt.

    ld a, 6                 ; Yellow border.
    out (0xfe), a

    ei                      ; Pass control to the ISR.
    halt


    ds 0x80ff - $
isr_addr:                   ; The address of the ISR.
    dw isr


isr:
                            ; 1 to 4 ticks since activating of
                            ; the ~INT signal to complete the
                            ; current instruction.

                            ; 19            Interrupt acknowledgement.

    pop hl                  ; 19 + 10 = 29  Remove the return address from the stack.

    jp (ix)                 ; 29 +  8 = 37  Do some work.


    ; Step 1: Reset the M1 cycles counter.
reset_m1_count:
    ld a, r                 ; 37 +  9 = 46
    ld a, c                 ; 46 +  4 = 50
                            ; We don't the values, but these
                            ; instructions help to make the total
                            ; number of ticks to be the same as
                            ; in steps 2 and 3.

                            ; 50 + 14 = 64
    ld ix, remember_m1_count

    ld a, 0                 ; 64 +  7 = 71
    ld r, a                 ; 71 +  9 = 80

    ei                      ; Continue on the next interrupt. It
    halt                    ; is important that we do that at a
                            ; tick that is a multiple of 4 since
                            ; beginning of this ISR call so the
                            ; next interrupt occurs at exactly
                            ; same tick within M1 of the HALT
                            ; instruction.


    ; Step 2: Remember the number of M1 cycles passed since last
    ; interrupt's EI.
remember_m1_count:
    ld a, r                 ; 37 +  9 = 46  Remember the number
    ld c, a                 ; 46 +  4 = 50  of M1 cycles.

    ld ix, calibrate        ; 50 + 14 = 64
                            ; The next step is to adjust the
                            ; number of ticks between HALT
                            ; instructions such that we know ~INT
                            ; becomes active at a specific tick
                            ; within HALT's M1 cycle.
                            ;
                            ; Note that the total number of ticks
                            ; at steps 1, 2 and 3 must be the same
                            ; and the value of the R register at
                            ; steps 2 and 3 must be sampled at
                            ; the same tick for the calibration
                            ; to work correctly.

    ld a, 0                 ; 64 +  7 = 71
    ld r, a                 ; 71 +  9 = 80

    ei
    halt


    ; Step 3: Adjust the number of ticks between EI instructions
    ; of subsequent interrupts.
calibrate:
    ld a, r                 ; 37 +  9 = 46  See if one more HALT
    cp c                    ; 46 +  4 = 50  instruction has been
    jr nz, done_calibration ; 50 +  7 = 57  executed this time.

    nop                     ; 57 +  4 = 61
    nop                     ; 61 +  4 = 65

    ld a, 0                 ; 65 +  7 = 72
    ld r, a                 ; 72 +  9 = 81  One tick longer.

    ei
    halt

done_calibration:
    ld ix, draw             ; (57 + 5) + 14 = 76

    ei
    halt


    ; Step 4: While in sync with the moment when ~INT becomes
    ; active, draw something on the screen so that imprecise
    ; timing becomes visible.
draw:
                            ; 1 tick since ~INT to complete
                            ; execution of the current HALT
                            ; instruction.
                            ;
                            ; 37 ticks to get to this point.
                            ;
    ld a, 1                 ; 7

    ld b, 16                ; 7

    ld c, 218               ; 7

                            ; This is frame tick 59.

top_lines_delay:            ; 10459
    nop                     ;   4
    nop                     ;   4
    nop                     ;   4
    nop                     ;   4
    nop                     ;   4
    nop                     ;   4
    nop                     ;   4
    nop                     ;   4
    dec c                   ;   4
    jr nz, top_lines_delay  ;   7 + 5

                            ; This is frame tick 10518.

    dec de                  ; 6
    dec de                  ; 6
    dec de                  ; 6
    dec de                  ; 6
    dec de                  ; 6
    ld c, 0                 ; 7
    ld c, 0                 ; 7
    ld c, 0                 ; 7

                            ; This is frame tick 10569.

draw_line:                  ; 3595
    ld c, 10                ;   7

delay_line:                 ;   155
    dec c                   ;     4
    jr nz, delay_line       ;     7 + 5

    ld a, b                 ;   4
    and 3                   ;   7


    out (0xfe), a           ;   11
                            ; This instruction is first executed
                            ; at tick 10742. The last tick of
                            ; that instruction is thus 10752,
                            ; which is the beginning of the 48th
                            ; scanline (48 * 224 = 10752).

    ld a, 6                 ;   7
    out (0xfe), a           ;   11
    xor 5                   ;   7

    dec b                   ;   4
    jr nz, draw_line        ;   7 + 5

                            ; This is frame tick 14164.

    ld c, 8                 ; 7

delay:                      ; 123
    dec c                   ;   4
    jr nz, delay            ;   7 + 5

                            ; 13
    ld a, r                 ;   9
    nop                     ;   4

                            ; This is frame tick 14307.

                            ; 24
    ld hl, 0x4000           ;   10
    ld a, 0xff              ;   7
    ld b, 0x00              ;   7

    ld (hl), a              ; 7
                            ; This instruction is at tick 14331,
                            ; meaning its write cycle starts at
                            ; tick 14335 -- the last tick at
                            ; which a memory write cycle would
                            ; perform uncontended before the
                            ; first pixel of the screen area is
                            ; displayed.

    ld (hl), b              ; 7

    ; ld a, 0                 ; 7
                            ; This instruction is to maintain the
                            ; phase of ~INT within HALT's M1
                            ; cycle. For this, the total number
                            ; of ticks since ~INT not counting
                            ; the first one (sampling) must be a
                            ; multiple of 4.

    ei
    halt
