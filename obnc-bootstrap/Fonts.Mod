MODULE Fonts; (*JG 18.11.90; PDR 8.6.12; NW 25.3.2013*)

  TYPE Font* = POINTER TO FontDesc;
    FontDesc* = RECORD
      name*: ARRAY 32 OF CHAR;
      next*: Font
    END;

VAR Default*, root*: Font;

PROCEDURE This*(name: ARRAY OF CHAR): Font;
  VAR F: Font;
BEGIN F := root;
  WHILE (F # NIL) & (name # F.name) DO F := F.next END;
  IF F = NIL THEN
    NEW(F); F.name := name;
  END;
  RETURN F
END This;

BEGIN root := NIL; Default := This("Oberon10.Scn.Fnt")
END Fonts.
