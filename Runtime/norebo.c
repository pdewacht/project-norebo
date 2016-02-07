#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include "risc-cpu.h"

#define PathEnv "NOREBO_PATH"
#define InnerCore "InnerCore"

#define MemBytes (8 * 1024 * 1024)
#define MemWords (MemBytes / 4)
#define StackOrg 0x80000
#define MaxFiles 500
#define NameLength 32

struct File {
  FILE *f;
  char name[NameLength];
  bool registered;
};

static uint32_t mem[MemWords];
static uint32_t sysarg[3], sysres;
static uint32_t nargc;
static char **nargv;
static struct File files[MaxFiles];
static DIR *dir;

/* Memory access */

static uint32_t mem_read_word(uint32_t adr) {
  if (adr / 4 >= MemWords) {
    errx(1, "Memory read out of bounds (address %#08x)", adr);
  }
  return mem[adr / 4];
}

static uint8_t mem_read_byte(uint32_t adr) {
  uint32_t w = mem_read_word(adr);
  return (uint8_t)(w >> (adr % 4 * 8));
}

static void mem_write_word(uint32_t adr, uint32_t val) {
  if (adr / 4 >= MemWords) {
    errx(1, "Memory write out of bounds (address %#08x)", adr);
  }
  mem[adr / 4] = val;
}

static void mem_write_byte(uint32_t adr, uint32_t val) {
  uint32_t w = mem_read_word(adr);
  uint32_t shift = (adr & 3) * 8;
  w &= ~(0xFFu << shift);
  w |= (uint32_t)val << shift;
  mem_write_word(adr, w);
}

static void mem_check_range(uint32_t adr, uint32_t siz, const char *proc) {
  if (adr >= MemBytes || MemBytes - adr < siz) {
    errx(1, "%s: Memory access out of bounds", proc);
  }
}

/* Norebo module */

static uint32_t norebo_halt(uint32_t ec, uint32_t _2, uint32_t _3) {
  exit(ec);
}

static uint32_t norebo_argc(uint32_t _1, uint32_t _2, uint32_t _3) {
  return nargc;
}

static uint32_t norebo_argv(uint32_t idx, uint32_t adr, uint32_t siz) {
  mem_check_range(adr, siz, "Norebo.Argv");
  if (idx < nargc) {
    if (siz > 0) {
      strncpy((char *)mem + adr, nargv[idx], siz - 1);
      ((char *)mem)[adr + siz - 1] = 0;
    }
    return (uint32_t)strlen(nargv[idx]);
  } else {
    return -1;
  }
}

static bool files_get_name(char *name, uint32_t adr);

static uint32_t norebo_trap(uint32_t trap, uint32_t name_adr, uint32_t pos) {
  char message[100];
  switch (trap) {
    case 1: strcpy(message, "array index out of range"); break;
    case 2: strcpy(message, "type guard failure"); break;
    case 3: strcpy(message, "array or string copy overflow"); break;
    case 4: strcpy(message, "access via NIL pointer"); break;
    case 5: strcpy(message, "illegal procedure call"); break;
    case 6: strcpy(message, "integer division by zero"); break;
    case 7: strcpy(message, "assertion violated"); break;
    default: sprintf(message, "unknown trap %d", trap); break;
  }
  char name[NameLength];
  if (!files_get_name(name, name_adr)) {
    strcpy(name, "(unknown)");
  }
  errx(100 + trap, "%s at %s pos %d", message, name, pos);
}

/* Files module */

static FILE *path_fopen(const char *path, const char *filename, const char *mode) {
  if (!path) {
    errno = ENOENT;
    return NULL;
  }
  const char *sep = strchr(path, ';') ? ";" : ":";
  FILE *f = NULL;
  do {
    size_t part_len = strcspn(path, sep);
    if (part_len == 0) {
      f = fopen(filename, mode);
    } else {
      char *buf = NULL;
      int r = asprintf(&buf, "%.*s/%s", (int)part_len, path, filename);
      if (r < 0) {
        err(1, NULL);
      }
      f = fopen(buf, mode);
      free(buf);
    }
    path += part_len + 1;
  } while (f == NULL && errno == ENOENT && path[-1] != 0);
  return f;
}

static bool files_check_name(char *name) {
  for (int i = 0; i < NameLength; ++i) {
    char ch = name[i];
    if (ch == 0) {
      return true;
    } else if (! ((ch >= 'A' && ch <= 'Z') ||
                  (ch >= 'a' && ch <= 'z') ||
                  (i > 0 && (ch == '.' || (ch >= '0' && ch <= '9'))))) {
      return false;
    }
  }
  return false;
}

static bool files_get_name(char *name, uint32_t adr) {
  mem_check_range(adr, NameLength, "Files.GetName");
  memcpy(name, (char *)mem + adr, NameLength);
  return files_check_name(name);
}

static int files_allocate(const char *name, bool registered) {
  for (int h = 0; h < MaxFiles; ++h) {
    if (!files[h].f) {
      strncpy(files[h].name, name, NameLength);
      files[h].registered = registered;
      return h;
    }
  }
  errx(1, "Files.Allocate: Too many open files");
}

static void files_check_handle(int h, const char *proc) {
  if (h < 0 || h >= MaxFiles || !files[h].f) {
    errx(1, "%s: Invalid file handle", proc);
  }
}

static uint32_t files_new(uint32_t adr, uint32_t _2, uint32_t _3) {
  char name[NameLength];
  if (!files_get_name(name, adr)) {
    return -1;
  }
  int h = files_allocate(name, false);
  files[h].f = tmpfile();
  if (!files[h].f) {
    err(1, "Files.New: %s", name);
  }
  return h;
}

static uint32_t files_old(uint32_t adr, uint32_t _2, uint32_t _3) {
  char name[NameLength];
  if (!files_get_name(name, adr)) {
    return -1;
  }
  int h = files_allocate(name, true);
  files[h].f = fopen(name, "r+b");
  if (!files[h].f) {
    files[h].f = path_fopen(getenv(PathEnv), name, "rb");
  }
  if (!files[h].f) {
    files[h] = (struct File){0};
    return -1;
  }
  return h;
}

static uint32_t files_register(uint32_t h, uint32_t _2, uint32_t _3) {
  files_check_handle(h, "Files.Register");
  if (!files[h].registered && files[h].name[0]) {
    FILE *old = files[h].f;
    files[h].f = fopen(files[h].name, "w+b");
    if (!files[h].f) {
      err(1, "Can't create file %s", files[h].name);
    }
    errno = 0;
    fseek(old, 0, SEEK_SET);
    char buf[8192];
    size_t in = fread(buf, 1, sizeof(buf), old);
    while (in != 0) {
      size_t out = fwrite(buf, 1, in, files[h].f);
      if (in != out) {
        err(1, "Can't write file %s", files[h].name);
      }
      in = fread(buf, 1, sizeof(buf), old);
    }
    fclose(old);
    if (fflush(files[h].f) != 0) {
      err(1, "Can't flush file %s", files[h].name);
    }
    files[h].registered = true;
  }
  return 0;
}

static uint32_t files_close(uint32_t h, uint32_t _2, uint32_t _3) {
  files_check_handle(h, "Files.Close");
  fclose(files[h].f);
  files[h] = (struct File){0};
  return 0;
}

static uint32_t files_seek(uint32_t h, uint32_t pos, uint32_t whence) {
  files_check_handle(h, "Files.Seek");
  return fseek(files[h].f, pos, whence);
}

static uint32_t files_tell(uint32_t h, uint32_t _2, uint32_t _3) {
  files_check_handle(h, "Files.Tell");
  return (uint32_t)ftell(files[h].f);
}

static uint32_t files_read(uint32_t h, uint32_t adr, uint32_t siz) {
  files_check_handle(h, "Files.Read");
  mem_check_range(adr, siz, "Files.Read");
  size_t r = fread((char *)mem + adr, 1, siz, files[h].f);
  memset((char *)mem + adr + r, 0, siz - r);
  return (uint32_t)r;
}

static uint32_t files_write(uint32_t h, uint32_t adr, uint32_t siz) {
  files_check_handle(h, "Files.Write");
  mem_check_range(adr, siz, "Files.Write");
  return (uint32_t)fwrite((char *)mem + adr, 1, siz, files[h].f);
}

static uint32_t files_length(uint32_t h, uint32_t _2, uint32_t _3) {
  files_check_handle(h, "Files.Length");
  fflush(files[h].f);
  struct stat s;
  int r = fstat(fileno(files[h].f), &s);
  if (r < 0) { err(1, "Files.Length"); }
  return (uint32_t)s.st_size;
}

static uint32_t time_to_oberon(time_t t) {
  struct tm tm = {0};
  localtime_r(&t, &tm);
  return ((tm.tm_year % 100) * 0x4000000) |
    (tm.tm_mon * 0x400000) |
    (tm.tm_mday * 0x20000) |
    (tm.tm_hour * 0x1000) |
    (tm.tm_min * 0x40) |
    tm.tm_sec;
}

static uint32_t files_date(uint32_t h, uint32_t _2, uint32_t _3) {
  files_check_handle(h, "Files.Date");
  fflush(files[h].f);
  if (files[h].registered) {
    struct stat s;
    int r = fstat(fileno(files[h].f), &s);
    if (r < 0) { err(1, "Files.Date"); }
    return time_to_oberon(s.st_mtime);
  } else {
    return time_to_oberon(time(NULL));
  }
}

static uint32_t files_delete(uint32_t adr, uint32_t _2, uint32_t _3) {
  char name[NameLength];
  if (!files_get_name(name, adr) || !name[0]) {
    return -1;
  }
  if (remove(name) < 0) {
    return -1;
  }
  return 0;
}

static uint32_t files_purge(uint32_t h, uint32_t _2, uint32_t _3) {
  errx(1, "Files.Purge not implemented");
}

static uint32_t files_rename(uint32_t adr_old, uint32_t adr_new, uint32_t _3) {
  char old_name[NameLength], new_name[NameLength];
  if (!files_get_name(old_name, adr_old) || !old_name[0] ||
      !files_get_name(new_name, adr_new) || !new_name[0]) {
    return -1;
  }
  if (rename(old_name, new_name) < 0) {
    return -1;
  }
  return 0;
}

/* FileDir module */

static uint32_t filedir_enumerate_begin(uint32_t _1, uint32_t _2, uint32_t _3) {
  if (dir) {
    closedir(dir);
  }
  dir = opendir(".");
  if (!dir) {
    err(1, "FileDir.BeginEnumerate");
  }
  return 0;
}

static uint32_t filedir_enumerate_next(uint32_t adr, uint32_t _2, uint32_t _3) {
  mem_check_range(adr, NameLength, "FileDir.EnumerateNext");
  struct dirent *ent = NULL;
  if (dir) {
    do {
      ent = readdir(dir);
    } while (ent && !files_check_name(ent->d_name));
  }
  if (!ent) {
    mem_write_byte(adr, 0);
    return -1;
  }
  strncpy((char *)mem + adr, ent->d_name, NameLength);
  return 0;
}

static uint32_t filedir_enumerate_end(uint32_t _1, uint32_t _2, uint32_t _3) {
  if (dir) {
    closedir(dir);
    dir = NULL;
  }
  return 0;
}

/* I/O dispatch */

typedef uint32_t (* sysreq_fn)(uint32_t, uint32_t, uint32_t);

static sysreq_fn sysreq_table[] = {
  [ 1] = norebo_halt,
  [ 2] = norebo_argc,
  [ 3] = norebo_argv,
  [ 4] = norebo_trap,

  [11] = files_new,
  [12] = files_old,
  [13] = files_register,
  [14] = files_close,
  [15] = files_seek,
  [16] = files_tell,
  [17] = files_read,
  [18] = files_write,
  [19] = files_length,
  [20] = files_date,
  [21] = files_delete,
  [22] = files_purge,
  [23] = files_rename,

  [31] = filedir_enumerate_begin,
  [32] = filedir_enumerate_next,
  [33] = filedir_enumerate_end,
};

static const uint32_t sysreq_cnt = sizeof(sysreq_table) / sizeof(sysreq_table[0]);

static uint32_t sysreq_exec(uint32_t n) {
  if (n >= sysreq_cnt || !sysreq_table[n]) {
    errx(1, "Unimplemented sysreq %d\n", n);
  }
  return sysreq_table[n](sysarg[0], sysarg[1], sysarg[2]);
}

static uint32_t risc_time(void) {
  struct timeval tv = {0};
  gettimeofday(&tv, NULL);
  return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void risc_leds(uint32_t n) {
  static char buf[] = "[LEDs: 76543210]\n";
  for (int i = 0; i < 8; ++i) {
    buf[14 - i] = (n & (1 << i)) ? (char)('0' + i) : '-';
  }
  fputs(buf, stderr);
}

static uint32_t io_read_word(uint32_t adr) {
  switch (-adr / 4) {
  /* carried over from oberon */
  case 64/4:
    return risc_time();
  case 56/4:
    return getchar();
  case 52/4:
    return 3;
  /* norebo interface */
  case 16/4:
    return sysarg[2];
  case 12/4:
    return sysarg[1];
  case 8/4:
    return sysarg[0];
  case 4/4:
    return sysres;
  default:
    errx(1, "Unimplemented read of I/O address %d", adr);
  }
}

static void io_write_word(uint32_t adr, uint32_t val) {
  switch (-adr / 4) {
  /* carried over from oberon */
  case 60/4:
    risc_leds(val);
    break;
  case 56/4:
    putchar(val);
    break;
  /* norebo interface */
  case 16/4:
    sysarg[2] = val;
    break;
  case 12/4:
    sysarg[1] = val;
    break;
  case 8/4:
    sysarg[0] = val;
    break;
  case 4/4:
    sysres = sysreq_exec(val);
    //printf("%d(%d,%d,%d)=>%d\n",val,sysarg[0],sysarg[1],sysarg[2],sysres);
    break;
  default:
    errx(1, "Unimplemented write of I/O address %d", adr);
  }
}

/* CPU glue */

static uint32_t cpu_read_program(struct RISC *cpu, uint32_t adr) {
  return mem_read_word(adr * 4);
}

static uint32_t cpu_read_word(struct RISC *cpu, uint32_t adr) {
  return (int32_t)adr >= 0 ? mem_read_word(adr) : io_read_word(adr);
}

static uint32_t cpu_read_byte(struct RISC *cpu, uint32_t adr) {
  return (int32_t)adr >= 0 ? mem_read_byte(adr) : io_read_word(adr);
}

static void cpu_write_word(struct RISC *cpu, uint32_t adr, uint32_t val) {
  (int32_t)adr >= 0 ? mem_write_word(adr, val) : io_write_word(adr, val);
}

static void cpu_write_byte(struct RISC *cpu, uint32_t adr, uint32_t val) {
  (int32_t)adr >= 0 ? mem_write_byte(adr, val) : io_write_word(adr, val);
}

/* Boot */

static void load_inner_core(void) {
  FILE *f = fopen(InnerCore, "rb");
  if (!f) {
    f = path_fopen(getenv(PathEnv), InnerCore, "rb");
  }
  if (!f) {
    err(1, "Can't load " InnerCore);
  }

  uint32_t siz, adr;
  if (fread(&siz, 1, 4, f) != 4) {
    goto fail;
  }
  while (siz != 0) {
    if (fread(&adr, 1, 4, f) != 4) {
      goto fail;
    }
    mem_check_range(adr, siz, InnerCore);
    if (fread((char *)mem + adr, 1, siz, f) != siz) {
      goto fail;
    }
    if (fread(&siz, 1, 4, f) != 4) {
      goto fail;
    }
  }
  fclose(f);
  return;

 fail:
  if (feof(f)) {
    errx(1, "Unexpected end of file while reading " InnerCore);
  }
  err(1, "Error while reading " InnerCore);
}

int main(int argc, char *argv[]) {
  nargc = argc - 1;
  nargv = argv + 1;

  load_inner_core();
  mem_write_word(12, MemBytes);
  mem_write_word(24, StackOrg);
  struct RISC cpu = {
    .read_program = cpu_read_program,
    .read_word = cpu_read_word,
    .read_byte = cpu_read_byte,
    .write_word = cpu_write_word,
    .write_byte = cpu_write_byte,
    .PC = 0,
    .R[12] = 0x20,
    .R[14] = StackOrg,
  };
  risc_run(&cpu);
  return 0;
}
