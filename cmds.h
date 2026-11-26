/* ===================================================================
 * PixelBeeOS command library - 250+ commands, table driven.
 * Every command has a one-line usage string used by `man`, `apropos`,
 * `commands` and by the "Usage:" message when arguments are wrong.
 * =================================================================== */
static void app_open_cal(int m, int y);
static void app_open_browser(const char *url);
static void app_open_paint(void);
static int app_launch(const char *name);
static void cmd_lspci(int net_only);
static void app_list(void);
typedef struct { const char *name, *desc; u8 cat; } Cmd;
enum { C_LEG, C_ALI, C_M1, C_M2, C_BASE, C_UNIT, C_TIME, C_TEXT, C_FUN, C_SYS, C_FILE, C_N };
static const char *cat_name[C_N] = {"core", "alias", "math", "math", "convert", "units", "time", "text", "fun", "system", "files"};
static const char *cur_desc = "", *cur_name = "";

#define R(n, a, d, c) {n, n " " a "  -  " d, c}
#define AL(n, t) {n, n "  -  same as " t, C_ALI}

typedef struct { const char *name; int mul, div, pre, post; const char *unit; } Unit;
#define UNIT_LIST(X) \
    X("c2f", "Celsius to Fahrenheit", 9, 5, 0, 3200, "F") X("f2c", "Fahrenheit to Celsius", 5, 9, -3200, 0, "C") \
    X("c2k", "Celsius to Kelvin", 1, 1, 0, 27315, "K") X("k2c", "Kelvin to Celsius", 1, 1, -27315, 0, "C") \
    X("km2mi", "kilometers to miles", 6214, 10000, 0, 0, "mi") X("mi2km", "miles to kilometers", 16093, 10000, 0, 0, "km") \
    X("kg2lb", "kilograms to pounds", 22046, 10000, 0, 0, "lb") X("lb2kg", "pounds to kilograms", 4536, 10000, 0, 0, "kg") \
    X("cm2in", "centimeters to inches", 3937, 10000, 0, 0, "in") X("in2cm", "inches to centimeters", 254, 100, 0, 0, "cm") \
    X("m2ft", "meters to feet", 32808, 10000, 0, 0, "ft") X("ft2m", "feet to meters", 3048, 10000, 0, 0, "m") \
    X("l2gal", "liters to US gallons", 2642, 10000, 0, 0, "gal") X("gal2l", "US gallons to liters", 37854, 10000, 0, 0, "L") \
    X("oz2g", "ounces to grams", 28350, 1000, 0, 0, "g") X("g2oz", "grams to ounces", 3527, 100000, 0, 0, "oz") \
    X("kmh2mph", "km/h to mph", 6214, 10000, 0, 0, "mph") X("mph2kmh", "mph to km/h", 16093, 10000, 0, 0, "km/h") \
    X("min2sec", "minutes to seconds", 60, 1, 0, 0, "s") X("sec2min", "seconds to minutes", 1, 60, 0, 0, "min") \
    X("hour2min", "hours to minutes", 60, 1, 0, 0, "min") X("min2hour", "minutes to hours", 1, 60, 0, 0, "h") \
    X("day2hour", "days to hours", 24, 1, 0, 0, "h") X("hour2day", "hours to days", 1, 24, 0, 0, "days") \
    X("mb2kb", "megabytes to kilobytes", 1024, 1, 0, 0, "KB") X("kb2mb", "kilobytes to megabytes", 1, 1024, 0, 0, "MB") \
    X("gb2mb", "gigabytes to megabytes", 1024, 1, 0, 0, "MB") X("mb2gb", "megabytes to gigabytes", 1, 1024, 0, 0, "GB")
#define UENT(n, d, m, dv, p, q, u) {n, m, dv, p, q, u},
#define UROW(n, d, m, dv, p, q, u) {n, n " N  -  " d, C_UNIT},
static const Unit units[] = { UNIT_LIST(UENT) };

static const Cmd cmds[] = {
    /* core (implemented in exec_line) */
    R("help", "[tools]", "help pages", C_LEG), R("list", "", "show files and folders", C_LEG),
    R("go", "to NAME", "enter a folder  (go back = up one)", C_LEG), R("cd", "NAME", "change folder", C_LEG),
    R("md", "NAME", "make a folder", C_LEG), R("make", "file|folder NAME", "create a file or folder", C_LEG),
    R("edit", "NAME", "text editor, Esc saves", C_LEG), R("write", "NAME text", "one-line file", C_LEG),
    R("append", "NAME text", "add a line to a file", C_LEG), R("echo", "text [> file]", "print or save text", C_LEG),
    R("show", "NAME", "show a file", C_LEG), R("type", "NAME", "show a file", C_LEG),
    R("delete", "NAME", "delete file or empty folder", C_LEG), R("rename", "OLD NEW", "rename in place", C_LEG),
    R("move", "A B", "move or rename", C_LEG), R("copy", "A B", "copy a file", C_LEG),
    R("tree", "", "folder tree", C_LEG), R("find", "TEXT", "search names everywhere", C_LEG),
    R("clear", "", "clear the screen", C_LEG), R("about", "", "about PixelBeeOS", C_LEG),
    R("uptime", "", "time since boot", C_LEG), R("date", "", "date and time", C_LEG),
    R("memory", "", "RAM disk usage", C_LEG), R("meminfo", "", "RAM and file-store report", C_LEG), R("tasks", "", "list interactive app tasks", C_LEG), R("devices", "", "detected hardware and driver status", C_LEG), R("security", "", "lock, permissions and firewall status", C_LEG), R("island", "TEXT", "show text in the PixelBeeOS notification card", C_LEG),
    R("run", "FILE", "run a script: one command per line, # comments, if, alias, exit", C_LEG), R("if", "A == B CMD", "conditional: == != exists FILE", C_LEG),
    R("alias", "NAME = TEXT", "make a command shortcut", C_LEG), R("unalias", "NAME", "remove a shortcut", C_LEG), R("pipes", "", "how to chain commands with |", C_LEG), R("for", "x in a b c do CMD", "loop: for n in 1..5 do echo %n%", C_LEG),
    R("shutdown", "", "power off", C_LEG), R("restart", "", "reboot", C_LEG),
    R("beep", "", "PC speaker test", C_LEG), R("snake", "", "snake game", C_LEG),
    R("theme", "[name]", "frost, slate, liquid, lumen, prism, glacier, night, aurora, sunset, nebula", C_LEG), R("timer", "90|5m|stop", "countdown in the island", C_LEG),
    R("history", "", "last commands", C_LEG), R("sysinfo", "", "system summary", C_LEG),
    R("calc", "EXPR", "calculator with ( ) + - * / %", C_LEG), R("open", "NAME", "open a file or folder", C_LEG),
    /* aliases (handled by exec_line) */
    AL("dir", "list"), AL("ls", "list"), AL("cls", "clear"), AL("del", "delete"), AL("rm", "delete"), AL("cp", "copy"),
    AL("ren", "rename"), AL("mkdir", "md"), AL("cat", "type"), AL("read", "show"), AL("notepad", "edit"), AL("ver", "about"),
    AL("time", "date"), AL("mem", "memory"), AL("reboot", "restart"), AL("halt", "shutdown"), AL("off", "shutdown"),
    AL("poweroff", "shutdown"), AL("goto", "go"), AL("chdir", "cd"), AL("new", "make"), AL("create", "make"), AL("view", "show"),
    AL("erase", "delete"), AL("search", "find"), AL("info", "about"), AL("neofetch", "sysinfo"), AL("version", "about"),
    AL("free", "memory"), AL("math", "calc"), AL("wallpaper", "theme"), AL("clock", "date"), AL("specs", "sysinfo"),
    AL("game", "snake"), AL("play", "snake"),
    /* math: one number */
    R("sqrt", "N", "integer square root", C_M1), R("square", "N", "N times N", C_M1), R("cube", "N", "N cubed", C_M1),
    R("abs", "N", "absolute value", C_M1), R("neg", "N", "negate", C_M1), R("double", "N", "N times 2", C_M1),
    R("half", "N", "N divided by 2", C_M1), R("fact", "N", "factorial (0..12)", C_M1), R("fib", "N", "Nth Fibonacci number", C_M1),
    R("isprime", "N", "is N prime?", C_M1), R("nextprime", "N", "next prime above N", C_M1), R("prevprime", "N", "prime below N", C_M1),
    R("factors", "N", "prime factors", C_M1), R("divisors", "N", "all divisors", C_M1), R("digits", "N", "how many digits", C_M1),
    R("digitsum", "N", "sum of digits", C_M1), R("revnum", "N", "reverse the digits", C_M1), R("numpalin", "N", "is N a palindrome number?", C_M1),
    R("even", "N", "is N even?", C_M1), R("odd", "N", "is N odd?", C_M1), R("sign", "N", "negative, zero or positive", C_M1),
    R("triangle", "N", "Nth triangular number", C_M1), R("collatz", "N", "Collatz steps to reach 1", C_M1),
    R("log2", "N", "floor of log base 2", C_M1), R("bits", "N", "count of 1 bits", C_M1), R("cuberoot", "N", "integer cube root", C_M1),
    /* math: several numbers */
    R("gcd", "A B", "greatest common divisor", C_M2), R("lcm", "A B", "least common multiple", C_M2), R("pow", "A B", "A to the power B", C_M2),
    R("min", "A B ...", "smallest number", C_M2), R("max", "A B ...", "largest number", C_M2), R("mod", "A B", "remainder", C_M2),
    R("add", "A B ...", "add numbers", C_M2), R("sub", "A B", "subtract", C_M2), R("mul", "A B", "multiply", C_M2), R("div", "A B", "divide (with remainder)", C_M2),
    R("avg", "A B ...", "average", C_M2), R("sum", "A B ...", "total", C_M2), R("product", "A B ...", "multiply them all", C_M2),
    R("range", "A B [step]", "list numbers from A to B", C_M2), R("compare", "A B", "bigger, smaller or equal", C_M2),
    R("choose", "N K", "combinations (N choose K)", C_M2), R("percent", "P N", "P percent of N", C_M2),
    R("discount", "PRICE PCT", "price after discount", C_M2), R("tip", "BILL PCT", "tip and total", C_M2),
    R("split", "TOTAL PEOPLE", "share a bill", C_M2), R("interest", "MONEY RATE YEARS", "simple interest", C_M2),
    /* number formats */
    R("bin", "N", "to binary", C_BASE), R("hex", "N", "to hexadecimal", C_BASE), R("oct", "N", "to octal", C_BASE),
    R("frombin", "BITS", "binary to decimal", C_BASE), R("fromhex", "HEX", "hex to decimal", C_BASE), R("fromoct", "OCT", "octal to decimal", C_BASE),
    R("roman", "N", "to Roman numerals (1..3999)", C_BASE), R("fromroman", "XIV", "Roman numerals to number", C_BASE),
    R("spell", "N", "number in English words", C_BASE), R("ordinal", "N", "1st, 2nd, 3rd ...", C_BASE), R("comma", "N", "add thousands commas", C_BASE),
    /* units */
    UNIT_LIST(UROW)
    /* time and date */
    R("year", "", "current year", C_TIME), R("month", "", "current month", C_TIME), R("weekday", "", "today's weekday", C_TIME),
    R("hour", "", "current hour", C_TIME), R("minute", "", "current minute", C_TIME), R("now", "", "full date and time", C_TIME),
    R("cal", "[MONTH YEAR]", "calendar page", C_TIME), R("leapyear", "[YEAR]", "is it a leap year?", C_TIME),
    R("daysin", "MONTH [YEAR]", "days in a month", C_TIME), R("dayofweek", "YEAR MONTH DAY", "weekday of any date", C_TIME),
    R("age", "YEAR [MONTH DAY]", "age in years", C_TIME), R("epoch", "", "Unix time in seconds", C_TIME),
    R("stopwatch", "start|stop|lap|reset", "stopwatch", C_TIME), R("daysuntil", "MONTH DAY", "days until a date", C_TIME),
    R("calendar", "[MONTH YEAR]", "open the Calendar app (events, holidays)", C_TIME), R("zodiac", "MONTH DAY", "star sign", C_TIME), R("season", "[MONTH]", "season (northern hemisphere)", C_TIME),
    /* text */
    R("upper", "TEXT", "UPPERCASE", C_TEXT), R("lower", "TEXT", "lowercase", C_TEXT), R("reverse", "TEXT", "reverse the text", C_TEXT),
    R("len", "TEXT", "number of characters", C_TEXT), R("wordcount", "TEXT", "number of words", C_TEXT), R("vowels", "TEXT", "count vowels", C_TEXT),
    R("repeat", "N TEXT", "repeat text N times", C_TEXT), R("rot13", "TEXT", "rot13 cipher", C_TEXT), R("caesar", "SHIFT TEXT", "Caesar cipher", C_TEXT),
    R("palindrome", "TEXT", "is the text a palindrome?", C_TEXT), R("titlecase", "TEXT", "Capitalize Each Word", C_TEXT),
    R("countchar", "C TEXT", "how many times C appears", C_TEXT), R("morse", "TEXT", "to Morse code", C_TEXT),
    R("binary", "TEXT", "text to binary", C_TEXT), R("hextext", "TEXT", "text to hex bytes", C_TEXT), R("ord", "CHAR", "character code", C_TEXT),
    R("chr", "CODE", "code to character", C_TEXT), R("hash", "TEXT", "djb2 hash", C_TEXT), R("checksum", "TEXT", "sum of all bytes", C_TEXT),
    R("leet", "TEXT", "l33t sp34k", C_TEXT), R("shout", "TEXT", "LOUD!!!", C_TEXT), R("whisper", "TEXT", "quiet...", C_TEXT),
    R("mock", "TEXT", "aLtErNaTiNg CaSe", C_TEXT), R("center", "TEXT", "center on screen", C_TEXT), R("line", "N [CHAR]", "draw a line", C_TEXT),
    R("box", "TEXT", "put text in a box", C_TEXT), R("acronym", "WORDS", "first letters", C_TEXT), R("snakecase", "TEXT", "snake_case", C_TEXT),
    R("camel", "TEXT", "camelCase", C_TEXT), R("slug", "TEXT", "url-friendly-slug", C_TEXT), R("mask", "TEXT", "hide text with stars", C_TEXT),
    R("sortletters", "TEXT", "letters in A-Z order", C_TEXT), R("alphabet", "", "print the alphabet", C_TEXT), R("asciitable", "", "ASCII table", C_TEXT),
    /* fun */
    R("dice", "[SIDES]", "roll one die", C_FUN), R("roll", "NdM", "roll N dice with M sides", C_FUN), R("coin", "", "flip a coin", C_FUN),
    R("8ball", "[question]", "magic 8-ball", C_FUN), R("random", "[MAX | MIN MAX]", "random number", C_FUN), R("pick", "A B C ...", "pick one at random", C_FUN),
    R("shuffle", "A B C ...", "shuffle the words", C_FUN), R("lottery", "", "6 lucky numbers", C_FUN), R("password", "[LENGTH]", "random password", C_FUN),
    R("pin", "[LENGTH]", "random PIN", C_FUN), R("joke", "", "tell a joke", C_FUN), R("quote", "", "a nerdy quote", C_FUN),
    R("fortune", "", "your fortune", C_FUN), R("funfact", "", "a fun fact", C_FUN), R("cowsay", "TEXT", "a cow says it", C_FUN),
    R("snowflake", "", "ASCII snowflake", C_FUN), R("penguin", "", "ASCII penguin", C_FUN), R("heart", "", "ASCII heart", C_FUN),
    R("mountain", "", "ASCII glacier peaks", C_FUN), R("rps", "rock|paper|scissors", "play against PixelBeeOS", C_FUN),
    R("guess", "[N]", "number guessing game 1..100", C_FUN), R("hello", "", "PixelBeeOS says hi", C_FUN), R("compliment", "", "a nice word", C_FUN),
    R("motivate", "", "get motivated", C_FUN), R("luck", "NAME", "your lucky number", C_FUN), R("yesno", "", "decide yes or no", C_FUN),
    R("colorpick", "", "random color name", C_FUN),
    /* system */
    R("browser", "[address]", "BeeBrowser: offline pages and BeeSearch", C_SYS), R("web", "[address]", "same as browser", C_SYS), R("browse", "[address]", "same as browser", C_SYS),
    R("network", "", "open the Network panel (connect to the internet)", C_SYS), R("net", "[up|status]", "network card, DHCP address and status", C_SYS), R("ping", "HOST", "send 4 ping packets", C_SYS), R("dns", "HOST", "look up an IP address", C_SYS), R("http", "URL", "fetch a plain http:// page as text", C_SYS), R("get", "URL /PATH", "download a file to the disk D:", C_SYS), R("disk", "list|mount|ls|save|load", "real disks: FAT32 storage (disk for help)", C_SYS),
    R("paint", "", "drawing app (use the mouse)", C_SYS), R("mines", "", "Minesweeper (mouse or keys)", C_SYS),
    R("textsharp", "on|off", "crisp high-contrast text", C_SYS), R("deskicons", "on|off", "show or hide desktop shortcut icons", C_SYS), R("lock", "", "lock the screen (Super+L)", C_SYS), R("passwd", "", "set or change the lock password", C_SYS), R("autolock", "MINUTES", "auto-lock after idle (0 = off)", C_SYS),
    R("hotcorners", "on|off|lock", "screen-corner actions", C_SYS), R("mission", "", "Mission Control: all windows (F3)", C_SYS), R("snap", "left|right|full", "tile the front window", C_SYS), R("security", "", "show what is and is not protected", C_SYS),
    R("apps", "[name]", "list apps, or open one by name", C_SYS), R("lspci", "", "list all PCI hardware", C_SYS), R("display", "", "show screen resolution", C_SYS), R("vsync", "on|off", "wait for screen refresh before drawing (less tearing)", C_SYS), R("look", "glass|clear|sharp", "window style: frosted glass, see-through, or solid", C_SYS), R("netcards", "", "find network cards (wifi or wired)", C_SYS), R("calculator", "", "calculator app (keys or mouse)", C_SYS), R("clockapp", "", "big digital clock app", C_SYS), R("notepad", "", "notepad app (saves to RAM disk)", C_SYS), R("filemanager", "", "file manager app", C_SYS), R("snakewin", "", "snake game in a window", C_SYS), R("mouse", "", "mouse position and buttons", C_SYS),
    R("whoami", "", "current user", C_SYS), R("hostname", "", "computer name", C_SYS), R("pwd", "", "current folder", C_SYS),
    R("drives", "", "list drives", C_SYS), R("vol", "", "volume label", C_SYS), R("res", "", "screen resolution", C_SYS),
    R("cpu", "", "processor name", C_SYS), R("ram", "", "installed memory", C_SYS),
    R("color", "[accent|text] NAME", "terminal colors", C_SYS), R("prompt", "short|full|text WORDS", "change the prompt", C_SYS),
    R("title", "TEXT", "change the window title", C_SYS), R("glass", "40-230", "glass opacity", C_SYS), R("blur", "0-14", "glass blur strength", C_SYS),
    R("set", "NAME VALUE", "set a variable (use %NAME%)", C_SYS), R("get", "NAME", "read a variable", C_SYS),
    R("vars", "", "list variables", C_SYS), R("unset", "NAME", "delete a variable", C_SYS),
    R("todo", "add|done|del|clear TEXT", "to-do list", C_SYS), R("note", "TEXT", "save a quick note", C_SYS),
    R("notes", "[clear]", "show quick notes", C_SYS), R("format", "yes", "wipe the RAM disk", C_SYS),
    R("status", "", "one-screen status", C_SYS), R("commands", "[category|prefix|all]", "list all commands", C_SYS),
    R("man", "NAME", "describe a command", C_SYS), R("apropos", "WORD", "search command descriptions", C_SYS),
    R("surprise", "", "random command to try", C_SYS),
    /* files */
    R("touch", "NAME ...", "create empty files", C_FILE), R("head", "NAME [N]", "first lines", C_FILE), R("tail", "NAME [N]", "last lines", C_FILE),
    R("wc", "NAME", "lines, words, bytes", C_FILE), R("lines", "NAME", "number of lines", C_FILE), R("grep", "TEXT NAME", "lines containing TEXT", C_FILE),
    R("catn", "NAME", "show with line numbers", C_FILE), R("rev", "NAME", "show lines backwards", C_FILE), R("stat", "NAME", "file details", C_FILE),
    R("exists", "NAME", "does it exist?", C_FILE), R("size", "NAME", "size in bytes", C_FILE), R("du", "", "space used here", C_FILE),
    R("rmdir", "NAME", "remove an empty folder", C_FILE), R("clearfile", "NAME", "empty a file", C_FILE), R("diff", "A B", "compare two files", C_FILE),
    R("join", "A B OUT", "glue two files together", C_FILE), R("replace", "OLD NEW NAME", "replace text inside a file", C_FILE),
    R("lowerfile", "NAME", "lowercase a whole file", C_FILE), R("upperfile", "NAME", "UPPERCASE a whole file", C_FILE),
    R("sortfile", "NAME", "sort the lines of a file", C_FILE), R("mkdirs", "a\\b\\c", "make nested folders", C_FILE),
    R("root", "", "go to C:\\", C_FILE), R("up", "", "go up one folder", C_FILE), R("dirs", "", "list folders only", C_FILE),
    R("files", "", "list files only", C_FILE), R("countitems", "", "how many files and folders", C_FILE),
    R("biggest", "", "largest file here", C_FILE), R("empties", "", "empty files here", C_FILE), R("hexdump", "NAME", "hex view of a file", C_FILE),
};
#define NCMDS ((int)(sizeof cmds / sizeof cmds[0]))

static const Cmd *cmd_lookup(const char *n) {
    for (int i = 0; i < NCMDS; i++) if (ci_eq(cmds[i].name, n)) return &cmds[i];
    return 0;
}

/* ---------------- shared helpers ---------------- */
#define IS(x) ci_eq(n, x)
static void usage(void) { t_puts("Usage: ", A_WARN); t_say(cur_desc, A_TXT); }
#define NEED(k) do { if (ntok < (k) + 1) { usage(); return 1; } } while (0)

static int geti(int k, int *out) {
    if (k >= ntok) return 0;
    const char *p = tk[k]; int neg = 0, v = 0, base = 10;
    if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    if (!*p) return 0;
    for (; *p; p++) {
        int d; char ch = lw(*p);
        if (ch >= '0' && ch <= '9') d = ch - '0'; else if (base == 16 && ch >= 'a' && ch <= 'f') d = ch - 'a' + 10; else return 0;
        if (__builtin_mul_overflow(v, base, &v) || __builtin_add_overflow(v, d, &v)) return 0;
    }
    *out = neg ? -v : v; return 1;
}
static int getfx(int k, int *out) {                     /* "36.6" -> 3660 */
    if (k >= ntok) return 0;
    const char *p = tk[k]; int neg = 0, w = 0, f = 0, fd = 0;
    if (*p == '-') { neg = 1; p++; }
    if (!*p) return 0;
    while (*p >= '0' && *p <= '9') { if (__builtin_mul_overflow(w, 10, &w) || __builtin_add_overflow(w, *p - '0', &w)) return 0; p++; }
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { if (fd < 2) { f = f * 10 + (*p - '0'); fd++; } p++; } }
    if (*p) return 0;
    while (fd < 2) { f *= 10; fd++; }
    int v; if (__builtin_mul_overflow(w, 100, &v) || __builtin_add_overflow(v, f, &v)) return 0;
    *out = neg ? -v : v; return 1;
}
static void fxs(int v) {                                /* print x100 value */
    u32 u = v < 0 ? 0u - (u32)v : (u32)v; if (v < 0) t_putc('-', A_TXT);
    t_num(u / 100, 0); t_putc('.', A_TXT); t_putc('0' + (u / 10) % 10, A_TXT); t_putc('0' + u % 10, A_TXT);
}
static void outn(int v) { t_puts(" = ", A_DIM); t_snum(v); t_putc('\n', A_TXT); }
static void outs(const char *s) { t_puts(" = ", A_DIM); t_say(s, A_OK); }
static void outfx(int v, const char *unit) { t_puts(" = ", A_DIM); fxs(v); if (unit) { t_putc(' ', A_TXT); t_puts(unit, A_DIM); } t_putc('\n', A_TXT); }
static void out_yesno(int v) { outs(v ? "yes" : "no"); }
static void fmt_base(u32 v, int base, char *b) {
    char t[34]; int i = 0; if (!v) t[i++] = '0';
    while (v) { int d = (int)(v % (u32)base); t[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= (u32)base; }
    int o = 0; while (i) b[o++] = t[--i]; b[o] = 0;
}
static int cgcd(int a, int b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { int t = a % b; a = b; b = t; } return a; }
static int isqrt_(int v) { if (v < 2) return v < 0 ? 0 : v; int lo = 0, hi = 46340; while (lo < hi) { int m = (lo + hi + 1) / 2; if (m * m <= v) lo = m; else hi = m - 1; } return lo; }
static int icbrt(int a) { int lo = 0, hi = 1290; while (lo < hi) { int m = (lo + hi + 1) / 2; if (m * m * m <= a) lo = m; else hi = m - 1; } return lo; }
static int isprime_(int v) { if (v < 2) return 0; for (int i = 2; i <= 46340 && i * i <= v; i++) if (v % i == 0) return 0; return 1; }
static int pct100(int base100, int p100, int *ok) {     /* base * p% with x100 fixed point, 32-bit only */
    int t; if (__builtin_mul_overflow(base100 / 100, p100, &t)) { *ok = 0; return 0; }
    return t / 100 + (base100 % 100) * p100 / 10000;
}
static u32 rnd_(void) { rng ^= cur_ms * 2654435761u; return rnd(); }

/* ---------------- math: one number ---------------- */
static int h_m1(const char *n) {
    NEED(1); int v; if (!geti(1, &v)) { usage(); return 1; }
    if (IS("sqrt")) { if (v < 0) err("No real square root"); else outn(isqrt_(v)); }
    else if (IS("square")) { if (v > 46340 || v < -46340) err("Too big"); else outn(v * v); }
    else if (IS("cube")) { if (v > 1290 || v < -1290) err("Too big"); else outn(v * v * v); }
    else if (IS("abs")) { if (v == (int)0x80000000) err("Too big"); else outn(v < 0 ? -v : v); }
    else if (IS("neg")) { if (v == (int)0x80000000) err("Too big"); else outn(-v); }
    else if (IS("double")) { int r; if (__builtin_mul_overflow(v, 2, &r)) err("Too big"); else outn(r); }
    else if (IS("half")) { t_puts(" = ", A_DIM); t_snum(v / 2); if (v % 2) t_puts(".5", A_OK); t_putc('\n', A_TXT); }
    else if (IS("fact")) { if (v < 0 || v > 12) err("Use 0 to 12"); else { int r = 1; for (int i = 2; i <= v; i++) r *= i; outn(r); } }
    else if (IS("fib")) { if (v < 0 || v > 46) err("Use 0 to 46"); else { int a = 0, b = 1; for (int i = 0; i < v; i++) { int t = a + b; a = b; b = t; } outn(a); } }
    else if (IS("isprime")) out_yesno(isprime_(v));
    else if (IS("nextprime")) { if (v > 2147483000) { err("Too big"); return 1; } int p = v + 1; while (!isprime_(p)) p++; outn(p); }
    else if (IS("prevprime")) { int p = v - 1; while (p >= 2 && !isprime_(p)) p--; if (p < 2) err("No prime below that"); else outn(p); }
    else if (IS("factors")) {
        if (v < 2) { err("Use a number above 1"); return 1; }
        t_puts(" = ", A_DIM); int first = 1, x = v;
        for (int p = 2; p <= 46340 && p * p <= x; p++) while (x % p == 0) { if (!first) t_puts(" x ", A_DIM); t_num((u32)p, 0); first = 0; x /= p; }
        if (x > 1) { if (!first) t_puts(" x ", A_DIM); t_num((u32)x, 0); }
        t_putc('\n', A_TXT);
    }
    else if (IS("divisors")) {
        if (v < 1) { err("Use a positive number"); return 1; }
        int cnt = 0; for (int i = 1; i <= 46340 && i * i <= v; i++) if (v % i == 0) cnt += (i * i == v) ? 1 : 2;
        t_puts(" count = ", A_DIM); t_num((u32)cnt, 0); t_putc('\n', A_TXT);
        if (v <= 5000) { t_puts(" ", A_DIM); for (int i = 1; i <= v; i++) if (v % i == 0) { t_num((u32)i, 0); t_putc(' ', A_TXT); } t_putc('\n', A_TXT); }
    }
    else if (IS("digits")) { u32 u = v < 0 ? 0u - (u32)v : (u32)v; int c = 1; while (u >= 10) { u /= 10; c++; } outn(c); }
    else if (IS("digitsum")) { u32 u = v < 0 ? 0u - (u32)v : (u32)v; int s = 0; while (u) { s += (int)(u % 10); u /= 10; } outn(s); }
    else if (IS("revnum")) { u32 u = v < 0 ? 0u - (u32)v : (u32)v, r = 0; while (u) { r = r * 10 + u % 10; u /= 10; } t_puts(" = ", A_DIM); if (v < 0) t_putc('-', A_TXT); t_num(r, 0); t_putc('\n', A_TXT); }
    else if (IS("numpalin")) { u32 u = v < 0 ? 0u - (u32)v : (u32)v, o = u, r = 0; while (u) { r = r * 10 + u % 10; u /= 10; } out_yesno(v >= 0 && r == o); }
    else if (IS("even")) out_yesno(v % 2 == 0);
    else if (IS("odd")) out_yesno(v % 2 != 0);
    else if (IS("sign")) outs(v < 0 ? "negative" : (v == 0 ? "zero" : "positive"));
    else if (IS("triangle")) { if (v < 0 || v > 65535) err("Use 0 to 65535"); else { u32 u = (u32)v; t_puts(" = ", A_DIM); t_num(u * (u + 1) / 2, 0); t_putc('\n', A_TXT); } }
    else if (IS("collatz")) {
        if (v < 1 || v > 700000000) { err("Use 1 to 700000000"); return 1; }
        u32 x = (u32)v, mx = x; int steps = 0; while (x != 1 && steps < 2000) { x = (x & 1) ? 3 * x + 1 : x / 2; if (x > mx) mx = x; steps++; }
        t_puts(" steps = ", A_DIM); t_num((u32)steps, 0); t_puts("   peak = ", A_DIM); t_num(mx, 0); t_putc('\n', A_TXT);
    }
    else if (IS("log2")) { if (v < 1) err("Use a positive number"); else { int r = 0; u32 u = (u32)v; while (u > 1) { u >>= 1; r++; } outn(r); } }
    else if (IS("bits")) { int c = 0; for (u32 u = (u32)v; u; u >>= 1) c += (int)(u & 1); outn(c); }
    else if (IS("cuberoot")) { int a = v < 0 ? -v : v; if (v == (int)0x80000000) { err("Too big"); return 1; } int r = icbrt(a); outn(v < 0 ? -r : r); }
    else return 0;
    return 1;
}

/* ---------------- math: several numbers ---------------- */
static int h_m2(const char *n) {
    NEED(1); int a = 0, b = 0, r;
    if (IS("min") || IS("max") || IS("add") || IS("sum") || IS("product") || IS("avg")) {
        int acc = 0, cnt = 0, big = 0; 
        for (int k = 1; k < ntok; k++) {
            int v; if (!geti(k, &v)) { usage(); return 1; }
            if (cnt == 0) acc = v;
            else if (IS("min")) { if (v < acc) acc = v; }
            else if (IS("max")) { if (v > acc) acc = v; }
            else if (IS("product")) { if (__builtin_mul_overflow(acc, v, &acc)) big = 1; }
            else { if (__builtin_add_overflow(acc, v, &acc)) big = 1; }
            cnt++;
        }
        if (big) { err("Too big"); return 1; }
        if (IS("avg")) { int tenth = (acc % cnt) * 10 / cnt; if (tenth < 0) tenth = -tenth; t_puts(" = ", A_DIM); t_snum(acc / cnt); t_putc('.', A_TXT); t_putc('0' + tenth, A_TXT); t_putc('\n', A_TXT); }
        else outn(acc);
        return 1;
    }
    if (IS("range")) {
        int step = 1; if (!geti(1, &a) || !geti(2, &b)) { usage(); return 1; } if (ntok > 3) geti(3, &step);
        if (step == 0) { err("Step can't be 0"); return 1; }
        if (a > b && step > 0) step = -step;
        int c = 0; t_puts(" ", A_DIM);
        for (int v = a; (step > 0 ? v <= b : v >= b) && c < 60; v += step, c++) { t_snum(v); t_putc(' ', A_TXT); }
        t_putc('\n', A_TXT); return 1;
    }
    if (IS("discount") || IS("tip") || IS("split") || IS("interest") || IS("percent")) {
        int x, y; if (!getfx(1, &x) || !getfx(2, &y)) { usage(); return 1; }
        int ok = 1;
        if (IS("percent")) { int v = pct100(y, x, &ok); if (!ok) err("Too big"); else { t_puts(" = ", A_DIM); fxs(v); t_putc('\n', A_TXT); } return 1; }
        if (IS("discount")) { int off = pct100(x, y, &ok); if (!ok) err("Too big"); else { t_puts(" save ", A_DIM); fxs(off); t_puts("   pay ", A_DIM); fxs(x - off); t_putc('\n', A_TXT); } return 1; }
        if (IS("tip")) { int t = pct100(x, y, &ok); if (!ok) err("Too big"); else { t_puts(" tip ", A_DIM); fxs(t); t_puts("   total ", A_DIM); fxs(x + t); t_putc('\n', A_TXT); } return 1; }
        if (IS("split")) { if (y < 100) { err("Need at least 1 person"); return 1; } int people = y / 100; t_puts(" each pays ", A_DIM); fxs(x / people); t_putc('\n', A_TXT); return 1; }
        if (ntok < 4) { usage(); return 1; }
        int yrs; if (!geti(3, &yrs) || yrs < 0) { usage(); return 1; }
        int gain = pct100(x, y, &ok); if (!ok || __builtin_mul_overflow(gain, yrs, &gain)) { err("Too big"); return 1; }
        t_puts(" interest ", A_DIM); fxs(gain); t_puts("   total ", A_DIM); fxs(x + gain); t_putc('\n', A_TXT); return 1;
    }
    if (!geti(1, &a) || !geti(2, &b)) { usage(); return 1; }
    if (IS("gcd")) outn(cgcd(a, b));
    else if (IS("lcm")) { int g = cgcd(a, b); if (!g) outn(0); else { int t = a / g; if (__builtin_mul_overflow(t, b, &r)) err("Too big"); else outn(r < 0 ? -r : r); } }
    else if (IS("pow")) {
        if (b < 0) { err("Use a power of 0 or more"); return 1; }
        r = 1; for (int i = 0; i < b; i++) if (__builtin_mul_overflow(r, a, &r)) { err("Too big"); return 1; }
        outn(r);
    }
    else if (IS("mod")) { if (!b) err("Can't divide by zero"); else outn(b == -1 ? 0 : a % b); }
    else if (IS("sub")) { if (__builtin_sub_overflow(a, b, &r)) err("Too big"); else outn(r); }
    else if (IS("mul")) { if (__builtin_mul_overflow(a, b, &r)) err("Too big"); else outn(r); }
    else if (IS("div")) {
        if (!b) { err("Can't divide by zero"); return 1; }
        if (b == -1) { outn(-a); return 1; }
        t_puts(" = ", A_DIM); t_snum(a / b); if (a % b) { t_puts("  remainder ", A_DIM); t_snum(a % b); } t_putc('\n', A_TXT);
    }
    else if (IS("compare")) outs(a < b ? "first is smaller" : (a > b ? "first is bigger" : "equal"));
    else if (IS("choose")) {
        if (a < 0 || b < 0 || b > a || a > 30) { err("Use N up to 30 and 0 <= K <= N"); return 1; }
        if (b > a - b) b = a - b; u32 c = 1; for (int i = 1; i <= b; i++) c = c * (u32)(a - b + i) / (u32)i;
        t_puts(" = ", A_DIM); t_num(c, 0); t_putc('\n', A_TXT);
    }
    else return 0;
    return 1;
}

/* ---------------- number formats ---------------- */
static const char *ones_[20] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
static const char *tens_[10] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
static char spb[320]; static int spn;
static void sp_add(const char *s) { if (spn) spb[spn++] = ' '; while (*s) spb[spn++] = *s++; spb[spn] = 0; }
static void sp_chunk(int v) {
    if (v >= 100) { sp_add(ones_[v / 100]); sp_add("hundred"); v %= 100; }
    if (v >= 20) { sp_add(tens_[v / 10]); v %= 10; if (v) sp_add(ones_[v]); } else if (v > 0) sp_add(ones_[v]);
}
static void spell_(int v) {
    spn = 0; spb[0] = 0; if (v < 0) { sp_add("minus"); v = -v; }
    if (v == 0) { sp_add("zero"); return; }
    int g = v / 1000000000; if (g) { sp_chunk(g); sp_add("billion"); v %= 1000000000; }
    int m = v / 1000000; if (m) { sp_chunk(m); sp_add("million"); v %= 1000000; }
    int t = v / 1000; if (t) { sp_chunk(t); sp_add("thousand"); v %= 1000; }
    if (v) sp_chunk(v);
}
static int h_base(const char *n) {
    NEED(1); char b[40]; int v;
    if (IS("frombin") || IS("fromhex") || IS("fromoct")) {
        int base = IS("frombin") ? 2 : (IS("fromhex") ? 16 : 8); const char *p = tk[1]; u32 r = 0;
        if (base == 16 && p[0] == '0' && lw(p[1]) == 'x') p += 2;
        if (!*p) { usage(); return 1; }
        for (; *p; p++) { int d = (*p >= '0' && *p <= '9') ? *p - '0' : (lw(*p) >= 'a' && lw(*p) <= 'f') ? lw(*p) - 'a' + 10 : 99; if (d >= base || r > 0x7FFFFFFFu / (u32)base) { err("Not a valid number"); return 1; } r = r * (u32)base + (u32)d; }
        t_puts(" = ", A_DIM); t_num(r, 0); t_putc('\n', A_TXT); return 1;
    }
    if (IS("fromroman")) {
        int total = 0, prev = 0;
        for (int i = slen(tk[1]) - 1; i >= 0; i--) {
            char c = lw(tk[1][i]); int x = c == 'i' ? 1 : c == 'v' ? 5 : c == 'x' ? 10 : c == 'l' ? 50 : c == 'c' ? 100 : c == 'd' ? 500 : c == 'm' ? 1000 : 0;
            if (!x) { err("Not a Roman numeral"); return 1; }
            if (x < prev) total -= x; else { total += x; prev = x; }
        }
        outn(total); return 1;
    }
    if (!geti(1, &v)) { usage(); return 1; }
    if (IS("bin")) { fmt_base((u32)v, 2, b); outs(b); }
    else if (IS("hex")) { fmt_base((u32)v, 16, b); t_puts(" = 0x", A_DIM); t_say(b, A_OK); }
    else if (IS("oct")) { fmt_base((u32)v, 8, b); outs(b); }
    else if (IS("roman")) {
        if (v < 1 || v > 3999) { err("Use 1 to 3999"); return 1; }
        static const int val[13] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
        static const char *sy[13] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
        char o[40]; int p = 0; for (int i = 0; i < 13; i++) while (v >= val[i]) { for (const char *s = sy[i]; *s; s++) o[p++] = *s; v -= val[i]; }
        o[p] = 0; outs(o);
    }
    else if (IS("spell")) { if (v == (int)0x80000000) { err("Too big"); return 1; } spell_(v); outs(spb); }
    else if (IS("ordinal")) {
        u32 u = v < 0 ? 0u - (u32)v : (u32)v; u32 m100 = u % 100, m10 = u % 10;
        const char *s = (m100 >= 11 && m100 <= 13) ? "th" : m10 == 1 ? "st" : m10 == 2 ? "nd" : m10 == 3 ? "rd" : "th";
        t_puts(" = ", A_DIM); t_snum(v); t_say(s, A_OK);
    }
    else if (IS("comma")) {
        u32 u = v < 0 ? 0u - (u32)v : (u32)v; char d[16]; int dl = 0; if (!u) d[dl++] = '0'; while (u) { d[dl++] = (char)('0' + u % 10); u /= 10; }
        char o[24]; int p = 0; if (v < 0) o[p++] = '-';
        for (int i = dl - 1; i >= 0; i--) { o[p++] = d[i]; if (i && i % 3 == 0) o[p++] = ','; }
        o[p] = 0; outs(o);
    }
    else return 0;
    return 1;
}

/* ---------------- unit converters ---------------- */
static int h_unit(const char *n) {
    NEED(1); int v; if (!getfx(1, &v)) { usage(); return 1; }
    for (u32 i = 0; i < sizeof units / sizeof units[0]; i++) if (IS(units[i].name)) {
        const Unit *u = &units[i]; int x, t, r;
        if (__builtin_add_overflow(v, u->pre, &x)) { err("Too big"); return 1; }
        int q = x / u->div, rem = x % u->div;
        if (__builtin_mul_overflow(q, u->mul, &t) || __builtin_mul_overflow(rem, u->mul, &r)) { err("Number too big"); return 1; }
        if (__builtin_add_overflow(t, r / u->div, &t) || __builtin_add_overflow(t, u->post, &t)) { err("Number too big"); return 1; }
        outfx(t, u->unit); return 1;
    }
    return 0;
}

/* ---------------- time and date ---------------- */
static const char *mon_[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
static const char *wd_[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static int leap_(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int dim_(int y, int m) { static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; return (m == 2 && leap_(y)) ? 29 : d[m - 1]; }
static int dow_(int y, int m, int d) { static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4}; if (m < 3) y--; return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7; }
static u32 days_(int y, int m, int d) {
    y -= m <= 2; int era = y / 400, yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return (u32)(era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468);
}
static int sw_on; static u32 sw_t0, sw_acc;
static void sw_show(void) {
    u32 e = sw_acc + (sw_on ? cur_ms - sw_t0 : 0); t_puts(" = ", A_DIM); t_num(e / 1000, 0); t_putc('.', A_TXT);
    t_putc('0' + (e / 100) % 10, A_TXT); t_putc('0' + (e / 10) % 10, A_TXT); t_putc('0' + e % 10, A_TXT); t_say(sw_on ? " s  (running)" : " s", A_DIM);
}
static void cal_page(int y, int m, int today) {
    char hd[40]; int p = 0; for (const char *s = mon_[m - 1]; *s; s++) hd[p++] = *s; hd[p++] = ' ';
    char yb[8]; fmt_base((u32)y, 10, yb); for (char *q = yb; *q; q++) hd[p++] = *q; hd[p] = 0;
    for (int i = 0; i < (20 - p) / 2 + 1; i++) t_putc(' ', A_TXT);
    t_say(hd, A_ACC); t_say(" Su Mo Tu We Th Fr Sa", A_DIM);
    int first = dow_(y, m, 1), nd_ = dim_(y, m); t_putc(' ', A_TXT);
    for (int i = 0; i < first; i++) t_puts("   ", A_TXT);
    for (int d = 1; d <= nd_; d++) {
        u8 a = d == today ? A_ACC : A_TXT;
        t_putc(d < 10 ? ' ' : (char)('0' + d / 10), a); t_putc((char)('0' + d % 10), a);
        if ((first + d) % 7 == 0) { t_putc('\n', A_TXT); if (d < nd_) t_putc(' ', A_TXT); } else t_putc(' ', A_TXT);
    }
    if ((first + nd_) % 7) t_putc('\n', A_TXT);
}
static int h_time(const char *n) {
    int hh, mm, ss, dd, mo, yy; rtc_read(&hh, &mm, &ss, &dd, &mo, &yy); int a, b, c;
    if (IS("year")) outn(yy);
    else if (IS("month")) { t_puts(" = ", A_DIM); t_puts(mon_[mo - 1], A_OK); t_puts(" (", A_DIM); t_num((u32)mo, 0); t_say(")", A_DIM); }
    else if (IS("weekday")) outs(wd_[dow_(yy, mo, dd)]);
    else if (IS("hour")) outn(hh);
    else if (IS("minute")) outn(mm);
    else if (IS("now")) {
        t_puts(" ", A_DIM); t_puts(wd_[dow_(yy, mo, dd)], A_OK); t_puts(", ", A_DIM); t_num((u32)dd, 0); t_putc(' ', A_TXT); t_puts(mon_[mo - 1], A_TXT);
        t_putc(' ', A_TXT); t_num((u32)yy, 0); t_puts("   ", A_DIM); t_pad2(hh); t_putc(':', A_DIM); t_pad2(mm); t_putc(':', A_DIM); t_pad2(ss); t_putc('\n', A_TXT);
    }
    else if (IS("cal")) {
        int m2 = mo, y2 = yy;
        if (ntok > 1) { if (!geti(1, &m2) || m2 < 1 || m2 > 12) { usage(); return 1; } if (ntok > 2 && (!geti(2, &y2) || y2 < 1 || y2 > 9999)) { usage(); return 1; } }
        cal_page(y2, m2, (m2 == mo && y2 == yy) ? dd : 0);
    }
    else if (IS("leapyear")) { int y2 = yy; if (ntok > 1 && !geti(1, &y2)) { usage(); return 1; } out_yesno(leap_(y2)); }
    else if (IS("daysin")) { int y2 = yy; NEED(1); if (!geti(1, &a) || a < 1 || a > 12) { usage(); return 1; } if (ntok > 2 && !geti(2, &y2)) { usage(); return 1; } outn(dim_(y2, a)); }
    else if (IS("dayofweek")) {
        NEED(3); if (!geti(1, &a) || !geti(2, &b) || !geti(3, &c) || b < 1 || b > 12 || a < 1 || a > 9999 || c < 1 || c > dim_(a, b)) { usage(); return 1; }
        outs(wd_[dow_(a, b, c)]);
    }
    else if (IS("age")) {
        NEED(1); if (!geti(1, &a) || a < 1 || a > yy) { usage(); return 1; } int yrs = yy - a;
        if (ntok > 3 && geti(2, &b) && geti(3, &c) && (mo < b || (mo == b && dd < c))) yrs--;
        t_puts(" = ", A_DIM); t_num((u32)yrs, 0); t_say(" years old", A_OK);
    }
    else if (IS("epoch")) { t_puts(" = ", A_DIM); t_num(days_(yy, mo, dd) * 86400u + (u32)hh * 3600u + (u32)mm * 60u + (u32)ss, 0); t_putc('\n', A_TXT); }
    else if (IS("stopwatch")) {
        const char *w = ntok > 1 ? tl[1] : "";
        if (ci_eq(w, "start")) { if (!sw_on) { sw_on = 1; sw_t0 = cur_ms; } t_say(" Stopwatch running", A_OK); }
        else if (ci_eq(w, "stop")) { if (sw_on) { sw_acc += cur_ms - sw_t0; sw_on = 0; } sw_show(); }
        else if (ci_eq(w, "reset")) { sw_on = 0; sw_acc = 0; t_say(" Stopwatch reset", A_OK); }
        else sw_show();
        if (ci_eq(w, "lap")) { sw_show(); }
    }
    else if (IS("daysuntil")) {
        NEED(2); if (!geti(1, &a) || !geti(2, &b) || a < 1 || a > 12 || b < 1 || b > 31) { usage(); return 1; }
        int ty = yy; if (b > dim_(ty, a)) { err("That day does not exist"); return 1; }
        if (days_(ty, a, b) < days_(yy, mo, dd)) { ty++; if (b > dim_(ty, a)) { err("That day does not exist"); return 1; } }
        u32 df = days_(ty, a, b) - days_(yy, mo, dd);
        if (!df) t_say(" That is today!", A_OK); else { t_puts(" = ", A_DIM); t_num(df, 0); t_say(df == 1 ? " day" : " days", A_OK); }
    }
    else if (IS("zodiac")) {
        NEED(2); if (!geti(1, &a) || !geti(2, &b) || a < 1 || a > 12 || b < 1 || b > 31) { usage(); return 1; }
        static const char *sg[13] = {"Capricorn", "Aquarius", "Pisces", "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo", "Libra", "Scorpio", "Sagittarius", "Capricorn"};
        static const int cut[12] = {20, 19, 21, 20, 21, 21, 23, 23, 23, 23, 22, 22};
        outs(sg[a - 1 + (b >= cut[a - 1] ? 1 : 0)]);
    }
    else if (IS("season")) {
        int m2 = mo; if (ntok > 1 && (!geti(1, &m2) || m2 < 1 || m2 > 12)) { usage(); return 1; }
        outs(m2 == 12 || m2 <= 2 ? "Winter" : (m2 <= 5 ? "Spring" : (m2 <= 8 ? "Summer" : "Autumn")));
    }
    else if (IS("calendar")) {
        int m2 = 0, y2 = 0;
        if (ntok > 1) { if (!geti(1, &m2) || m2 < 1 || m2 > 12) { usage(); return 1; } if (ntok > 2 && (!geti(2, &y2) || y2 < 1970 || y2 > 2999)) { usage(); return 1; } }
        app_open_cal(m2, y2);
    }
    else return 0;
    return 1;
}

/* ---------------- text tools ---------------- */
static const char *rest1(void) { return ntok > 1 ? trest[1] : ""; }
static void emit_map(const char *r, int mode) {
    t_putc(' ', A_TXT); int i = 0, up = 1;
    for (; *r; r++, i++) {
        char c = *r;
        if (mode == 'U') c = (c >= 'a' && c <= 'z') ? c - 32 : c;
        else if (mode == 'L') c = lw(c);
        else if (mode == 'R') { if (c >= 'a' && c <= 'z') c = (char)('a' + (c - 'a' + 13) % 26); else if (c >= 'A' && c <= 'Z') c = (char)('A' + (c - 'A' + 13) % 26); }
        else if (mode == 'T') { c = up ? ((c >= 'a' && c <= 'z') ? c - 32 : c) : lw(c); up = (c == ' '); }
        else if (mode == 'K') { char l = lw(c); c = l == 'a' ? '4' : l == 'e' ? '3' : l == 'i' ? '1' : l == 'o' ? '0' : l == 's' ? '5' : l == 't' ? '7' : c; }
        else if (mode == 'M') c = (i & 1) ? ((c >= 'a' && c <= 'z') ? c - 32 : c) : lw(c);
        else if (mode == 'S') c = c == ' ' ? ' ' : '*';
        t_putc(c, A_TXT);
    }
    t_putc('\n', A_TXT);
}
static const char *mz_[26] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};
static const char *mzd_[10] = {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};
static int h_text(const char *n) {
    const char *r = rest1(); int i;
    if (IS("alphabet")) { t_say(" a b c d e f g h i j k l m n o p q r s t u v w x y z", A_TXT); t_say(" A B C D E F G H I J K L M N O P Q R S T U V W X Y Z", A_DIM); return 1; }
    if (IS("asciitable")) {
        for (int base = 32; base < 127; base += 16) { t_putc(' ', A_TXT); t_num((u32)base, 3); t_puts("  ", A_DIM); for (int c = base; c < base + 16 && c < 127; c++) { t_putc((char)c, A_ACC); t_putc(' ', A_TXT); } t_putc('\n', A_TXT); }
        return 1;
    }
    NEED(1);
    if (IS("upper")) emit_map(r, 'U'); else if (IS("lower")) emit_map(r, 'L'); else if (IS("rot13")) emit_map(r, 'R');
    else if (IS("titlecase")) emit_map(r, 'T'); else if (IS("leet")) emit_map(r, 'K'); else if (IS("mock")) emit_map(r, 'M'); else if (IS("mask")) emit_map(r, 'S');
    else if (IS("reverse")) { t_putc(' ', A_TXT); for (i = slen(r) - 1; i >= 0; i--) t_putc(r[i], A_TXT); t_putc('\n', A_TXT); }
    else if (IS("len")) outn(slen(r));
    else if (IS("wordcount")) outn(ntok - 1);
    else if (IS("vowels")) { int c = 0; for (; *r; r++) { char l = lw(*r); if (l == 'a' || l == 'e' || l == 'i' || l == 'o' || l == 'u') c++; } outn(c); }
    else if (IS("palindrome")) { char b[256]; int p = 0; for (; *r && p < 255; r++) { char l = lw(*r); if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9')) b[p++] = l; } int ok = 1; for (i = 0; i < p / 2; i++) if (b[i] != b[p - 1 - i]) ok = 0; out_yesno(ok); }
    else if (IS("shout")) { t_putc(' ', A_TXT); for (; *r; r++) t_putc((*r >= 'a' && *r <= 'z') ? *r - 32 : *r, A_WARN); t_say("!!!", A_WARN); }
    else if (IS("whisper")) { t_putc(' ', A_TXT); for (; *r; r++) t_putc(lw(*r), A_DIM); t_say("...", A_DIM); }
    else if (IS("repeat")) {
        int cnt; if (ntok < 3 || !geti(1, &cnt) || cnt < 1) { usage(); return 1; } if (cnt > 40) cnt = 40; r = trest[2];
        t_putc(' ', A_TXT); for (i = 0; i < cnt && i * (slen(r) + 1) < 900; i++) { t_puts(r, A_TXT); t_putc(' ', A_TXT); } t_putc('\n', A_TXT);
    }
    else if (IS("caesar")) {
        int sh; if (ntok < 3 || !geti(1, &sh)) { usage(); return 1; } sh = ((sh % 26) + 26) % 26; r = trest[2]; t_putc(' ', A_TXT);
        for (; *r; r++) { char c = *r; if (c >= 'a' && c <= 'z') c = (char)('a' + (c - 'a' + sh) % 26); else if (c >= 'A' && c <= 'Z') c = (char)('A' + (c - 'A' + sh) % 26); t_putc(c, A_OK); }
        t_putc('\n', A_TXT);
    }
    else if (IS("countchar")) { if (ntok < 3) { usage(); return 1; } int c = 0; char ch = lw(tk[1][0]); for (r = trest[2]; *r; r++) if (lw(*r) == ch) c++; outn(c); }
    else if (IS("ord")) outn((u8)tk[1][0]);
    else if (IS("chr")) { int v; if (!geti(1, &v) || v < 32 || v > 126) { err("Use a code from 32 to 126"); return 1; } char b[2] = {(char)v, 0}; outs(b); }
    else if (IS("hash")) { u32 h = 5381; for (; *r; r++) h = h * 33 + (u8)*r; char b[12]; fmt_base(h, 16, b); t_puts(" = 0x", A_DIM); t_say(b, A_OK); }
    else if (IS("checksum")) { u32 h = 0; for (; *r; r++) h += (u8)*r; outn((int)(h % 65536)); }
    else if (IS("binary")) { t_putc(' ', A_TXT); for (i = 0; r[i] && i < 8; i++) { for (int bit = 7; bit >= 0; bit--) t_putc(((u8)r[i] >> bit) & 1 ? '1' : '0', A_TXT); t_putc(' ', A_TXT); } t_putc('\n', A_TXT); if (slen(r) > 8) t_say(" (first 8 characters)", A_DIM); }
    else if (IS("hextext")) { t_putc(' ', A_TXT); for (i = 0; r[i] && i < 24; i++) { char b[4]; fmt_base((u8)r[i], 16, b); if (!b[1]) { b[1] = b[0]; b[0] = '0'; b[2] = 0; } t_puts(b, A_TXT); t_putc(' ', A_TXT); } t_putc('\n', A_TXT); }
    else if (IS("morse")) {
        t_putc(' ', A_TXT);
        for (; *r; r++) { char l = lw(*r); if (l >= 'a' && l <= 'z') t_puts(mz_[l - 'a'], A_ACC); else if (l >= '0' && l <= '9') t_puts(mzd_[l - '0'], A_ACC); else if (l == ' ') t_puts("/", A_DIM); else continue; t_putc(' ', A_TXT); }
        t_putc('\n', A_TXT);
    }
    else if (IS("center")) { int pad = (cols - slen(r)) / 2; for (i = 0; i < pad; i++) t_putc(' ', A_TXT); t_say(r, A_ACC); }
    else if (IS("line")) { int len; if (!geti(1, &len) || len < 1) { usage(); return 1; } if (len > cols - 2) len = cols - 2; char ch = ntok > 2 ? tk[2][0] : '-'; t_putc(' ', A_TXT); for (i = 0; i < len; i++) t_putc(ch, A_ACC); t_putc('\n', A_TXT); }
    else if (IS("box")) {
        int len = slen(r); if (len > cols - 6) len = cols - 6;
        t_putc(' ', A_TXT); t_putc('+', A_ACC); for (i = 0; i < len + 2; i++) t_putc('-', A_ACC); t_say("+", A_ACC);
        t_puts(" | ", A_ACC); for (i = 0; i < len; i++) t_putc(r[i], A_TXT); t_say(" |", A_ACC);
        t_putc(' ', A_TXT); t_putc('+', A_ACC); for (i = 0; i < len + 2; i++) t_putc('-', A_ACC); t_say("+", A_ACC);
    }
    else if (IS("acronym")) { t_puts(" = ", A_DIM); for (i = 1; i < ntok; i++) { char c = tk[i][0]; t_putc((c >= 'a' && c <= 'z') ? c - 32 : c, A_OK); } t_putc('\n', A_TXT); }
    else if (IS("snakecase")) { t_puts(" = ", A_DIM); for (; *r; r++) t_putc(*r == ' ' ? '_' : lw(*r), A_OK); t_putc('\n', A_TXT); }
    else if (IS("camel")) { t_puts(" = ", A_DIM); for (i = 1; i < ntok; i++) for (int k = 0; tk[i][k]; k++) { char c = tk[i][k]; t_putc((i > 1 && k == 0 && c >= 'a' && c <= 'z') ? c - 32 : (i == 1 ? lw(c) : (k ? lw(c) : c)), A_OK); } t_putc('\n', A_TXT); }
    else if (IS("slug")) { t_puts(" = ", A_DIM); int dash = 0, any = 0; for (; *r; r++) { char l = lw(*r); if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9')) { if (dash && any) t_putc('-', A_OK); t_putc(l, A_OK); dash = 0; any = 1; } else dash = 1; } t_putc('\n', A_TXT); }
    else if (IS("sortletters")) { int cnt[256] = {0}; for (; *r; r++) if (*r != ' ') cnt[(u8)lw(*r)]++; t_puts(" = ", A_DIM); for (i = 0; i < 256; i++) while (cnt[i]--) t_putc((char)i, A_OK); t_putc('\n', A_TXT); }
    else return 0;
    return 1;
}

/* ---------------- fun ---------------- */
#define NARR(a) ((int)(sizeof a / sizeof a[0]))
static const char *jokes_[] = {"Why do programmers prefer dark mode? Because light attracts bugs.", "There are 10 kinds of people: those who know binary and those who don't.",
    "A SQL query walks into a bar, sees two tables and asks: can I join you?", "Why did the kernel panic? Too many unhandled issues.", "I would tell you a UDP joke, but you might not get it.",
    "Why was the glacier a great programmer? It never rushed a commit.", "Real programmers count from 0. Even their birthdays."};
static const char *quotes_[] = {"Talk is cheap. Show me the code. - Linus Torvalds", "Simplicity is a prerequisite for reliability. - Dijkstra",
    "The best way to predict the future is to invent it. - Alan Kay", "Make it work, make it right, make it fast. - Kent Beck", "Programs must be written for people to read. - Abelson & Sussman"};
static const char *fortunes_[] = {"A small bug today saves a big crash tomorrow.", "Your next build will compile on the first try.", "Someone will thank you for your code comments.",
    "A great idea is hiding inside your last error message.", "The kernel you write today becomes the OS you love tomorrow."};
static const char *facts_[] = {"The first computer bug was a real moth, found in 1947.", "Ice is less dense than water, so it floats.", "A glacier can creep from centimeters to meters per day.",
    "The Linux kernel started as a hobby project in 1991.", "The IBM 350 disk drive of 1956 held about 5 MB.", "Binary uses only two digits and can still describe anything."};
static const char *compl_[] = {"Your code is cleaner than fresh snow.", "You write kernels for fun. That is seriously impressive.", "Your debugging skills are glacial-level calm.", "PixelBeeOS is lucky to have you."};
static const char *motiv_[] = {"One more command. One more feature. Keep building.", "Every great OS started with a blinking cursor.", "Bugs are just features that have not found their purpose yet.", "You are one commit away from something great."};
static const char *ball_[] = {"It is certain.", "Without a doubt.", "Yes, definitely.", "Ask again later.", "Cannot predict now.", "Don't count on it.", "My sources say no.", "Very doubtful."};
static const char *cols_[] = {"cyan", "ice blue", "aurora green", "sunset orange", "glacier white", "midnight blue", "coral pink", "violet", "amber", "mint"};
static const char *pk_(const char **a, int n) { return a[rnd_() % (u32)n]; }
static int gn_t, gn_n;
static int h_fun(const char *n) {
    int a, b;
    if (IS("dice")) { int s = 6; if (ntok > 1 && (!geti(1, &s) || s < 2 || s > 1000)) { usage(); return 1; } outn((int)(rnd_() % (u32)s) + 1); }
    else if (IS("roll")) {
        NEED(1); int cnt = 0, sd = 0; const char *p = tk[1]; while (*p >= '0' && *p <= '9') cnt = cnt * 10 + (*p++ - '0');
        if (lw(*p) != 'd') { usage(); return 1; } p++; while (*p >= '0' && *p <= '9') sd = sd * 10 + (*p++ - '0');
        if (cnt < 1 || cnt > 20 || sd < 2 || sd > 1000 || *p) { usage(); return 1; }
        int tot = 0; t_puts(" ", A_DIM); for (int i = 0; i < cnt; i++) { int v = (int)(rnd_() % (u32)sd) + 1; tot += v; t_num((u32)v, 0); t_putc(' ', A_TXT); }
        t_puts("  total ", A_DIM); t_num((u32)tot, 0); t_putc('\n', A_TXT);
    }
    else if (IS("coin")) outs(rnd_() & 1 ? "Heads" : "Tails");
    else if (IS("8ball")) outs(pk_(ball_, NARR(ball_)));
    else if (IS("random")) {
        int lo = 1, hi = 100; if (ntok == 2) { if (!geti(1, &hi)) { usage(); return 1; } } else if (ntok > 2) { if (!geti(1, &lo) || !geti(2, &hi)) { usage(); return 1; } }
        if (hi < lo) { int t = lo; lo = hi; hi = t; } if (hi - lo < 0 || hi - lo > 2000000000) { err("Range too big"); return 1; }
        outn(lo + (int)(rnd_() % (u32)(hi - lo + 1)));
    }
    else if (IS("pick")) { NEED(1); outs(tk[1 + (int)(rnd_() % (u32)(ntok - 1))]); }
    else if (IS("shuffle")) {
        NEED(1); int ord[16]; for (int i = 0; i < ntok - 1; i++) ord[i] = i + 1;
        for (int i = ntok - 2; i > 0; i--) { int j = (int)(rnd_() % (u32)(i + 1)); int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
        t_puts(" ", A_DIM); for (int i = 0; i < ntok - 1; i++) { t_puts(tk[ord[i]], A_OK); t_putc(' ', A_TXT); } t_putc('\n', A_TXT);
    }
    else if (IS("lottery")) {
        int pk[6], c = 0; while (c < 6) { int v = (int)(rnd_() % 49) + 1, dup = 0; for (int i = 0; i < c; i++) if (pk[i] == v) dup = 1; if (!dup) pk[c++] = v; }
        for (int i = 0; i < 6; i++) for (int j = i + 1; j < 6; j++) if (pk[j] < pk[i]) { int t = pk[i]; pk[i] = pk[j]; pk[j] = t; }
        t_puts(" ", A_DIM); for (int i = 0; i < 6; i++) { t_num((u32)pk[i], 2); t_putc(' ', A_TXT); } t_putc('\n', A_TXT);
    }
    else if (IS("password") || IS("pin")) {
        int pw = IS("password"), len = pw ? 12 : 4; if (ntok > 1 && (!geti(1, &len) || len < 3 || len > 40)) { usage(); return 1; }
        static const char cs[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789!@#$%&*"; char o[44];
        for (int i = 0; i < len; i++) o[i] = pw ? cs[rnd_() % (sizeof cs - 1)] : (char)('0' + rnd_() % 10); o[len] = 0; outs(o);
    }
    else if (IS("joke")) t_say(pk_(jokes_, NARR(jokes_)), A_TXT);
    else if (IS("quote")) t_say(pk_(quotes_, NARR(quotes_)), A_ACC);
    else if (IS("fortune")) t_say(pk_(fortunes_, NARR(fortunes_)), A_WARN);
    else if (IS("funfact")) t_say(pk_(facts_, NARR(facts_)), A_TXT);
    else if (IS("compliment")) t_say(pk_(compl_, NARR(compl_)), A_OK);
    else if (IS("motivate")) t_say(pk_(motiv_, NARR(motiv_)), A_OK);
    else if (IS("cowsay")) {
        NEED(1); const char *r = rest1(); int len = slen(r); if (len > cols - 6) len = cols - 6;
        t_putc(' ', A_TXT); for (int i = 0; i < len + 2; i++) t_putc('_', A_DIM); t_putc('\n', A_TXT);
        t_puts("< ", A_DIM); for (int i = 0; i < len; i++) t_putc(r[i], A_TXT); t_say(" >", A_DIM);
        t_putc(' ', A_TXT); for (int i = 0; i < len + 2; i++) t_putc('-', A_DIM); t_putc('\n', A_TXT);
        t_say("        \\   ^__^", A_ACC); t_say("         \\  (oo)\\_______", A_ACC); t_say("            (__)\\       )\\/\\", A_ACC);
        t_say("                ||----w |", A_ACC); t_say("                ||     ||", A_ACC);
    }
    else if (IS("snowflake")) {
        t_say("      *    .    *", A_ACC); t_say("   .   \\   |   /   .", A_ACC); t_say("  *  -- \\  |  / --  *", A_ACC);
        t_say("   ----- --(*)-- -----", A_TXT); t_say("  *  -- /  |  \\ --  *", A_ACC); t_say("   .   /   |   \\   .", A_ACC); t_say("      *    .    *", A_ACC);
    }
    else if (IS("penguin")) {
        t_say("    .--.", A_TXT); t_say("   |o_o |", A_TXT); t_say("   |:_/ |", A_TXT); t_say("  //   \\ \\", A_ACC); t_say(" (|     | )", A_ACC); t_say("/'\\_   _/`\\", A_ACC); t_say("\\___)=(___/", A_ACC);
    }
    else if (IS("heart")) {
        t_say("   ***   ***", A_ERR); t_say("  ***** *****", A_ERR); t_say("  ***********", A_ERR); t_say("   *********", A_ERR); t_say("     *****", A_ERR); t_say("       *", A_ERR);
    }
    else if (IS("mountain")) {
        t_say("          /\\", A_ACC); t_say("         /  \\     /\\", A_ACC); t_say("    /\\  /    \\   /  \\", A_TXT); t_say("   /  \\/      \\_/    \\", A_TXT); t_say("  /    \\              \\", A_DIM); t_say(" ~~~~~~~~~~~~~~~~~~~~~~~~~", A_DIM);
    }
    else if (IS("rps")) {
        NEED(1); char c = lw(tk[1][0]); int me = c == 'r' ? 0 : c == 'p' ? 1 : c == 's' ? 2 : -1; if (me < 0) { usage(); return 1; }
        int cpu = (int)(rnd_() % 3); static const char *nm[3] = {"rock", "paper", "scissors"};
        t_puts(" you: ", A_DIM); t_puts(nm[me], A_TXT); t_puts("   PixelBeeOS: ", A_DIM); t_puts(nm[cpu], A_TXT);
        if (me == cpu) { t_say("   -> draw", A_WARN); notify("Draw!", 0); } else if ((me + 1) % 3 == cpu) { t_say("   -> you lose", A_ERR); notify("PixelBeeOS wins", 2); } else { t_say("   -> you win!", A_OK); notify("You win!", 1); }
    }
    else if (IS("guess")) {
        if (!gn_t) { gn_t = (int)(rnd_() % 100) + 1; gn_n = 0; if (ntok < 2) { t_say(" I'm thinking of a number 1..100. Type: guess N", A_ACC); return 1; } }
        if (ntok < 2) { t_say(" Game on - type: guess N", A_DIM); return 1; }
        if (!geti(1, &a)) { usage(); return 1; } gn_n++;
        if (a == gn_t) { t_puts(" Correct! tries: ", A_OK); t_num((u32)gn_n, 0); t_putc('\n', A_TXT); notify("You got it!", 1); gn_t = 0; }
        else t_say(a < gn_t ? " Higher" : " Lower", A_WARN);
    }
    else if (IS("hello")) {
        int hh, mm, ss, dd, mo, yy; rtc_read(&hh, &mm, &ss, &dd, &mo, &yy);
        t_puts(hh < 5 ? " Good night" : hh < 12 ? " Good morning" : hh < 18 ? " Good afternoon" : " Good evening", A_ACC); t_say(", Vin. Welcome to PixelBeeOS!", A_ACC);
    }
    else if (IS("luck")) { NEED(1); u32 h = 5381; for (const char *r = rest1(); *r; r++) h = h * 33 + (u8)lw(*r); t_puts(" Lucky number: ", A_DIM); t_num(h % 100, 0); t_putc('\n', A_TXT); }
    else if (IS("yesno")) outs((rnd_() % 3) == 0 ? "Maybe" : ((rnd_() & 1) ? "Yes" : "No"));
    else if (IS("colorpick")) outs(pk_(cols_, NARR(cols_)));
    else { (void)b; return 0; }
    return 1;
}

/* ---------------- system, variables, todo, help system ---------------- */
#ifdef HOSTTEST
static void cpu_brand(char *o) { scpy(o, "Host Test CPU", 48); }
#else
static void cpuid_(u32 leaf, u32 *a, u32 *b, u32 *c, u32 *d) { __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0)); }
static void cpu_brand(char *o) {
    u32 r[4]; cpuid_(0x80000000u, &r[0], &r[1], &r[2], &r[3]); int p = 0;
    if (r[0] >= 0x80000004u) { for (u32 l = 0x80000002u; l <= 0x80000004u; l++) { cpuid_(l, &r[0], &r[1], &r[2], &r[3]); for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) o[p++] = (char)(r[i] >> (8 * k)); } }
    else { cpuid_(0, &r[0], &r[1], &r[2], &r[3]); u32 v[3] = {r[1], r[3], r[2]}; for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) o[p++] = (char)(v[i] >> (8 * k)); }
    o[p] = 0; int s = 0; while (o[s] == ' ') s++; if (s) { int q = 0; while (o[s]) o[q++] = o[s++]; o[q] = 0; }
}
#endif
static char vname[16][16], vval[16][48];
static int var_find(const char *nm) { for (int i = 0; i < 16; i++) if (vname[i][0] && ci_eq(vname[i], nm)) return i; return -1; }
static char vb[200];
static const char *var_lookup(const char *nm) {                  /* %NAME% expansion */
    int i = var_find(nm); if (i >= 0) return vval[i];
    if (ci_eq(nm, "CD")) { path_str(vb); return vb; }
    if (ci_eq(nm, "USERNAME")) return "vin";
    if (ci_eq(nm, "COMPUTERNAME")) return "PIXELBEE-PC";
    int hh, mm, ss, dd, mo, yy; rtc_read(&hh, &mm, &ss, &dd, &mo, &yy);
    if (ci_eq(nm, "TIME")) { vb[0] = '0' + hh / 10; vb[1] = '0' + hh % 10; vb[2] = ':'; vb[3] = '0' + mm / 10; vb[4] = '0' + mm % 10; vb[5] = ':'; vb[6] = '0' + ss / 10; vb[7] = '0' + ss % 10; vb[8] = 0; return vb; }
    if (ci_eq(nm, "DATE")) { fmt_base((u32)yy, 10, vb); int p = slen(vb); vb[p++] = '-'; vb[p++] = '0' + mo / 10; vb[p++] = '0' + mo % 10; vb[p++] = '-'; vb[p++] = '0' + dd / 10; vb[p++] = '0' + dd % 10; vb[p] = 0; return vb; }
    if (ci_eq(nm, "RANDOM")) { fmt_base(rnd_() % 32768, 10, vb); return vb; }
    return 0;
}
static char xl[320];
static void expand_vars(const char *in) {
    int o = 0;
    while (*in && o < 300) {
        if (*in == '%') {
            const char *e = in + 1; char nm[16]; int k = 0;
            while (*e && k < 15 && ((*e >= 'A' && *e <= 'Z') || (*e >= 'a' && *e <= 'z') || (*e >= '0' && *e <= '9') || *e == '_')) nm[k++] = *e++;
            nm[k] = 0;
            if (k && *e == '%') { const char *v = var_lookup(nm); if (v) { while (*v && o < 300) xl[o++] = *v++; in = e + 1; continue; } }
        }
        xl[o++] = *in++;
    }
    xl[o] = 0;
}
static char todo_t[8][48]; static u8 todo_d[8], todo_u[8]; static char note_t[8][64]; static int note_n;
static void list_names(int filter, const char *pre) {
    int cnt = 0; t_putc(' ', A_TXT);
    for (int i = 0; i < NCMDS; i++) {
        const Cmd *c = &cmds[i]; int ok = filter < 0 ? 1 : (filter == 99 ? 1 : (c->cat == filter || (filter == C_M1 && c->cat == C_M2)));
        if (pre) { int pl = slen(pre), m = 1; for (int k = 0; k < pl; k++) if (lw(c->name[k]) != lw(pre[k])) { m = 0; break; } ok = m && slen(c->name) >= pl; }
        if (!ok) continue;
        int L = slen(c->name); if (cx_ + L + 2 >= cols) t_puts("\n ", A_TXT);
        t_puts(c->name, c->cat == C_ALI ? A_DIM : (c->cat == C_LEG ? A_ACC : A_TXT)); t_putc(' ', A_TXT); cnt++;
    }
    t_putc('\n', A_TXT); t_puts(" ", A_DIM); t_num((u32)cnt, 0); t_say(" command(s)", A_DIM);
}
static int h_sys(const char *n) {
    const char *r = rest1();
    if (IS("whoami")) outs("vin");
    else if (IS("hostname")) outs("PIXELBEE-PC");
    else if (IS("pwd")) { char p[200]; path_str(p); outs(p); }
    else if (IS("drives")) { int u = 0; for (int k = 1; k < NODES; k++) if (nd[k].used) u++; t_puts(" C:\\  RAM disk  ", A_ACC); t_num((u32)u, 0); t_say(" items", A_DIM); }
    else if (IS("vol")) outs("PIXELBEE-RAM");
    else if (IS("res")) { t_puts(" = ", A_DIM); t_num(W, 0); t_putc('x', A_TXT); t_num(H, 0); t_say("  32-bit color", A_DIM); }
    else if (IS("cpu")) { char b[56]; cpu_brand(b); outs(b); }
    else if (IS("ram")) { t_puts(" = ", A_DIM); t_num(mem_kb / 1024, 0); t_say(" MB", A_OK); }
    else if (IS("color")) {
        static const struct { const char *nm; u32 c; } cl[] = {{"cyan", 0x6FE3FF}, {"green", 0x8FFFC4}, {"pink", 0xFF8FA3}, {"yellow", 0xFFE08A}, {"blue", 0x7FB2FF}, {"orange", 0xFFB454}, {"purple", 0xC9A0FF}, {"white", 0xFFFFFF}};
        int which = A_ACC, k = 1; if (ntok > 1 && ci_eq(tk[1], "text")) { which = A_TXT; k = 2; } else if (ntok > 1 && ci_eq(tk[1], "accent")) k = 2;
        if (k >= ntok) { t_puts(" Colors:", A_DIM); for (u32 i = 0; i < 8; i++) { t_putc(' ', A_TXT); t_puts(cl[i].nm, A_TXT); } t_putc('\n', A_TXT); return 1; }
        for (u32 i = 0; i < 8; i++) if (ci_eq(cl[i].nm, tk[k])) { pal[which] = cl[i].c; dirty = 1; t_say(" Color changed", A_ACC); notify("Color changed", 1); return 1; }
        err("Unknown color"); 
    }
    else if (IS("prompt")) {
        NEED(1);
        if (ci_eq(tk[1], "short")) prompt_mode = 1; else if (ci_eq(tk[1], "full")) prompt_mode = 0;
        else if (ci_eq(tk[1], "text") && ntok > 2) { scpy(prompt_txt, trest[2], 24); prompt_mode = 2; } else { usage(); return 1; }
        t_say(" Prompt changed", A_OK);
    }
    else if (IS("title")) { NEED(1); scpy(wtitle, r, 36); build_scene(); dirty = 1; notify("Title changed", 1); }
    else if (IS("glass")) { int v; NEED(1); if (!geti(1, &v) || v < 40 || v > 230) { usage(); return 1; } win_alpha = v; build_scene(); dirty = 1; notify("Glass updated", 1); }
    else if (IS("blur")) { int v; NEED(1); if (!geti(1, &v) || v < 0 || v > 14) { usage(); return 1; } blur_r = v; build_scene(); dirty = 1; notify("Blur updated", 1); }
    else if (IS("set")) {
        if (ntok < 2) { usage(); return 1; }
        int i = var_find(tk[1]); if (i < 0) for (int k = 0; k < 16; k++) if (!vname[k][0]) { i = k; break; }
        if (i < 0) { err("Variable table is full"); return 1; }
        if (slen(tk[1]) > 15) { err("Name too long"); return 1; }
        scpy(vname[i], tk[1], 16); scpy(vval[i], ntok > 2 ? trest[2] : "", 48); t_say(" Variable saved  (use %NAME%)", A_OK);
    }
    else if (IS("get")) { NEED(1); int i = var_find(tk[1]); if (i < 0) err("No such variable"); else outs(vval[i]); }
    else if (IS("vars")) { int c = 0; for (int i = 0; i < 16; i++) if (vname[i][0]) { t_puts(" ", A_DIM); t_puts(vname[i], A_ACC); t_puts(" = ", A_DIM); t_say(vval[i], A_TXT); c++; } if (!c) t_say(" (no variables)", A_DIM); }
    else if (IS("unset")) { NEED(1); int i = var_find(tk[1]); if (i < 0) err("No such variable"); else { vname[i][0] = 0; t_say(" Removed", A_OK); } }
    else if (IS("todo")) {
        int v;
        if (ntok > 2 && ci_eq(tk[1], "add")) { for (int i = 0; i < 8; i++) if (!todo_u[i]) { todo_u[i] = 1; todo_d[i] = 0; scpy(todo_t[i], trest[2], 48); t_say(" Added", A_OK); notify("To-do added", 1); return 1; } err("List is full (8)"); return 1; }
        if (ntok > 2 && (ci_eq(tk[1], "done") || ci_eq(tk[1], "del")) && geti(2, &v) && v >= 1 && v <= 8 && todo_u[v - 1]) { if (ci_eq(tk[1], "done")) { todo_d[v - 1] = 1; notify("Done!", 1); } else todo_u[v - 1] = 0; t_say(" OK", A_OK); return 1; }
        if (ntok > 1 && ci_eq(tk[1], "clear")) { for (int i = 0; i < 8; i++) todo_u[i] = 0; t_say(" Cleared", A_OK); return 1; }
        int c = 0; for (int i = 0; i < 8; i++) if (todo_u[i]) { t_puts(todo_d[i] ? " [x] " : " [ ] ", todo_d[i] ? A_OK : A_ACC); t_num((u32)i + 1, 0); t_putc(' ', A_TXT); t_say(todo_t[i], todo_d[i] ? A_DIM : A_TXT); c++; }
        if (!c) t_say(" Nothing to do. Try: todo add buy milk", A_DIM);
    }
    else if (IS("note")) { NEED(1); if (note_n >= 8) { for (int i = 1; i < 8; i++) scpy(note_t[i - 1], note_t[i], 64); note_n = 7; } scpy(note_t[note_n++], r, 64); t_say(" Note saved", A_OK); notify("Note saved", 1); }
    else if (IS("notes")) {
        if (ntok > 1 && ci_eq(tk[1], "clear")) { note_n = 0; t_say(" Notes cleared", A_OK); return 1; }
        if (!note_n) t_say(" (no notes)", A_DIM); for (int i = 0; i < note_n; i++) { t_puts(" - ", A_DIM); t_say(note_t[i], A_TXT); }
    }
    else if (IS("format")) {
        if (ntok < 2 || !ci_eq(tk[1], "yes")) { t_say(" This erases every file. Type: format yes", A_WARN); return 1; }
        for (int i = 1; i < NODES; i++) nd[i].used = 0; cwd = 0; t_say(" RAM disk wiped", A_OK); notify("RAM disk wiped", 0);
    }
    else if (IS("status")) {
        int hh, mm, ss, dd, mo, yy, u = 0; rtc_read(&hh, &mm, &ss, &dd, &mo, &yy); for (int k = 1; k < NODES; k++) if (nd[k].used) u++;
        t_puts(" ", A_DIM); t_pad2(hh); t_putc(':', A_DIM); t_pad2(mm); t_puts("  up ", A_DIM); t_num(cur_ms / 60000, 0); t_puts(" min  files ", A_DIM); t_num((u32)u, 0);
        t_puts("  theme ", A_DIM); t_say(themes[theme_i].name, A_ACC);
    }
    else if (IS("commands")) {
        int np = 0, na = 0; for (int i = 0; i < NCMDS; i++) { if (cmds[i].cat == C_ALI) na++; else np++; }
        if (ntok < 2) {
            t_puts(" PixelBeeOS has ", A_ACC); t_num((u32)(np + na), 0); t_puts(" commands  (", A_ACC); t_num((u32)np, 0); t_puts(" main + ", A_ACC); t_num((u32)na, 0); t_say(" shortcuts)", A_ACC);
            static const char *grp[] = {"core", "math", "convert", "units", "time", "text", "fun", "system", "files", "alias"};
            static const u8 gc[] = {C_LEG, C_M1, C_BASE, C_UNIT, C_TIME, C_TEXT, C_FUN, C_SYS, C_FILE, C_ALI};
            for (int g = 0; g < 10; g++) { int c = 0; for (int i = 0; i < NCMDS; i++) if (cmds[i].cat == gc[g] || (g == 1 && cmds[i].cat == C_M2)) c++; t_puts("  ", A_TXT); t_puts(grp[g], A_ACC); t_puts(": ", A_DIM); t_num((u32)c, 0); if (g % 5 == 4) t_putc('\n', A_TXT); }
            t_say(" commands CATEGORY | commands PREFIX | commands all | man NAME | apropos WORD", A_DIM);
        } else if (ci_eq(tk[1], "all")) list_names(99, 0);
        else {
            static const char *grp[] = {"core", "math", "convert", "units", "time", "text", "fun", "system", "files", "alias"};
            static const u8 gc[] = {C_LEG, C_M1, C_BASE, C_UNIT, C_TIME, C_TEXT, C_FUN, C_SYS, C_FILE, C_ALI};
            for (int g = 0; g < 10; g++) if (ci_eq(tk[1], grp[g])) { list_names(gc[g], 0); return 1; }
            list_names(-1, tk[1]);
        }
    }
    else if (IS("man")) {
        NEED(1); const Cmd *c = cmd_lookup(tk[1]);
        if (!c) { err("No such command"); return 1; }
        t_puts(" ", A_DIM); t_say(c->desc, A_ACC); t_puts(" category: ", A_DIM); t_say(cat_name[c->cat], A_TXT);
    }
    else if (IS("apropos")) {
        NEED(1); int c = 0;
        for (int i = 0; i < NCMDS; i++) if (ci_has(cmds[i].desc, tk[1])) { if (c < 14) { t_puts(" ", A_DIM); t_say(cmds[i].desc, A_TXT); } c++; }
        if (!c) t_say(" Nothing matches", A_DIM); else if (c > 14) { t_puts(" ...and ", A_DIM); t_num((u32)(c - 14), 0); t_say(" more", A_DIM); }
    }
    else if (IS("surprise")) { const Cmd *c; do c = &cmds[rnd_() % (u32)NCMDS]; while (c->cat <= C_ALI); t_puts(" Try: ", A_DIM); t_say(c->desc, A_ACC); }
    else if (IS("textsharp")) { NEED(1); if (ci_eq(tk[1], "on")) sharp_text = 1; else if (ci_eq(tk[1], "off")) sharp_text = 0; else { usage(); return 1; } build_scene(); dirty = 1; t_say(sharp_text ? " Crisp text on." : " Crisp text off.", A_OK); }
    else if (IS("deskicons")) { NEED(1); if (ci_eq(tk[1], "on")) desk_icons = 1; else if (ci_eq(tk[1], "off")) desk_icons = 0; else { usage(); return 1; } dirty = 1; t_say(desk_icons ? " Desktop icons shown." : " Desktop icons hidden.", A_OK); }
    else if (IS("lock")) ux_lock_now();
    else if (IS("passwd")) ux_passwd_start();
    else if (IS("autolock")) { int v; NEED(1); if (!geti(1, &v) || v < 0 || v > 240) { usage(); return 1; } autolock_min = v; t_say(v ? " Auto-lock is on (needs a password: passwd)." : " Auto-lock is off.", A_OK); }
    else if (IS("hotcorners")) { NEED(1); if (ci_eq(tk[1], "on")) hc_mode = 1; else if (ci_eq(tk[1], "off")) hc_mode = 0; else if (ci_eq(tk[1], "lock")) hc_mode = 2; else { usage(); return 1; }
        t_say(" TL = Mission Control, TR = Notes, BL = show desktop, BR = lock (only in 'lock' mode).", A_DIM); }
    else if (IS("mission")) mc_open = 1;
    else if (IS("snap")) { NEED(1); if (ci_eq(tk[1], "left")) ux_snap_top(0); else if (ci_eq(tk[1], "right")) ux_snap_top(1); else if (ci_eq(tk[1], "full")) { if (nwin > 0 && !win_[nwin - 1].max) win_toggle_max(win_[nwin - 1].app); } else { usage(); return 1; } }
    else if (IS("security")) {
        t_puts(" Screen lock : ", A_DIM); t_say(pw_set ? "password set (this session only)" : "no password - type passwd", pw_set ? A_OK : A_WARN);
        t_puts(" Auto-lock   : ", A_DIM); t_say(autolock_min ? "on while a password is set" : "off", A_TXT);
        t_say(" Not protected: files are in RAM (no disk, no encryption), and programs are not isolated from each other.", A_WARN);
        t_say(" This is a screen lock against casual access, not a hardened security system.", A_DIM); }
    else if (IS("paint")) app_open_paint();
    else if (IS("mines")) app_open_mines();
    else if (IS("lspci")) cmd_lspci(0);
    else if (IS("look")) { NEED(1);
        if (ci_eq(tk[1], "glass")) { flat_ui = 0; win_alpha = 130; blur_r = 9; } else if (ci_eq(tk[1], "clear")) { flat_ui = 0; win_alpha = 85; blur_r = 11; } else if (ci_eq(tk[1], "sharp") || ci_eq(tk[1], "flat")) { flat_ui = 1; win_alpha = 225; blur_r = 0; } else { usage(); return 1; }
        build_scene(); dirty = 1; notify("Style updated", 1); }
    else if (IS("vsync")) {
        if (ntok > 1 && ci_eq(tk[1], "on")) vsync_on = 1; else if (ntok > 1 && ci_eq(tk[1], "off")) vsync_on = 0;
        t_puts(" Vsync: ", A_DIM); t_say(!vsync_ok ? "not available on this display (no retrace signal found)" : (vsync_on ? "on" : "off"), vsync_ok && vsync_on ? A_OK : A_WARN);
    }
    else if (IS("display")) { char nb[12]; t_puts(" Screen mode: ", A_DIM); fmt_base(W, 10, nb); t_puts(nb, A_ACC); t_puts(" x ", A_DIM); fmt_base(H, 10, nb); t_puts(nb, A_ACC); t_puts("  (", A_DIM); fmt_base(fb_bpp, 10, nb); t_puts(nb, A_DIM); t_puts("-bit colour, row size ", A_DIM); fmt_base(PITCH, 10, nb); t_puts(nb, A_DIM); t_say(" bytes)", A_DIM);
        t_say(" If text looks soft, this is not your laptop screen's native size (the panel stretches it).", A_DIM); t_say(" Native for a VAIO VPCEH is 1366 x 768. Send me this line if it is different.", A_DIM); }
    else if (IS("netcards")) cmd_lspci(1);
    else if (IS("apps")) { if (ntok > 1) { if (!app_launch(tk[1])) err("No such app. Type: apps"); } else app_list(); }
    else if (IS("calculator") || IS("clockapp") || IS("notepad") || IS("filemanager") || IS("snakewin")) app_launch(n);
    else if (IS("mouse")) {
        if (!mouse_seen) t_say(" No mouse movement yet. Click the VM window and move the mouse.", A_WARN);
        else { t_puts(" x ", A_DIM); t_num((u32)mx, 0); t_puts("  y ", A_DIM); t_num((u32)my, 0); t_puts("  buttons: ", A_DIM); t_say(mbtn & 1 ? (mbtn & 2 ? "left+right" : "left") : (mbtn & 2 ? "right" : "none"), A_OK); }
    }
    else if (IS("browser") || IS("web") || IS("browse")) app_open_browser(ntok > 1 ? trest[1] : "vin://home");
    else return 0;
    return 1;
}

/* ---------------- file tools ---------------- */
#define MAXL 256
static int lst[MAXL], lln[MAXL];
static int fget(int k) {
    if (k >= ntok) { usage(); return -1; }
    int idx = resolve(tk[k]);
    if (idx < 0) { char leaf[24]; int par; if (resolve_parent(tk[k], &par, leaf)) { add_txt(leaf); idx = fs_find(par, leaf); } }
    if (idx <= 0) { err("File not found"); return -1; }
    if (nd[idx].isdir) { err("That is a folder"); return -1; }
    return idx;
}
static int split_lines(int idx) {
    int n = 0, st = 0, sz = nd[idx].size;
    for (int i = 0; i <= sz; i++) if (i == sz || fdata[idx][i] == '\n') { if (i == sz && st == sz) break; if (n < MAXL) { lst[n] = st; lln[n] = i - st; n++; } st = i + 1; }
    return n;
}
static void put_line(int idx, int k, u8 a) { for (int i = 0; i < lln[k]; i++) t_putc(fdata[idx][lst[k] + i], a); t_putc('\n', A_TXT); }
static int dir_bytes(int dir) { int t = 0; for (int k = 1; k < NODES; k++) if (nd[k].used && nd[k].parent == dir) t += nd[k].isdir ? dir_bytes(k) : nd[k].size; return t; }
static int line_has(int idx, int k, const char *nee) {
    int nl = slen(nee); if (!nl) return 1;
    for (int s = 0; s + nl <= lln[k]; s++) { int m = 1; for (int j = 0; j < nl; j++) if (lw(fdata[idx][lst[k] + s + j]) != lw(nee[j])) { m = 0; break; } if (m) return 1; }
    return 0;
}
static int line_cmp(int idx, int a, int b) {
    int la = lln[a], lb_ = lln[b], m = la < lb_ ? la : lb_;
    for (int i = 0; i < m; i++) { char x = lw(fdata[idx][lst[a] + i]), y = lw(fdata[idx][lst[b] + i]); if (x != y) return x < y ? -1 : 1; }
    return la - lb_;
}
static char ftmp[FSIZE + 64];
static int h_file(const char *n) {
    int idx, v;
    if (IS("root")) { cwd = 0; t_say(" C:\\", A_ACC); return 1; }
    if (IS("up")) { cwd = nd[cwd].parent < 0 ? 0 : nd[cwd].parent; char p[200]; path_str(p); t_puts(" ", A_DIM); t_say(p, A_ACC); return 1; }
    if (IS("dirs") || IS("files") || IS("countitems") || IS("biggest") || IS("empties") || IS("du")) {
        int nf = 0, ndr = 0, best = -1;
        for (int k = 1; k < NODES; k++) if (nd[k].used && nd[k].parent == cwd) {
            if (nd[k].isdir) { ndr++; if (IS("dirs")) { t_puts("  <DIR>  ", A_ACC); t_say(nd[k].name, A_ACC); } }
            else { nf++; if (IS("files")) { t_puts("  ", A_TXT); t_num((u32)nd[k].size, 6); t_puts(" B  ", A_DIM); t_say(nd[k].name, A_TXT); } if (IS("empties") && !nd[k].size) { t_puts("  ", A_DIM); t_say(nd[k].name, A_TXT); } if (best < 0 || nd[k].size > nd[best].size) best = k; }
        }
        if (IS("countitems")) { t_puts(" ", A_DIM); t_num((u32)nf, 0); t_puts(" file(s), ", A_TXT); t_num((u32)ndr, 0); t_say(" folder(s)", A_TXT); }
        else if (IS("biggest")) { if (best < 0) t_say(" No files here", A_DIM); else { t_puts(" ", A_DIM); t_puts(nd[best].name, A_ACC); t_puts("  ", A_DIM); t_num((u32)nd[best].size, 0); t_say(" bytes", A_DIM); } }
        else if (IS("du")) { t_puts(" ", A_DIM); t_num((u32)dir_bytes(cwd), 0); t_puts(" bytes in ", A_TXT); t_num((u32)(nf + ndr), 0); t_say(" items here", A_TXT); }
        else if (IS("dirs") && !ndr) t_say("  (no folders)", A_DIM); else if (IS("files") && !nf) t_say("  (no files)", A_DIM);
        return 1;
    }
    NEED(1);
    if (IS("touch")) {
        for (int k = 1; k < ntok; k++) { int par; char leaf[24]; if (!resolve_parent(tk[k], &par, leaf) || !valid_name(leaf)) { err("Bad file name"); continue; } add_txt(leaf); if (fs_find(par, leaf) >= 0) continue; if (fs_new(par, leaf, 0) < 0) { err("Disk full"); break; } }
        t_say(" Done", A_OK); notify("Files created", 1);
    }
    else if (IS("mkdirs")) {
        const char *p = tk[1]; int cur = cwd; if (*p == '\\' || *p == '/') { cur = 0; p++; } char seg[24];
        while (*p) { int k = 0; while (*p && *p != '\\' && *p != '/') { if (k < 23) seg[k++] = *p; p++; } seg[k] = 0; if (*p) p++; if (!k) continue; if (!valid_name(seg)) { err("Bad folder name"); return 1; }
            int f = fs_find(cur, seg); if (f < 0) f = fs_new(cur, seg, 1); if (f < 0 || !nd[f].isdir) { err("Can't create that"); return 1; } cur = f; }
        t_say(" Folders ready", A_OK); notify("Folders created", 1);
    }
    else if (IS("rmdir")) { idx = resolve(tk[1]); if (idx <= 0 || !nd[idx].isdir) { err("Folder not found"); return 1; } for (int k = 1; k < NODES; k++) if (nd[k].used && nd[k].parent == idx) { err("Folder is not empty"); return 1; } nd[idx].used = 0; t_say(" Folder removed", A_OK); }
    else if (IS("exists")) { int e = resolve(tk[1]); if (e < 0) { char leaf[24]; int par; if (resolve_parent(tk[1], &par, leaf)) { add_txt(leaf); e = fs_find(par, leaf); } } out_yesno(e >= 0); }
    else if (IS("stat")) {
        idx = resolve(tk[1]); if (idx < 0) { err("Not found"); return 1; } char p[200]; node_path(idx, p);
        t_puts(" Name: ", A_DIM); t_say(idx ? nd[idx].name : "C:\\", A_ACC); t_puts(" Path: ", A_DIM); t_say(p, A_TXT);
        t_puts(" Type: ", A_DIM); t_say(nd[idx].isdir ? "folder" : "text file", A_TXT); t_puts(" Size: ", A_DIM); t_num((u32)(nd[idx].isdir ? dir_bytes(idx) : nd[idx].size), 0); t_say(" bytes", A_TXT);
    }
    else if (IS("size")) { idx = resolve(tk[1]); if (idx < 0) { char leaf[24]; int par; if (resolve_parent(tk[1], &par, leaf)) { add_txt(leaf); idx = fs_find(par, leaf); } } if (idx < 0) { err("Not found"); return 1; } outn(nd[idx].isdir ? dir_bytes(idx) : nd[idx].size); }
    else if (IS("clearfile")) { if ((idx = fget(1)) < 0) return 1; nd[idx].size = 0; t_say(" File emptied", A_OK); }
    else if (IS("lines") || IS("wc") || IS("head") || IS("tail") || IS("catn") || IS("rev")) {
        if ((idx = fget(1)) < 0) return 1; int nl = split_lines(idx);
        if (IS("lines")) outn(nl);
        else if (IS("wc")) { int w = 0, in = 0; for (int i = 0; i < nd[idx].size; i++) { char c = fdata[idx][i]; if (c == ' ' || c == '\n' || c == '\t') in = 0; else if (!in) { in = 1; w++; } } t_puts(" lines ", A_DIM); t_num((u32)nl, 0); t_puts("  words ", A_DIM); t_num((u32)w, 0); t_puts("  bytes ", A_DIM); t_num((u32)nd[idx].size, 0); t_putc('\n', A_TXT); }
        else if (IS("head") || IS("tail")) { int cnt = 5; if (ntok > 2 && (!geti(2, &cnt) || cnt < 1)) { usage(); return 1; } int from = IS("head") ? 0 : (nl > cnt ? nl - cnt : 0), to = IS("head") ? (cnt < nl ? cnt : nl) : nl; for (int k = from; k < to; k++) put_line(idx, k, A_TXT); if (!nl) t_say(" (empty file)", A_DIM); }
        else if (IS("catn")) { for (int k = 0; k < nl; k++) { t_puts(" ", A_DIM); t_num((u32)k + 1, 3); t_puts("  ", A_DIM); put_line(idx, k, A_TXT); } if (!nl) t_say(" (empty file)", A_DIM); }
        else { for (int k = nl - 1; k >= 0; k--) put_line(idx, k, A_TXT); }
    }
    else if (IS("grep")) {
        NEED(2); if ((idx = fget(2)) < 0) return 1; int nl = split_lines(idx), hit = 0;
        for (int k = 0; k < nl; k++) if (line_has(idx, k, tk[1])) { t_puts(" ", A_DIM); t_num((u32)k + 1, 3); t_puts(": ", A_ACC); put_line(idx, k, A_TXT); hit++; }
        if (!hit) t_say(" No matching lines", A_DIM);
    }
    else if (IS("diff")) {
        NEED(2); int a = fget(1), b = a < 0 ? -1 : fget(2); if (a < 0 || b < 0) return 1;
        int na = split_lines(a); static int sa[MAXL], sb[MAXL], la2[MAXL], lb2[MAXL]; for (int i = 0; i < na; i++) { sa[i] = lst[i]; la2[i] = lln[i]; }
        int nb = split_lines(b); for (int i = 0; i < nb; i++) { sb[i] = lst[i]; lb2[i] = lln[i]; }
        int m = na < nb ? na : nb, first = -1;
        for (int i = 0; i < m && first < 0; i++) { if (la2[i] != lb2[i]) first = i; else for (int j = 0; j < la2[i]; j++) if (fdata[a][sa[i] + j] != fdata[b][sb[i] + j]) { first = i; break; } }
        if (first < 0 && na != nb) first = m;
        if (first < 0) t_say(" Files are identical", A_OK); else { t_puts(" First difference on line ", A_WARN); t_num((u32)first + 1, 0); t_putc('\n', A_TXT); }
    }
    else if (IS("join")) {
        NEED(3); int a = fget(1), b = a < 0 ? -1 : fget(2); if (a < 0 || b < 0) return 1;
        int par; char leaf[24]; if (!resolve_parent(tk[3], &par, leaf) || !valid_name(leaf)) { err("Bad output name"); return 1; } add_txt(leaf);
        if (nd[a].size + nd[b].size > FSIZE) { err("Result too big"); return 1; }
        int o = fs_find(par, leaf); if (o >= 0 && nd[o].isdir) { err("That is a folder"); return 1; } if (o < 0) o = fs_new(par, leaf, 0); if (o < 0) { err("Disk full"); return 1; }
        memcpy(ftmp, fdata[a], (unsigned)nd[a].size); memcpy(ftmp + nd[a].size, fdata[b], (unsigned)nd[b].size); int t = nd[a].size + nd[b].size; memcpy(fdata[o], ftmp, (unsigned)t); nd[o].size = t;
        t_puts(" Joined into ", A_OK); t_say(leaf, A_TXT); notify("Files joined", 1);
    }
    else if (IS("replace")) {
        NEED(3); if ((idx = fget(3)) < 0) return 1; int ol = slen(tk[1]), nl = slen(tk[2]), o = 0, cnt = 0; if (!ol) { usage(); return 1; }
        for (int i = 0; i < nd[idx].size;) {
            int m = i + ol <= nd[idx].size; for (int j = 0; m && j < ol; j++) if (fdata[idx][i + j] != tk[1][j]) m = 0;
            if (m) { if (o + nl > FSIZE) { err("Result too big"); return 1; } for (int j = 0; j < nl; j++) ftmp[o++] = tk[2][j]; i += ol; cnt++; }
            else { if (o + 1 > FSIZE) { err("Result too big"); return 1; } ftmp[o++] = fdata[idx][i++]; }
        }
        memcpy(fdata[idx], ftmp, (unsigned)o); nd[idx].size = o; t_puts(" Replaced ", A_OK); t_num((u32)cnt, 0); t_say(" time(s)", A_OK);
    }
    else if (IS("lowerfile") || IS("upperfile")) {
        if ((idx = fget(1)) < 0) return 1; int up = IS("upperfile");
        for (int i = 0; i < nd[idx].size; i++) { char c = fdata[idx][i]; if (up && c >= 'a' && c <= 'z') fdata[idx][i] = c - 32; else if (!up && c >= 'A' && c <= 'Z') fdata[idx][i] = c + 32; }
        t_say(" Done", A_OK);
    }
    else if (IS("sortfile")) {
        if ((idx = fget(1)) < 0) return 1; int nl = split_lines(idx), ord[MAXL];
        for (int i = 0; i < nl; i++) ord[i] = i;
        for (int i = 1; i < nl; i++) { int t = ord[i], j = i - 1; while (j >= 0 && line_cmp(idx, ord[j], t) > 0) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = t; }
        int o = 0; for (int k = 0; k < nl; k++) { if (o + lln[ord[k]] + 1 > FSIZE) { err("Result too big"); return 1; } memcpy(ftmp + o, fdata[idx] + lst[ord[k]], (unsigned)lln[ord[k]]); o += lln[ord[k]]; ftmp[o++] = '\n'; }
        memcpy(fdata[idx], ftmp, (unsigned)o); nd[idx].size = o; t_puts(" Sorted ", A_OK); t_num((u32)nl, 0); t_say(" line(s)", A_OK);
    }
    else if (IS("hexdump")) {
        if ((idx = fget(1)) < 0) return 1; int sz = nd[idx].size < 64 ? nd[idx].size : 64; if (!sz) { t_say(" (empty file)", A_DIM); return 1; }
        for (int row = 0; row < sz; row += 16) {
            char b[12]; fmt_base((u32)row, 16, b); t_puts(" ", A_DIM); for (int p = slen(b); p < 4; p++) t_putc('0', A_DIM); t_puts(b, A_DIM); t_puts("  ", A_DIM);
            for (int c = 0; c < 16; c++) { if (row + c < sz) { fmt_base((u8)fdata[idx][row + c], 16, b); if (!b[1]) { b[1] = b[0]; b[0] = '0'; b[2] = 0; } t_puts(b, A_TXT); } else t_puts("  ", A_TXT); t_putc(' ', A_TXT); }
            t_putc(' ', A_TXT); for (int c = 0; c < 16 && row + c < sz; c++) { char ch = fdata[idx][row + c]; t_putc(ch >= 32 && ch < 127 ? ch : '.', A_ACC); } t_putc('\n', A_TXT);
        }
    }
    else { (void)v; return 0; }
    return 1;
}

/* ---------------- dispatcher, "did you mean", tab completion ---------------- */
static int run_cmd(int cat, const char *n) {
    switch (cat) {
    case C_M1: return h_m1(n); case C_M2: return h_m2(n); case C_BASE: return h_base(n); case C_UNIT: return h_unit(n);
    case C_TIME: return h_time(n); case C_TEXT: return h_text(n); case C_FUN: return h_fun(n); case C_SYS: return h_sys(n); case C_FILE: return h_file(n);
    }
    return 0;
}
static const char *suggest(const char *w) {
    int wl = slen(w), best = 99; const char *bn = 0; if (wl < 2) return 0;
    for (int i = 0; i < NCMDS; i++) {
        const char *c = cmds[i].name; int cl = slen(c); if (cl - wl > 2 || wl - cl > 2 || cl > 20 || wl > 20) continue;
        int d[22][22]; for (int a = 0; a <= wl; a++) d[a][0] = a; for (int b = 0; b <= cl; b++) d[0][b] = b;
        for (int a = 1; a <= wl; a++) for (int b = 1; b <= cl; b++) { int x = d[a - 1][b] + 1, y = d[a][b - 1] + 1, z = d[a - 1][b - 1] + (lw(w[a - 1]) != lw(c[b - 1])); d[a][b] = x < y ? (x < z ? x : z) : (y < z ? y : z); }
        if (d[wl][cl] < best) { best = d[wl][cl]; bn = c; }
    }
    return best <= (wl <= 4 ? 1 : 2) ? bn : 0;
}
static void tab_complete(void) {
    for (int i = 0; i < ll; i++) if (line[i] == ' ') return;
    if (!ll) return;
    int cnt = 0, first = -1, lcp = 0; line[ll] = 0;
    for (int i = 0; i < NCMDS; i++) {
        const char *c = cmds[i].name; int m = 1; for (int k = 0; k < ll; k++) if (lw(c[k]) != lw(line[k])) { m = 0; break; } if (!m || slen(c) < ll) continue;
        if (cnt == 0) { first = i; lcp = slen(c); } else { int k = 0; while (k < lcp && lw(cmds[first].name[k]) == lw(c[k])) k++; lcp = k; } cnt++;
    }
    if (!cnt) return;
    if (cnt == 1) { for (const char *p = cmds[first].name + ll; *p; p++) { line[ll++] = *p; t_putc(*p, A_TXT); } line[ll++] = ' '; t_putc(' ', A_TXT); return; }
    if (lcp > ll) { for (int k = ll; k < lcp; k++) { line[k] = lw(cmds[first].name[k]); t_putc(line[k], A_TXT); } ll = lcp; return; }
    char keep[256]; scpy(keep, line, 256); int kl = ll; t_putc('\n', A_TXT); list_names(-1, keep);
    prompt(); for (int i = 0; i < kl; i++) { line[i] = keep[i]; t_putc(keep[i], A_TXT); } ll = kl;
}
