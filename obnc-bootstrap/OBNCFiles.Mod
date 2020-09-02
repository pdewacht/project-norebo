MODULE OBNCFiles;
  IMPORT SYSTEM, Files;

  TYPE File*    = Files.File;
    Rider* = RECORD rr: Files.Rider; eof*: BOOLEAN END;

  PROCEDURE Old*(name: ARRAY OF CHAR): File;
  BEGIN RETURN Files.Old(name)
  END Old;

  PROCEDURE New*(name: ARRAY OF CHAR): File;
  BEGIN RETURN Files.New(name)
  END New;

  PROCEDURE Register*(f: File);
  BEGIN Files.Register(f); Files.Close(f);
  END Register;

  PROCEDURE Close*(f: File);
  BEGIN Files.Close(f)
  END Close;

  PROCEDURE Delete*(name: ARRAY OF CHAR; VAR res: INTEGER);
  BEGIN Files.Delete(name, res)
  END Delete;

  PROCEDURE Rename*(old, new: ARRAY OF CHAR; VAR res: INTEGER);
  BEGIN Files.Rename(old, new, res)
  END Rename;

  PROCEDURE Length*(f: File): INTEGER;
  BEGIN RETURN Files.Length(f)
  END Length;

  PROCEDURE Date*(f: File): INTEGER;
  BEGIN RETURN 0
  END Date;

  PROCEDURE Set*(VAR r: Rider; f: File; pos: INTEGER);
  BEGIN IF pos # -1 THEN Files.Set(r.rr, f, pos); r.eof := r.rr.eof END
  END Set;

  PROCEDURE Pos*(VAR r: Rider): INTEGER;
  BEGIN RETURN Files.Pos(r.rr)
  END Pos;

  PROCEDURE Base*(VAR r: Rider): File;
  BEGIN RETURN Files.Base(r.rr)
  END Base;

  PROCEDURE ReadByte*(VAR r: Rider; VAR x: BYTE);
  BEGIN Files.Read(r.rr, x); r.eof := r.rr.eof
  END ReadByte;

  PROCEDURE ReadBytes*(VAR r: Rider; VAR x: ARRAY OF BYTE; n: INTEGER);
  BEGIN Files.ReadBytes(r.rr, x, n); r.eof := r.rr.eof
  END ReadBytes;

  PROCEDURE Read*(VAR r: Rider; VAR ch: CHAR);
    VAR b: BYTE;
  BEGIN Files.Read(r.rr, b); r.eof := r.rr.eof; ch := CHR(b)
  END Read;

  PROCEDURE ReadInt*(VAR R: Rider; VAR x: INTEGER);
    VAR x0, x1, x2, x3: BYTE;
  BEGIN ReadByte(R, x0); ReadByte(R, x1); ReadByte(R, x2); ReadByte(R, x3);
    x := ((x3 * 100H + x2) * 100H + x1) * 100H + x0
  END ReadInt;

  PROCEDURE ReadSet*(VAR R: Rider; VAR s: SET);
    VAR n: INTEGER;
  BEGIN ReadInt(R, n); s := SYSTEM.VAL(SET, n)
  END ReadSet;

  PROCEDURE ReadReal*(VAR R: Rider; VAR x: REAL);
    VAR n: INTEGER;
  BEGIN ReadInt(R, n); x := SYSTEM.VAL(REAL, n)
  END ReadReal;

  PROCEDURE ReadString*(VAR R: Rider; VAR x: ARRAY OF CHAR);
    VAR i: INTEGER; ch: CHAR;
  BEGIN i := 0; Read(R, ch);
    WHILE ch # 0X DO
      IF i < LEN(x)-1 THEN x[i] := ch; INC(i) END ;
      Read(R, ch)
    END ;
    x[i] := 0X
  END ReadString;

  PROCEDURE ReadNum*(VAR R: Rider; VAR x: INTEGER);
    VAR n, y: INTEGER; b: BYTE;
  BEGIN n := 32; y := 0; ReadByte(R, b);
    WHILE b >= 80H DO y := ROR(y + b-80H, 7); DEC(n, 7); ReadByte(R, b) END ;
    IF n <= 4 THEN x := ROR(y + b MOD 10H, 4) ELSE x := ASR(ROR(y + b, 7), n-7) END
  END ReadNum;

  PROCEDURE WriteByte*(VAR r: Rider; x: BYTE);
  BEGIN Files.Write(r.rr, x); r.eof := r.rr.eof
  END WriteByte;

  PROCEDURE WriteBytes*(VAR r: Rider; VAR x: ARRAY OF BYTE; n: INTEGER);
  BEGIN Files.WriteBytes(r.rr, x, n); r.eof := r.rr.eof
  END WriteBytes;

  PROCEDURE Write*(VAR r: Rider; ch: CHAR);
  BEGIN Files.Write(r.rr, ORD(ch)); r.eof := r.rr.eof
  END Write;

  PROCEDURE WriteInt*(VAR R: Rider; x: INTEGER);
  BEGIN WriteByte(R, x MOD 100H);
    WriteByte(R, x DIV 100H MOD 100H);
    WriteByte(R, x DIV 10000H MOD 100H);
    WriteByte(R, x DIV 1000000H MOD 100H)
  END WriteInt;

  PROCEDURE WriteSet*(VAR R: Rider; s: SET);
  BEGIN WriteInt(R, SYSTEM.VAL(INTEGER, s))
  END WriteSet;

  PROCEDURE WriteReal*(VAR R: Rider; x: REAL);
  BEGIN  WriteInt(R, SYSTEM.VAL(INTEGER, x))
  END WriteReal;

  PROCEDURE WriteString*(VAR R: Rider; x: ARRAY OF CHAR);
    VAR i: INTEGER; ch: CHAR;
  BEGIN i := 0;
    REPEAT ch := x[i]; Write(R, ch); INC(i) UNTIL ch = 0X
  END WriteString;

  PROCEDURE WriteNum*(VAR R: Rider; x: INTEGER);
  BEGIN
    WHILE (x < -40H) OR (x >= 40H) DO WriteByte(R, x MOD 80H + 80H); x := ASR(x, 7) END ;
    WriteByte(R, x MOD 80H)
  END WriteNum;

END OBNCFiles.
