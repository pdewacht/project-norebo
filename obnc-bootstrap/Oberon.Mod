MODULE Oberon;
  IMPORT Texts, Out;

  VAR Log*: Texts.Text;

  PROCEDURE Clock*(): INTEGER;
  BEGIN RETURN 0
  END Clock;

  PROCEDURE Time*(): INTEGER;
  BEGIN RETURN 0
  END Time;


  PROCEDURE OutputLog(T: Texts.Text; op: INTEGER; beg, end: INTEGER);
    VAR R: Texts.Reader;
      B: Texts.Buffer;
      ch: CHAR;
  BEGIN
    IF op = Texts.insert THEN
      Texts.OpenReader(R, T, beg);
      WHILE beg # end DO
        Texts.Read(R, ch);
        IF ch = 0DX THEN ch := 0AX END;
        Out.Char(ch);
        INC(beg)
      END;
(*      NEW(B); Texts.OpenBuf(B); Texts.Delete(T, beg, end, B);*)
    END
  END OutputLog;

  PROCEDURE OpenLog*;
  BEGIN NEW(Log); Log.notify := OutputLog; Texts.Open(Log, "")
  END OpenLog;

  PROCEDURE Collect* (count: INTEGER);
  BEGIN
  END Collect;
BEGIN OpenLog
END Oberon.
