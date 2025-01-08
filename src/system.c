/****************************************************************************
**
**  This file is part of GAP, a system for computational discrete algebra.
**
**  Copyright of GAP belongs to its developers, whose names are too numerous
**  to list here. Please refer to the COPYRIGHT file for details.
**
**  SPDX-License-Identifier: GPL-2.0-or-later
**
**  The  files   "system.c" and  "sysfiles.c"  contains all  operating system
**  dependent  functions.  This file contains  all system dependent functions
**  except file and stream operations, which are implemented in "sysfiles.c".
**  The following labels determine which operating system is actually used.
*/

#include "system.h"

#include "gaputils.h"
#ifdef GAP_MEM_CHECK
#include "gasman_intern.h"
#endif
#include "profile.h"
#include "sysfiles.h"
#include "sysopt.h"
#include "sysroots.h"
#include "sysstr.h"
#include "version.h"

#ifdef HPCGAP
#include "hpc/cpu.h"
#include "hpc/misc.h"
#endif

#if defined(USE_GASMAN)
#include "sysmem.h"
#elif defined(USE_JULIA_GC)
#include "julia.h"
#elif defined(USE_BOEHM_GC)
#include "boehm_gc.h"
#endif

#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>

#if defined(__APPLE__) && defined(__MACH__)
// Workaround: TRUE / FALSE are also defined by the macOS Mach-O headers
#define ENUM_DYLD_BOOL
#include <mach-o/dyld.h>
#endif

/****************************************************************************
**
*F * * * * * * * * * * * command line settable options  * * * * * * * * * * *
*/

/****************************************************************************
**
*V  SyCTRD  . . . . . . . . . . . . . . . . . . .  true if '<ctr>-D' is <eof>
*/
UInt SyCTRD;


/****************************************************************************
**
*V  SyCompilePlease . . . . . . . . . . . . . . .  tell GAP to compile a file
*V  SyCompileOutput . . . . . . . . . . . . . . . . . . into this output file
*V  SyCompileInput  . . . . . . . . . . . . . . . . . .  from this input file
*V  SyCompileName . . . . . . . . . . . . . . . . . . . . . .  with this name
*V  SyCompileMagic1 . . . . . . . . . . . . . . . . . . and this magic string
*/
BOOL SyCompilePlease;
const char * SyCompileOutput;
const char * SyCompileInput;
const char * SyCompileName;
const char * SyCompileMagic1;


/****************************************************************************
**
*V  SyDebugLoading  . . . . . . . . .  output messages about loading of files
*/
Int SyDebugLoading;


/****************************************************************************
**
*V  DotGapPath
**
**  The path to the user's ~/.gap directory, if available.
*/
static Char DotGapPath[GAP_PATH_MAX];


/****************************************************************************
**
*V  IgnoreGapRC . . . . . . . . . . . . . . . . . . . -r option for kernel
**
*/
static Int IgnoreGapRC;

/****************************************************************************
**
*V  SyLineEdit  . . . . . . . . . . . . . . . . . . . .  support line editing
**
**  0: no line editing
**  1: line editing if terminal
**  2: always line editing (EMACS)
*/
UInt SyLineEdit;

/****************************************************************************
**
*V  SyUseReadline   . . . . . . . . . . . . . . . . . .  support line editing
**
**  Switch for not using readline although GAP is compiled with libreadline
*/
UInt SyUseReadline;

/****************************************************************************
**
*V  SyNrCols  . . . . . . . . . . . . . . . . . .  length of the output lines
**
**  'SyNrCols' is the length of the lines on the standard output  device.
**
**  Per default this is 80 characters which is the usual width of  terminals.
**  It can be changed by the '-x' options for larger terminals  or  printers.
**
**  'Pr' uses this to decide where to insert a <newline> on the output lines.
**  'SyRead' uses it to decide when to start scrolling the echoed input line.
**
**  See also getwindowsize() below.
**
**  Put in this package because the command line processing takes place here.
*/
UInt SyNrCols;
UInt SyNrColsLocked;

/****************************************************************************
**
*V  SyNrRows  . . . . . . . . . . . . . . . . . number of lines on the screen
**
**  'SyNrRows' is the number of lines on the standard output device.
**
**  Per default this is 24, which is the  usual  size  of  terminal  screens.
**  It can be changed with the '-y' option for larger terminals or  printers.
**
**  'SyHelp' uses this to decide where to stop with '-- <space> for more --'.
**
**  See also getwindowsize() below.
*/
UInt SyNrRows;
UInt SyNrRowsLocked;

/****************************************************************************
**
*V  SyQuiet . . . . . . . . . . . . . . . . . . . . . . . . . suppress prompt
**
**  'SyQuiet' determines whether GAP should print the prompt and the  banner.
**
**  Per default its false, i.e. GAP prints the prompt and  the  nice  banner.
**  It can be changed by the '-q' option to have GAP operate in silent  mode.
**
**  It is used by the functions in 'gap.c' to suppress printing the  prompts.
**
**  Put in this package because the command line processing takes place here.
*/
UInt SyQuiet;

/****************************************************************************
**
*V  SyQuitOnBreak . . . . . . . . . . exit GAP instead of entering break loop
*/
UInt SyQuitOnBreak;

/****************************************************************************
**
*V  SyRestoring . . . . . . . . . . . . . . . . . . . . restoring a workspace
**
**  `SyRestoring' determines whether GAP is restoring a workspace or not.  If
**  it is zero no restoring should take place otherwise it holds the filename
**  of a workspace to restore.
**
*/
#ifdef GAP_ENABLE_SAVELOAD
Char * SyRestoring;
#endif


/****************************************************************************
**
*V  SyInitializing                               set to 1 during library init
**
**  `SyInitializing' is set to 1 during the library initialization phase of
**  startup. It suppresses some behaviours that may not be possible so early
**  such as homogeneity tests in the plist code.
*/

UInt SyInitializing;


/****************************************************************************
**
*V  SyUseModule . . . . . . . . . check for static modules in 'READ_GAP_ROOT'
*/
int SyUseModule;


/****************************************************************************
**
*V  SyWindow  . . . . . . . . . . . . . . . .  running under a window handler
**
**  'SyWindow' is 1 if GAP  is running under  a window handler front end such
**  as 'xgap', and 0 otherwise.
**
**  If running under  a window handler front  end, GAP adds various  commands
**  starting with '@' to the output to let 'xgap' know what is going on.
*/
UInt SyWindow;


/****************************************************************************
**
*F * * * * * * * * * * * * * initialize module * * * * * * * * * * * * * * *
*/


/****************************************************************************
**
*F  SyExit( <ret> ) . . . . . . . . . . . . . exit GAP with return code <ret>
**
**  'SyExit' is the official way  to exit GAP, bus errors are the unofficial.
**  The function 'SyExit' must perform all the necessary cleanup operations.
**  If ret is 0 'SyExit' should signal to a calling process that all is  ok.
**  If ret is 1 'SyExit' should signal a  failure  to  the  calling process.
*/
void SyExit(UInt ret)
{
#ifdef USE_JULIA_GC
    jl_atexit_hook(ret);
#endif
    exit((int)ret);
}


/****************************************************************************
**
*F  Panic( <msg> )
*/
void Panic_(const char * file, int line, const char * fmt, ...)
{
    fprintf(stderr, "Panic in %s:%d: ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end (args);
    fputs("\n", stderr);
    SyExit(1);
}


/****************************************************************************
**
*F  SyDotGapPath()
*/
const Char * SyDotGapPath(void)
{
    return DotGapPath;
}


/****************************************************************************
**
*F  InitSystem( <argc>, <argv> )  . . . . . . . . . initialize system package
**
**  'InitSystem' is called very early during the initialization from  'main'.
**  It is passed the command line array  <argc>, <argv>  to look for options.
**
**  For UNIX it initializes the default files 'stdin', 'stdout' and 'stderr',
**  installs the handler 'syAnswerIntr' to answer the user interrupts
**  '<ctr>-C', scans the command line for options, sets up the GAP root paths,
**  locates the '.gaprc' file (if any), and more.
*/

#if defined(USE_GASMAN) || defined(USE_BOEHM_GC)
typedef struct { Char symbol; UInt value; } sizeMultiplier;

static sizeMultiplier memoryUnits[]= {
  {'k', 1024},
  {'K', 1024},
  {'m', 1024*1024},
  {'M', 1024*1024},
  {'g', 1024*1024*1024},
  {'G', 1024*1024*1024},
#ifdef SYS_IS_64_BIT
  {'t', 1024UL*1024*1024*1024},
  {'T', 1024UL*1024*1024*1024},
  {'p', 1024UL*1024*1024*1024*1024}, // you never know
  {'P', 1024UL*1024*1024*1024*1024},
#endif
};


#ifdef SYS_IS_64_BIT
static const UInt maxmem = 15000000000000000000UL;
#else
static const UInt maxmem = 4000000000UL;
#endif

static BOOL ParseMemory(const char * s, UInt * result)
{
    char * end;
    const double size = strtod(s, &end);
    const char symbol = end[0];

    if (s == end)
        goto err;

    // if no unit was specified: return immediately
    if (symbol == 0) {
        *result = size;
        return TRUE;
    }

    // units consist of a single character; reject if there is more
    if (end[1] != 0)
        goto err;

    for (int i = 0; i < ARRAY_SIZE(memoryUnits); i++) {
        if (symbol == memoryUnits[i].symbol) {
            UInt value = memoryUnits[i].value;
            if (size > maxmem / value)
                *result = maxmem;
            else
                *result = size * value;
            return TRUE;
        }
    }

err:
    fputs("Unrecognized memory size '", stderr);
    fputs(s, stderr);
    fputs("'\n", stderr);
    return FALSE;
}

#endif

static void usage(void)
{
    fputs("usage: gap [OPTIONS] [FILES]\n", stderr);
    fputs("       use '-h' option to get help.\n", stderr);
    fputs("\n", stderr);
    SyExit(1);
}

struct optInfo {
  Char shortkey;
  Char longkey[50];
  int (*handler)(const char * argv[], void *);
  void *otherArg;
  UInt minargs;
};


static int toggle(const char * argv[], void * Variable)
{
  UInt * variable = (UInt *) Variable;
  *variable = !*variable;
  return 0;
}

#ifdef HPCGAP
static int storePosInteger(const char * argv[], void * Where)
{
  UInt *where = (UInt *)Where;
  UInt n;
  const char *p = argv[0];
  n = 0;
  while (IsDigit(*p)) {
    n = n * 10 + (*p-'0');
    p++;
  }
  if (p == argv[0] || *p || n == 0) {
      fputs("Argument not a positive integer\n", stderr);
      usage();
  }
  *where = n;
  return 1;
}
#endif

#ifdef GAP_ENABLE_SAVELOAD
static int storeString(const char * argv[], void * Where)
{
    const char ** where = (const char **)Where;
    *where = argv[0];
    return 1;
}
#endif

#ifdef USE_GASMAN
static int storeMemory(const char * argv[], void * Where)
{
    UInt * where = (UInt *)Where;
    if (!ParseMemory(argv[0], where))
        usage();
    return 1;
}
#endif

#if defined(USE_GASMAN) || defined(USE_BOEHM_GC)
static int storeMemory2(const char * argv[], void * Where)
{
    UInt * where = (UInt *)Where;
    if (!ParseMemory(argv[0], where))
        usage();
    *where /= 1024;
    return 1;
}
#endif

static int processCompilerArgs(const char * argv[], void * dummy)
{
    SyCompilePlease = TRUE;
    SyCompileOutput = argv[0];
    SyCompileInput = argv[1];
    SyCompileName = argv[2];
    SyCompileMagic1 = argv[3];
    return 4;
}

#ifdef GAP_ENABLE_SAVELOAD
static int unsetString(const char * argv[], void * Where)
{
  *(Char **)Where = (Char *)0;
  return 0;
}
#endif

static int forceLineEditing(const char * argv[], void * Level)
{
  UInt level = (UInt)Level;
  SyLineEdit = level;
  return 0;
}

static int setGapRootPath(const char * argv[], void * Dummy)
{
  SySetGapRootPath( argv[0] );
  return 1;
}


#ifndef GAP_MEM_CHECK
// Provide stub with helpful error message

static int enableMemCheck(const char * argv[], void * dummy)
{
    fputs("# Error: --enableMemCheck not supported by this copy of GAP\n", stderr);
    fputs("  pass --enable-memory-checking to ./configure\n", stderr);
    SyExit(2);
}
#endif


static int printVersion(const char * argv[], void * dummy)
{
    fputs("GAP ", stdout);
    fputs(SyBuildVersion, stdout);
    fputs(" built on ", stdout);
    fputs(SyBuildDateTime, stdout);
    fputs("\n", stdout);
    SyExit(0);
}


// These are just the options that need kernel processing. Additional options
// will be recognised and handled in the library
//
// These options must be kept in sync with those in system.g, so the help
// output is correct
static const struct optInfo options[] = {
  { 'C',  "", processCompilerArgs, 0, 4}, // must handle in kernel
  { 'D',  "debug-loading", toggle, &SyDebugLoading, 0}, // must handle in kernel
#if defined(USE_GASMAN) || defined(USE_BOEHM_GC)
  { 'K',  "maximal-workspace", storeMemory2, &SyStorKill, 1}, // could handle from library with new interface
#endif
#ifdef GAP_ENABLE_SAVELOAD
  { 'L', "", storeString, &SyRestoring, 1}, // must be handled in kernel
  { 'R', "", unsetString, &SyRestoring, 0}, // kernel
#endif
  { 'M', "", toggle, &SyUseModule, 0}, // must be handled in kernel
  { 'e', "", toggle, &SyCTRD, 0 }, // kernel
  { 'f', "", forceLineEditing, (void *)2, 0 }, // probably library now
  { 'E', "", toggle, &SyUseReadline, 0 }, // kernel
  { 'l', "roots", setGapRootPath, 0, 1}, // kernel
#ifdef USE_GASMAN
  { 'm', "", storeMemory2, &SyStorMin, 1 }, // kernel
#endif
  { 'r', "", toggle, &IgnoreGapRC, 0 }, // kernel
#ifdef USE_GASMAN
  { 's', "", storeMemory, &SyAllocPool, 1 }, // kernel
#endif
  { 'n', "", forceLineEditing, 0, 0}, // prob library
#ifdef USE_GASMAN
  { 'o', "", storeMemory2, &SyStorMax, 1 }, // library with new interface
#endif
  { 'p', "", toggle, &SyWindow, 0 }, // ??
  { 'q', "", toggle, &SyQuiet, 0 }, // ??
#ifdef HPCGAP
  { 'S', "", toggle, &ThreadUI, 0 }, // Thread UI
  { 'Z', "", toggle, &DeadlockCheck, 0 }, // Deadlock prevention
  { 'P', "", storePosInteger, &SyNumProcessors, 1 }, // number of CPUs
  { 'G', "", storePosInteger, &SyNumGCThreads, 1 }, // number of GC threads
  { 0  , "single-thread", toggle, &SingleThreadStartup, 0 }, // startup with one thread only
#endif
  // The following options must be handled in the kernel so they are set up before loading the library
  { 0  , "prof", enableProfilingAtStartup, 0, 1},    // enable profiling at startup
  { 0  , "memprof", enableMemoryProfilingAtStartup, 0, 1 }, // enable memory profiling at startup
  { 0  , "cover", enableCodeCoverageAtStartup, 0, 1}, // enable code coverage at startup
  { 0  , "quitonbreak", toggle, &SyQuitOnBreak, 0}, // Quit GAP if we enter the break loop
  { 0  , "enableMemCheck", enableMemCheck, 0, 0 },
  { 0  , "version", printVersion, 0, 0 },
  { 0, "", 0, 0, 0}};


void InitSystem(int argc, const char * argv[], BOOL handleSignals)
{
    UInt                i;             // loop variable
    Int res;                       // return from option processing function

    // Initialize global and static variables
    SyCTRD = 1;
    SyCompilePlease = FALSE;
    SyDebugLoading = 0;
    SyLineEdit = 1;
#ifdef HAVE_LIBREADLINE
  #ifdef HPCGAP
    SyUseReadline = 0;
  #else
    SyUseReadline = 1;
  #endif
#endif
#ifdef HPCGAP
    SyNumProcessors = SyCountProcessors();
#endif
    SyNrCols = 0;
    SyNrColsLocked = 0;
    SyNrRows = 0;
    SyNrRowsLocked = 0;
    SyQuiet = 0;
    SyInitializing = 0;

#ifdef USE_GASMAN
    SyMsgsFlagBags = 0;
    SyStorMin = 16 * sizeof(Obj) * 1024;    // in kB
    SyStorMax = 256 * sizeof(Obj) * 1024;   // in kB
#ifdef SYS_IS_64_BIT
  #if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
    // Set to 3/4 of memory size (in kB), if this is larger
    Int SyStorMaxFromMem =
        (sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES) * 3) / 4 / 1024;
    SyStorMax = SyStorMaxFromMem > SyStorMax ? SyStorMaxFromMem : SyStorMax;
  #endif
#endif // defined(SYS_IS_64_BIT)

#ifdef SYS_IS_64_BIT
    SyAllocPool = 4096L*1024*1024;   // Note this is in bytes!
#else
    SyAllocPool = 1536L*1024*1024;   // Note this is in bytes!
#endif // defined(SYS_IS_64_BIT)
    SyStorOverrun = SY_STOR_OVERRUN_CLEAR;
    SyStorKill = 0;
#endif // defined(USE_GASMAN)
    SyUseModule = 1;
    SyWindow = 0;

    if (handleSignals) {
        SyInstallAnswerIntr();
    }

    // scan the command line for options that we have to process in the kernel
    // we just scan the whole command line looking for the keys for the options we recognise
    // anything else will presumably be dealt with in the library
    while ( argc > 1 )
      {
        if (argv[1][0] == '-' ) {

          if ( strlen(argv[1]) != 2 && argv[1][1] != '-') {
            fputs("gap: sorry, options must not be grouped '", stderr);
            fputs(argv[1], stderr);
            fputs("'.\n", stderr);
            usage();
          }


          for (i = 0;  options[i].shortkey != argv[1][1] &&
                       (argv[1][1] != '-' || argv[1][2] == 0 || strcmp(options[i].longkey, argv[1] + 2)) &&
                       (options[i].shortkey != 0 || options[i].longkey[0] != 0); i++)
            ;




          if (argc < 2 + options[i].minargs)
            {
              Char buf[2];
              fputs("gap: option ", stderr);
              fputs(argv[1], stderr);
              fputs(" requires at least ", stderr);
              buf[0] = options[i].minargs + '0';
              buf[1] = '\0';
              fputs(buf, stderr);
              fputs(" arguments\n", stderr);
              usage();
            }
          if (options[i].handler) {
            res = (*options[i].handler)(argv+2, options[i].otherArg);
            GAP_ASSERT(res == options[i].minargs);
          }
          else
            res = options[i].minargs;
          // recordOption(argv[1][1], res,  argv+2);
          argv += 1 + res;
          argc -= 1 + res;

        }
        else {
          argv++;
          argc--;
        }

      }

    InitSysFiles();

    // now that the user has had a chance to give -x and -y,
    // we determine the size of the screen ourselves
    getwindowsize();

    // when running in package mode set ctrl-d and line editing
    if ( SyWindow ) {
        SyRedirectStderrToStdOut();
        syWinPut( 0, "@p", "1." );
    }

    // the users home directory
    if ( getenv("HOME") != 0 ) {
        strxcpy(DotGapPath, getenv("HOME"), sizeof(DotGapPath));
# if defined(__APPLE__)
        // On Mac OS, add .gap to the sys roots, but leave
        // DotGapPath at $HOME/Library/Preferences/GAP
        strxcat(DotGapPath, "/.gap;", sizeof(DotGapPath));
        if (!IgnoreGapRC) {
          SySetGapRootPath(DotGapPath);
        }

        strxcpy(DotGapPath, getenv("HOME"), sizeof(DotGapPath));
        strxcat(DotGapPath, "/Library/Preferences/GAP;", sizeof(DotGapPath));
# elif defined(__CYGWIN__)
        strxcat(DotGapPath, "/_gap;", sizeof(DotGapPath));
# else
        strxcat(DotGapPath, "/.gap;", sizeof(DotGapPath));
# endif

        if (!IgnoreGapRC) {
          SySetGapRootPath(DotGapPath);
        }
        DotGapPath[strlen(DotGapPath)-1] = '\0';
    }
}
