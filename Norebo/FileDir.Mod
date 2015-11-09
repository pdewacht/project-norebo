MODULE FileDir;
  IMPORT SYSTEM, Norebo;

  CONST FnLength* = 32;

  TYPE FileName*  = ARRAY FnLength OF CHAR;
    EntryHandler* = PROCEDURE (name: FileName; unused: INTEGER; VAR continue: BOOLEAN);

  PROCEDURE Enumerate*(prefix: ARRAY OF CHAR; proc: EntryHandler);
    VAR name: FileName;
      continue: BOOLEAN;
      i: INTEGER;
      pfx, nmx: CHAR;
  BEGIN continue := TRUE;
    Norebo.SysReq(Norebo.filedirEnumerateBegin, 0, 0, 0);
    Norebo.SysReq(Norebo.filedirEnumerateNext, SYSTEM.ADR(name), 0, 0);
    WHILE continue & (Norebo.res = 0) DO
      i := 0;
      REPEAT pfx := prefix[i]; nmx := name[i]; INC(i)
      UNTIL (pfx # nmx) OR (pfx = 0X);
      IF pfx = 0X THEN proc(name, 0, continue) END;
      Norebo.SysReq(Norebo.filedirEnumerateNext, SYSTEM.ADR(name), 0, 0)
    END;
    Norebo.SysReq(Norebo.filedirEnumerateEnd, 0, 0, 0)
  END Enumerate;

  PROCEDURE Init*;
  END Init;

END FileDir.
