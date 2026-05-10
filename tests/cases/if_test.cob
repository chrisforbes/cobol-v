IDENTIFICATION DIVISION.
PROGRAM-ID. IF-TEST.
DATA DIVISION.
WORKING-STORAGE SECTION.
01  A PIC FLOAT VALUE 1.0.
01  B PIC FLOAT VALUE 2.0.
01  RES PIC FLOAT.
PROCEDURE DIVISION.
    MAIN.
        IF A < B
            MOVE 10.0 TO RES
        END-IF.
        
        IF A > B
            MOVE 20.0 TO RES
        ELSE
            IF A = 1.0
                MOVE 40.0 TO RES
            ELSE
                MOVE 30.0 TO RES
            END-IF
        END-IF.
        
        GOBACK.
