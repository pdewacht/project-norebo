MODULE Norebo;
  IMPORT SYSTEM;

  CONST sysreq = -4; sysarg1 = -8; sysarg2 = -12; sysarg3 = -16;
    noreboHalt* = 1;
    noreboArgc* = 2;
    noreboArgv* = 3;
    filesNew* = 11;
    filesOld* = 12;
    filesRegister* = 13;
    filesClose* = 14;
    filesSeek* = 15;
    filesTell* = 16;
    filesRead* = 17;
    filesWrite* = 18;
    filesLength* = 19;
    filesDate* = 20;
    filesDelete* = 21;
    filesPurge* = 22;
    filesRename* = 23;
    filedirEnumerateBegin* = 31;
    filedirEnumerateNext* = 32;
    filedirEnumerateEnd* = 33;

  VAR res*: INTEGER;

  PROCEDURE SysReq*(req, arg1, arg2, arg3: INTEGER);
  BEGIN
    SYSTEM.PUT(sysarg1, arg1);
    SYSTEM.PUT(sysarg2, arg2);
    SYSTEM.PUT(sysarg3, arg3);
    SYSTEM.PUT(sysreq, req);
    SYSTEM.GET(sysreq, res)
  END SysReq;

  PROCEDURE Halt*(exitcode: INTEGER);
  BEGIN SysReq(noreboHalt, exitcode, 0, 0)
  END Halt;

  PROCEDURE ParamCount*(): INTEGER;
  BEGIN SysReq(noreboArgc, 0, 0, 0)
    RETURN res
  END ParamCount;

  PROCEDURE ParamStr*(n: INTEGER; VAR param: ARRAY OF CHAR);
  BEGIN SysReq(noreboArgv, n, SYSTEM.ADR(param), LEN(param))
  END ParamStr;

END Norebo.
