MODULE Oberon; (*derived from JG 6.9.90 / 23.9.93 / 13.8.94 / NW 14.4.2013 / 22.12.2013*)
  IMPORT SYSTEM, Norebo, Kernel, Files, Modules, RS232, Texts;

  CONST (*message ids*)
    off = 0; idle = 1; active = 2;   (*task states*)
    BasicCycle = 20;

  TYPE Task* = POINTER TO TaskDesc;

    Handler* = PROCEDURE;

    TaskDesc* = RECORD
      state, nextTime, period*: INTEGER;
      next: Task;
      handle: Handler
    END;

  VAR User*: ARRAY 8 OF CHAR; Password*: LONGINT;
    Log*: Texts.Text;

    Par*: RECORD
      text*: Texts.Text;
      pos*: LONGINT
    END;

    NofTasks*: INTEGER;
    CurTask: Task;
    ActCnt: INTEGER; (*action count for GC*)
    Mod: Modules.Module;

  (*user identification*)

  PROCEDURE Code(VAR s: ARRAY OF CHAR): LONGINT;
    VAR i: INTEGER; a, b, c: LONGINT;
  BEGIN
    a := 0; b := 0; i := 0;
    WHILE s[i] # 0X DO
      c := b; b := a; a := (c MOD 509 + 1) * 127 + ORD(s[i]);
      INC(i)
    END;
    IF b >= 32768 THEN b := b - 65536 END;
    RETURN b * 65536 + a
  END Code;

  PROCEDURE SetUser* (VAR user, password: ARRAY OF CHAR);
  BEGIN User := user; Password := Code(password)
  END SetUser;

  PROCEDURE Clock*(): LONGINT;
  BEGIN RETURN Kernel.Clock()
  END Clock;

  PROCEDURE SetClock* (d: LONGINT);
  BEGIN Kernel.SetClock(d)
  END SetClock;

  PROCEDURE Time*(): LONGINT;
  BEGIN RETURN Kernel.Time()
  END Time;

  (*log management*)

  PROCEDURE OutputLog(T: Texts.Text; op: INTEGER; beg, end: LONGINT);
    VAR R: Texts.Reader;
      B: Texts.Buffer;
      ch: CHAR;
  BEGIN
    IF op = Texts.insert THEN
      Texts.OpenReader(R, T, beg);
      WHILE beg # end DO
        Texts.Read(R, ch);
        IF ch = 0DX THEN ch := 0AX END;
        RS232.Send(ORD(ch));
        INC(beg)
      END;
      NEW(B); Texts.OpenBuf(B); Texts.Delete(T, beg, end, B)
    END
  END OutputLog;

  PROCEDURE OpenLog*;
  BEGIN NEW(Log); Log.notify := OutputLog; Texts.Open(Log, "")
  END OpenLog;

  (*command interpretation*)

  PROCEDURE SetPar*(T: Texts.Text; pos: LONGINT);
  BEGIN Par.text := T; Par.pos := pos
  END SetPar;

  PROCEDURE Call* (name: ARRAY OF CHAR; VAR res: INTEGER);
    VAR mod: Modules.Module; P: Modules.Command;
      i, j: INTEGER; ch: CHAR;
      Mname, Cname: ARRAY 32 OF CHAR;
  BEGIN i := 0; ch := name[0];
    WHILE (ch # ".") & (ch # 0X) DO Mname[i] := ch; INC(i); ch := name[i] END ;
    IF ch = "." THEN
      Mname[i] := 0X; INC(i);
      Modules.Load(Mname, mod); res := Modules.res;
      IF Modules.res = 0 THEN
        j := 0; ch := name[i]; INC(i);
        WHILE ch # 0X DO Cname[j] := ch; INC(j); ch := name[i]; INC(i) END ;
        Cname[j] := 0X;
        P := Modules.ThisCommand(mod, Cname); res := Modules.res;
        IF Modules.res = 0 THEN P END
      END
    ELSE res := 5
    END
  END Call;

  PROCEDURE GetSelection* (VAR text: Texts.Text; VAR beg, end, time: LONGINT);
  BEGIN time := -1
  END GetSelection;

  PROCEDURE GC;
    VAR mod: Modules.Module;
  BEGIN
    IF (ActCnt = 0) OR (Kernel.allocated >= Kernel.heapLim - Kernel.heapOrg - 10000H) THEN
      mod := Modules.root; LED(21H);
      WHILE mod # NIL DO
        IF mod.name[0] # 0X THEN Kernel.Mark(mod.ptr) END ;
        mod := mod.next
      END ;
      LED(23H);
      Files.RestoreList; LED(27H);
      Kernel.Scan; LED(20H);
      ActCnt := BasicCycle
    END
  END GC;

  PROCEDURE NewTask*(h: Handler; period: INTEGER): Task;
    VAR t: Task;
  BEGIN NEW(t); t.state := off; t.next := t; t.handle := h; t.period := period; RETURN t
  END NewTask;

  PROCEDURE Install* (T: Task);
  BEGIN
    IF T.state = off THEN
      T.next := CurTask.next; CurTask.next := T; T.state := idle; T.nextTime := 0; INC(NofTasks)
    END
  END Install;

  PROCEDURE Remove* (T: Task);
    VAR t: Task;
  BEGIN
    IF T.state # off THEN t := T;
      WHILE t.next # T DO t := t.next END ;
      t.next := T.next; T.state := off; T.next := NIL; CurTask := t; DEC(NofTasks)
    END
  END Remove;

  PROCEDURE Collect* (count: INTEGER);
  BEGIN ActCnt := count
  END Collect;

  PROCEDURE Loop*;
    VAR t: INTEGER;
  BEGIN
    REPEAT
      CurTask := CurTask.next; t := Kernel.Time();
      IF t >= CurTask.nextTime THEN
        CurTask.nextTime := t + CurTask.period; CurTask.state := active; CurTask.handle; CurTask.state := idle
      END
    UNTIL FALSE
  END Loop;

  PROCEDURE Reset*;
  BEGIN
    IF CurTask.state = active THEN Remove(CurTask) END ;
    SYSTEM.LDREG(14, Kernel.stackOrg); (*reset stack pointer*) Loop
  END Reset;

  PROCEDURE Ignore(T: Texts.Text; op: INTEGER; beg, end: LONGINT);
  END Ignore;

  PROCEDURE ParamCall*;
    VAR p: ARRAY 100 OF CHAR;
      W: Texts.Writer;
      i, c, res: INTEGER;
  BEGIN Texts.OpenWriter(W); c := Norebo.ParamCount();
    FOR i := 1 TO c-1 DO
      Norebo.ParamStr(i, p);
      IF i # 1 THEN Texts.Write(W, " ") END;
      Texts.WriteString(W, p);
    END;
    NEW(Par.text); Texts.Open(Par.text, ""); Par.text.notify := Ignore;
    Texts.Append(Par.text, W.buf); Par.pos := 0;
    Norebo.ParamStr(0, p); Call(p, res); Norebo.Halt(res)
  END ParamCall;

  PROCEDURE Trap(VAR a: INTEGER; b: INTEGER);
    VAR u, v, w, pos, name: INTEGER; mod: Modules.Module;
  BEGIN u := SYSTEM.REG(15); SYSTEM.GET(u - 4, v); w := v DIV 10H MOD 10H; (*trap number*)
    IF w = 0 THEN Kernel.New(a, b)
    ELSE (*trap*) pos := v DIV 100H MOD 10000H; mod := Modules.root;
      WHILE (mod # NIL) & ((u < mod.code) OR (u >= mod.imp)) DO mod := mod.next END ;
      IF mod # NIL THEN name := SYSTEM.ADR(mod.name) ELSE name := 0 END ;
      Norebo.Trap(w, name, pos)
    END
  END Trap;

BEGIN 
  Kernel.Install(SYSTEM.ADR(Trap), 20H);
  User[0] := 0X; ActCnt := 0; CurTask := NewTask(GC, 1000); Install(CurTask);
  OpenLog; ParamCall;
END Oberon.
