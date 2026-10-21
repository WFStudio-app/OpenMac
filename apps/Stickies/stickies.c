/* stickies.c - OpenMac Stickies.app (terminal edition)
 * Emulates the classic macOS Stickies: small notes pinned on the desktop.
 * In the terminal each note is a text file in ~/.omac/Stickies/notes/,
 * with metadata (colour, position, creation/modification time) stored in
 * a sidecar .meta file, mirroring how Stickies keeps notes in its bundle.
 * Original code (c) OpenMac project, proprietary license - see ../../LICENSE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_NOTES 512
#define NAME_MAX_LEN 64
#define COLOR_COUNT 7

static const char *COLORS[COLOR_COUNT] = {
    "yellow", "pink", "blue", "green", "orange", "white", "purple"
};

/* ANSI background colours approximating the macOS Sticky palette */
static const char *COLOR_ANSI[COLOR_COUNT] = {
    "\x1b[43m", "\x1b[45m", "\x1b[44m", "\x1b[42m",
    "\x1b[48;5;214m", "\x1b[47;30m", "\x1b[48;5;93m"
};

static char g_root[512]; /* ~/.omac/Stickies */
static int g_no_color = 0;

static void die(const char *msg) {
    fprintf(stderr, "stickies: %s\n", msg);
    exit(1);
}

static void ensure_dirs(void) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s", g_root);
    for (char *q = dir + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; mkdir(dir, 0755); *q = '/'; }
    }
    mkdir(dir, 0755);
    strncat(dir, "/notes", sizeof dir - strlen(dir) - 1);
    mkdir(dir, 0755);
}

static int valid_name(const char *n) {
    size_t len = strlen(n);
    if (len == 0 || len >= NAME_MAX_LEN) return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)n[i];
        if (!isalnum(c) && c != '-' && c != '_') return 0;
    }
    return 1;
}

static void note_path(char *out, size_t cap, const char *name, const char *ext) {
    snprintf(out, cap, "%s/notes/%s%s", g_root, name, ext);
}

/* --- metadata ---------------------------------------------------------- */
typedef struct {
    char color[16];
    int x, y;          /* "pinned" screen position like Stickies window origin */
    long created;      /* epoch seconds */
} Meta;

static void meta_default(Meta *m) {
    strcpy(m->color, "yellow");
    m->x = 0; m->y = 0;
    m->created = (long)time(NULL);
}

static int meta_load(const char *name, Meta *m) {
    char p[1024];
    note_path(p, sizeof p, name, ".meta");
    FILE *f = fopen(p, "r");
    meta_default(m);
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        if (!strcmp(line, "color")) snprintf(m->color, sizeof m->color, "%s", val);
        else if (!strcmp(line, "x")) m->x = atoi(val);
        else if (!strcmp(line, "y")) m->y = atoi(val);
        else if (!strcmp(line, "created")) m->created = atol(val);
    }
    fclose(f);
    return 1;
}

static void meta_save(const char *name, const Meta *m) {
    char p[1024];
    note_path(p, sizeof p, name, ".meta");
    FILE *f = fopen(p, "w");
    if (!f) die("cannot write metadata");
    fprintf(f, "color=%s\nx=%d\ny=%d\ncreated=%ld\n", m->color, m->x, m->y, m->created);
    fclose(f);
}

static int color_index(const char *c) {
    for (int i = 0; i < COLOR_COUNT; i++)
        if (!strcmp(c, COLORS[i])) return i;
    return -1;
}

/* --- listing ----------------------------------------------------------- */
typedef struct { char name[NAME_MAX_LEN]; } NoteEntry;

static int list_notes(NoteEntry *out, int max) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s/notes", g_root);
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t len = strlen(e->d_name);
        if (len > 5 && !strcmp(e->d_name + len - 5, ".text")) {
            if (n < max) {
                memcpy(out[n].name, e->d_name, len - 5);
                out[n].name[len - 5] = '\0';
                n++;
            }
        }
    }
    closedir(d);
    /* sort by name for stable output */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(out[i].name, out[j].name) > 0) {
                NoteEntry t = out[i]; out[i] = out[j]; out[j] = t;
            }
    return n;
}

/* --- commands ---------------------------------------------------------- */
static void usage(void) {
    printf(
"stickies - OpenMac Stickies.app (terminal edition)\n"
"\n"
"Usage:\n"
"  stickies new [name] [-c COLOR]   create a sticky note (reads stdin or -t TEXT)\n"
"  stickies add name \"TEXT\"         append a line to an existing note\n"
"  stickies show name               print a note (coloured like the real app)\n"
"  stickies open                    TUI: browse and read notes (arrows/j/k, q quits)\n"
"  stickies list                    list all notes with colour and date\n"
"  stickies edit name               replace note content from stdin\n"
"  stickies rm name                 delete a note (alias: delete)\n"
"  stickies clear                   delete ALL notes\n"
"  stickies color name COLOR        change note colour\n"
"  stickies move name X Y           set the note's pinned position\n"
"\n"
"Colours: yellow pink blue green orange white purple\n"
"Notes are stored under ~/.omac/Stickies/notes/ as NAME.text + NAME.meta.\n"
"No-color mode: export STICKIES_NO_COLOR=1 or pipe output (auto-detected).\n");
}

static int cmd_new(int argc, char **argv) {
    const char *name = NULL;
    const char *color = "yellow";
    const char *text = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) { color = argv[++i]; }
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) { text = argv[++i]; }
        else if (!name) name = argv[i];
    }
    if (!name) {
        /* auto-generate: next free number, like untitled windows */
        static char gen[NAME_MAX_LEN];
        NoteEntry notes[MAX_NOTES];
        int n = list_notes(notes, MAX_NOTES);
        for (int k = 1; k <= n + 1; k++) {
            snprintf(gen, sizeof gen, "note%d", k);
            int taken = 0;
            for (int i = 0; i < n; i++) if (!strcmp(notes[i].name, gen)) taken = 1;
            if (!taken) { name = gen; break; }
        }
    }
    if (!valid_name(name)) die("invalid note name (letters/digits/-/_ only)");
    if (color_index(color) < 0) die("unknown colour");
    char check[1024];
    note_path(check, sizeof check, name, ".text");
    if (access(check, F_OK) == 0) die("note already exists");
    ensure_dirs();

    char p[1024];
    note_path(p, sizeof p, name, ".text");
    FILE *f = fopen(p, "w");
    if (!f) die("cannot create note");
    if (text) {
        fprintf(f, "%s\n", text);
    } else if (!isatty(STDIN_FILENO)) {
        char buf[4096];
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, stdin)) > 0) fwrite(buf, 1, r, stdout ? f : f);
    }
    fclose(f);
    Meta m; meta_default(&m); strcpy(m.color, color);
    meta_save(name, &m);
    printf("Created sticky '%s' (%s).\n", name, color);
    return 0;
}

static int cmd_add(int argc, char **argv) {
    if (argc < 1) die("add: need note name");
    const char *name = argv[0];
    if (!valid_name(name)) die("invalid note name");
    char p[1024];
    note_path(p, sizeof p, name, ".text");
    FILE *f = fopen(p, "a");
    if (!f) die("no such note");
    if (argc >= 2) {
        for (int i = 1; i < argc; i++) fprintf(f, "%s\n", argv[i]);
    } else if (!isatty(STDIN_FILENO)) {
        char buf[4096]; size_t r;
        while ((r = fread(buf, 1, sizeof buf, stdin)) > 0) fwrite(buf, 1, r, f);
    } else die("add: give TEXT argument or pipe via stdin");
    fclose(f);
    printf("Updated '%s'.\n", name);
    return 0;
}

static void print_sticky(const char *name) {
    char p[1024];
    note_path(p, sizeof p, name, ".text");
    FILE *f = fopen(p, "r");
    if (!f) { fprintf(stderr, "stickies: no such note '%s'\n", name); exit(1); }
    Meta m; meta_load(name, &m);
    int ci = color_index(m.color);
    if (ci < 0) ci = 0;
    char buf[4096];
    if (!g_no_color) {
        /* draw a simple bordered sticky, tinted by colour */
        printf("%s┌────────────────────────────┐%s\x1b[0m\n", COLOR_ANSI[ci], COLOR_ANSI[ci]);
        while (fgets(buf, sizeof buf, f)) {
            buf[strcspn(buf, "\n")] = '\0';
            printf("%s│ %-26s │%s\x1b[0m\n", COLOR_ANSI[ci], buf, COLOR_ANSI[ci]);
        }
        printf("%s└────────────────────────────┘%s\x1b[0m\n", COLOR_ANSI[ci], COLOR_ANSI[ci]);
    } else {
        while (fgets(buf, sizeof buf, f)) fputs(buf, stdout);
    }
    fclose(f);
}

static int cmd_show(int argc, char **argv) {
    if (argc < 1) die("show: need note name");
    print_sticky(argv[0]);
    return 0;
}

static int cmd_list(void) {
    NoteEntry notes[MAX_NOTES];
    int n = list_notes(notes, MAX_NOTES);
    if (n == 0) { printf("(no stickies)\n"); return 0; }
    printf("%-16s %-8s %-10s %s\n", "NAME", "COLOR", "CREATED", "SIZE");
    for (int i = 0; i < n; i++) {
        Meta m; meta_load(notes[i].name, &m);
        char p[1024];
        note_path(p, sizeof p, notes[i].name, ".text");
        struct stat st;
        long sz = stat(p, &st) == 0 ? (long)st.st_size : 0;
        char ts[32];
        time_t t = (time_t)m.created;
        struct tm tmv; localtime_r(&t, &tmv);
        strftime(ts, sizeof ts, "%Y-%m-%d", &tmv);
        printf("%-16s %-8s %-10s %ldB\n", notes[i].name, m.color, ts, sz);
    }
    return 0;
}

static int cmd_rm(int argc, char **argv) {
    if (argc < 1) die("rm: need note name");
    char p[1024], q[1024];
    note_path(p, sizeof p, argv[0], ".text");
    note_path(q, sizeof q, argv[0], ".meta");
    if (remove(p) != 0) die("no such note");
    remove(q);
    printf("Deleted '%s'.\n", argv[0]);
    return 0;
}

static int cmd_clear(void) {
    NoteEntry notes[MAX_NOTES];
    int n = list_notes(notes, MAX_NOTES);
    for (int i = 0; i < n; i++) cmd_rm(1, &(char *[]) { notes[i].name }) , 0;
    printf("Cleared %d note(s).\n", n);
    return 0;
}

static int cmd_color(int argc, char **argv) {
    if (argc < 2) die("color: need NAME COLOR");
    if (color_index(argv[1]) < 0) die("unknown colour");
    Meta m;
    char p[1024];
    note_path(p, sizeof p, argv[0], ".text");
    if (access(p, F_OK) != 0) die("no such note");
    meta_load(argv[0], &m);
    strcpy(m.color, argv[1]);
    meta_save(argv[0], &m);
    printf("'%s' is now %s.\n", argv[0], argv[1]);
    return 0;
}

static int cmd_move(int argc, char **argv) {
    if (argc < 3) die("move: need NAME X Y");
    Meta m;
    char p[1024];
    note_path(p, sizeof p, argv[0], ".text");
    if (access(p, F_OK) != 0) die("no such note");
    meta_load(argv[0], &m);
    m.x = atoi(argv[1]); m.y = atoi(argv[2]);
    meta_save(argv[0], &m);
    printf("'%s' pinned at (%d,%d).\n", argv[0], m.x, m.y);
    return 0;
}

static int cmd_edit(int argc, char **argv) {
    if (argc < 1) die("edit: need note name");
    char p[1024];
    note_path(p, sizeof p, argv[0], ".text");
    if (access(p, F_OK) != 0) die("no such note");
    if (isatty(STDIN_FILENO)) die("edit: pipe replacement text via stdin");
    FILE *f = fopen(p, "w");
    if (!f) die("cannot open note");
    char buf[4096]; size_t r;
    while ((r = fread(buf, 1, sizeof buf, stdin)) > 0) fwrite(buf, 1, r, f);
    fclose(f);
    printf("Rewrote '%s'.\n", argv[0]);
    return 0;
}

/* --- TUI browser ------------------------------------------------------- */
static int cmd_open(void) {
    NoteEntry notes[MAX_NOTES];
    int n = list_notes(notes, MAX_NOTES);
    if (n == 0) { printf("(no stickies — create one with 'stickies new')\n"); return 0; }
    if (!isatty(STDIN_FILENO)) die("open: interactive mode needs a TTY (use 'show' or 'list')");
    int sel = 0, scroll = 0;
    /* raw mode via stty so we don't link termios-heavy code; simpler: read lines */
    printf("Stickies: %zu note(s). Enter number to view, 'q' quit.\n", (size_t)n);
    for (int i = 0; i < n; i++)
        printf("  [%d] %s\n", i + 1, notes[i].name);
    char line[64];
    (void)scroll;
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line, "q") || !strcmp(line, "quit")) break;
        if (*line == '\0') continue;
        int idx = atoi(line) - 1;
        if (idx >= 0 && idx < n) {
            printf("\n");
            print_sticky(notes[idx].name);
            printf("\n");
        } else printf("pick 1..%d or q\n", n);
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(g_root, sizeof g_root, "%s/.omac/Stickies", home);
    const char *ov = getenv("STICKIES_ROOT");
    if (ov && *ov) snprintf(g_root, sizeof g_root, "%s", ov);
    if (getenv("STICKIES_NO_COLOR") || !isatty(STDOUT_FILENO)) g_no_color = 1;

    if (argc < 2) { usage(); return 0; }
    const char *cmd = argv[1];
    int ac = argc - 2;
    char **av = argv + 2;

    if (!strcmp(cmd, "new"))    return cmd_new(ac, av);
    if (!strcmp(cmd, "add"))    return cmd_add(ac, av);
    if (!strcmp(cmd, "show"))   return cmd_show(ac, av);
    if (!strcmp(cmd, "list"))   return cmd_list();
    if (!strcmp(cmd, "open"))   return cmd_open();
    if (!strcmp(cmd, "edit"))   return cmd_edit(ac, av);
    if (!strcmp(cmd, "rm") || !strcmp(cmd, "delete")) return cmd_rm(ac, av);
    if (!strcmp(cmd, "clear"))  return cmd_clear();
    if (!strcmp(cmd, "color"))  return cmd_color(ac, av);
    if (!strcmp(cmd, "move"))   return cmd_move(ac, av);
    if (!strcmp(cmd, "--help") || !strcmp(cmd, "help")) { usage(); return 0; }
    fprintf(stderr, "stickies: unknown command '%s' (try --help)\n", cmd);
    return 1;
}
