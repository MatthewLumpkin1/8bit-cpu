; Select the larger of two values by branching on carry instead of zero.
; CMP R0, R1 borrows exactly when R0 < R1, so JC after a CMP is an unsigned
; less-than test. Z alone could not do this - it only says equal or not equal.

        MOVI R0, 37
        MOVI R1, 92

        CMP  R0, R1
        JC   R1_BIGGER

        MOV  R2, R0             ; R0 >= R1
        JMP  DONE

R1_BIGGER:
        MOV  R2, R1

DONE:   STORE R2, 0xF2          ; expect 92
        HALT
