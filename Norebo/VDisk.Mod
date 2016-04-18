MODULE VDisk;  (*derived from Kernel.Mod NW/PR  11.4.86 / 27.12.95 / 4.2.2014*)
  IMPORT SYSTEM, Files, Texts, Oberon;

  (* Note: On a standard PO2013 system, the maximum file size is not
     much more than three megabyte. This module is not very useful in
     such an environment. *)

  CONST SectorLength* = 1024;
    mapsize = 10000H; (*1K sectors, 64MB*)

  TYPE Sector* = ARRAY SectorLength OF BYTE;
     VDisk* = POINTER TO VDiskDesc;
     VDiskDesc* = RECORD
       file*: Files.File;
       NofSectors*: INTEGER;
       sectorMap: ARRAY mapsize DIV 32 OF SET;
     END;

  VAR W: Texts.Writer;

  PROCEDURE nl; BEGIN Texts.WriteLn(W); Texts.Append(Oberon.Log, W.buf) END nl;

  PROCEDURE InitSecMap*(V: VDisk);
    VAR i: INTEGER;
  BEGIN V.NofSectors := 0; V.sectorMap[0] := {0 .. 31}; V.sectorMap[1] := {0 .. 31};
    FOR i := 2 TO mapsize DIV 32 - 1 DO V.sectorMap[i] := {} END
  END InitSecMap;

  PROCEDURE MarkSector*(V: VDisk; sec: INTEGER);
  BEGIN sec := sec DIV 29; ASSERT(SYSTEM.H(0) = 0);
    INCL(V.sectorMap[sec DIV 32], sec MOD 32); INC(V.NofSectors)
  END MarkSector;

  PROCEDURE FreeSector*(V: VDisk; sec: INTEGER);
  BEGIN sec := sec DIV 29; ASSERT(SYSTEM.H(0) = 0);
    EXCL(V.sectorMap[sec DIV 32], sec MOD 32); DEC(V.NofSectors)
  END FreeSector;

  PROCEDURE AllocSector*(V: VDisk; hint: INTEGER; VAR sec: INTEGER);
    VAR s: INTEGER;
  BEGIN (*find free sector, starting after hint*)
    hint := hint DIV 29; ASSERT(SYSTEM.H(0) = 0); s := hint;
    REPEAT INC(s);
      IF s = mapsize THEN s := 1 END ;
    UNTIL ~(s MOD 32 IN V.sectorMap[s DIV 32]);
    INCL(V.sectorMap[s DIV 32], s MOD 32); INC(V.NofSectors); sec := s * 29
  END AllocSector;

  PROCEDURE GetSector*(V: VDisk; src: INTEGER; VAR dst: Sector);
    VAR R: Files.Rider;
      i: INTEGER;
  BEGIN src := src DIV 29; ASSERT(SYSTEM.H(0) = 0);
    src := (src - 1) * SectorLength;
    IF src < Files.Length(V.file) THEN
      Files.Set(R, V.file, src);
      Files.ReadBytes(R, dst, SectorLength)
    ELSE
      FOR i := 0 TO SectorLength-1 DO dst[i] := 0 END
    END
  END GetSector;

  PROCEDURE PutSector*(V: VDisk; dst: INTEGER; VAR src: Sector);
    VAR R: Files.Rider;
      i: INTEGER;
  BEGIN dst := dst DIV 29; ASSERT(SYSTEM.H(0) =  0);
    dst := (dst - 1) * SectorLength;
    Files.Set(R, V.file, dst);
    i := Files.Pos(R);
    WHILE i < dst DO Files.WriteByte(R, 0); INC(i); END;
    Files.WriteBytes(R, src, SectorLength)
  END PutSector;

  (* TODO: ugh, needs initialization from VFileDir *)
  PROCEDURE Open*(VAR V: VDisk; F: Files.File);
  BEGIN NEW(V); V.file := F;
    InitSecMap(V)
  END Open;

BEGIN Texts.OpenWriter(W)
END VDisk.
