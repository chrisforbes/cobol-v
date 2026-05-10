*> Storage block with STD140 layout.
*> This test verifies that STD140 is accepted on ACCESS-STORAGE blocks.
*> For this struct {float, vec3}, STD140 and STD430 produce identical
*> offsets (0, 16) since vec3's 16-byte alignment dominates.
*> The difference between the layouts only manifests with scalar arrays
*> (which require OCCURS, not yet implemented).
IDENTIFICATION DIVISION.
PROGRAM-ID. STORAGE-STD140-TEST.
INPUT-OUTPUT SECTION.
FILE-CONTROL.
    SELECT DATA-BUF ASSIGN TO "GPU-BUFFER-0-0"
        ORGANIZATION IS ACCESS-STORAGE.
DATA DIVISION.
FILE SECTION.
FD  DATA-BUF
    LAYOUT IS STD140.
01  STORAGE-DATA.
    05  VAL PIC FLOAT.
    05  POS PIC V(3).
WORKING-STORAGE SECTION.
01  TMP PIC FLOAT.
PROCEDURE DIVISION.
    MAIN.
        MOVE VAL OF STORAGE-DATA TO TMP.
        MOVE TMP TO X OF POS OF STORAGE-DATA.
        GOBACK.
