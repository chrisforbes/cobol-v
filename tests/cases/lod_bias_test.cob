        IDENTIFICATION DIVISION.
        PROGRAM-ID. LOD-BIAS-TEST.
        ENVIRONMENT DIVISION.
        CONFIGURATION SECTION.
        OBJECT-COMPUTER. VULKAN-FRAGMENT-SHADER.
        INPUT-OUTPUT SECTION.
        FILE-CONTROL.
            SELECT MY-TEX ASSIGN TO "GPU-IMAGE-0-0"
                ORGANIZATION IS IMAGE-2D.
        DATA DIVISION.
        FILE SECTION.
        FD  MY-TEX.
        01  TEX-REC PIC V(4).
        WORKING-STORAGE SECTION.
        01  MY-COORD PIC V(2).
        01  MY-COLOR PIC V(4).
        01  MY-BIAS  PIC FLOAT.
        PROCEDURE DIVISION.
        MAIN-PARAGRAPH.
            MOVE 1.5 TO MY-BIAS.
            MOVE (0.5, 0.5) TO MY-COORD.
            
            *> Test implicit sampling with bias
            READ MY-TEX AT MY-COORD WITH BIAS MY-BIAS INTO MY-COLOR.
            
            *> Test with literal bias
            READ MY-TEX AT MY-COORD WITH BIAS 0.5 INTO MY-COLOR.
            
            GOBACK.
