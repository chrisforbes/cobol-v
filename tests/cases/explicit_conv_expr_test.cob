       IDENTIFICATION DIVISION.
       PROGRAM-ID. EXPLICIT-CONV-TEST.
       ENVIRONMENT DIVISION.
       CONFIGURATION SECTION.
       OBJECT-COMPUTER. VULKAN-COMPUTE-SHADER.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  MY-FLOAT          PIC FLOAT.
       01  MY-INT            PIC I.
       01  MY-UINT           PIC U.
       01  MY-V3FLOAT        PIC V(3).
       01  MY-V3INT          PIC IV(3).

       PROCEDURE DIVISION.
       MAIN-PARAGRAPH.
           MOVE 12.34 TO MY-FLOAT.
           
           *> Scalar conversions
           COMPUTE MY-INT = FLOAT-TO-INT(MY-FLOAT).
           COMPUTE MY-UINT = FLOAT-TO-UINT(MY-FLOAT).
           
           MOVE -42 TO MY-INT.
           COMPUTE MY-FLOAT = INT-TO-FLOAT(MY-INT).
           
           MOVE 100 TO MY-INT.
           COMPUTE MY-UINT = FLOAT-TO-UINT(100.0).
           COMPUTE MY-FLOAT = UINT-TO-FLOAT(MY-UINT).
           
           *> Vector conversions
           MOVE (1.1, 2.2, 3.3) TO MY-V3FLOAT.
           COMPUTE MY-V3INT = FLOAT-TO-INT(MY-V3FLOAT).
           
           MOVE (10, 20, 30) TO MY-V3INT.
           COMPUTE MY-V3FLOAT = INT-TO-FLOAT(MY-V3INT).
           
           *> Bitcast tests
           MOVE -1 TO MY-INT.
           COMPUTE MY-UINT = INT-TO-UINT(MY-INT).
           COMPUTE MY-INT = UINT-TO-INT(MY-UINT).
           
           GOBACK.
