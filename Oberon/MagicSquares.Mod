MODULE MagicSquares;   (*NW 11.8.97*)
  IMPORT Texts, Oberon;

  VAR W: Texts.Writer;

  PROCEDURE Generate*;  (*magic square of order 3, 5, 7, ... *)
    VAR i, j, x, nx, nsq, n: INTEGER;
      M: ARRAY 13, 13 OF INTEGER;
      S: Texts.Scanner;
  BEGIN Texts.OpenScanner(S, Oberon.Par.text, Oberon.Par.pos); Texts.Scan(S);
    IF S.class = Texts.Int THEN
      n := S.i; nsq := n*n; x := 0;
      i := n DIV 2; j := n-1;
      WHILE x < nsq DO
        nx := n + x; j := (j-1) MOD n; INC(x); M[i, j] := x;
        WHILE x < nx DO
          i := (i+1) MOD n; j := (j+1) MOD n;
          INC(x); M[i, j] := x
        END
      END ;
      FOR i := 0 TO n-1 DO
        FOR j := 0 TO n-1 DO Texts.WriteInt(W, M[i, j], 6) END ;
        Texts.WriteLn(W)
      END ;
      Texts.Append(Oberon.Log, W.buf)
    END
  END Generate;

BEGIN Texts.OpenWriter(W)
END MagicSquares.
