#include "eadk.h"
#include "topics.h"
#include "physics.h"
#include "units.h"
#include "solvers.h"
#include "mathview.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ascii_font.inc"

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumAI";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

enum {
    White = 0xffff, Ink = 0x2104, Grey = 0x6b4d, Light = 0xef7d,
    Line = 0xd6ba, Gold = 0xfda6, PaleGold = 0xffb8, Blue = 0x32b8,
    HistoryTint = 0xf7bf, FocusTint = 0xd6bc, ModalBar = 0x630e, PlotRed = 0xe9a7
};
static const char higher_label[] = "(higher only)";
static const char physics_sheet_title[] = "AQA GCSE Physics equation sheet";
static const char chemistry_sheet_title[] = "AQA GCSE Chemistry equation sheet";

enum { CHAR_WIDTH = 7, CHAR_HEIGHT = 14, TEXT_WIDTH = 43, FOOTER = 226,
       HISTORY_LIMIT = 5, WRAP_LIMIT = 160, CAS_TOOL_COUNT = 3, FAVOURITE_LIMIT = 48,
       CALCULATION_LIMIT = 12, EXTRA_LIMIT = 8 };

typedef enum { Prompt, Toolbox, Chapters, Topics, TopicMenu, Examples, Form, Reading,
               Result, CasMenu, History, Settings, Shortcuts, PhysicsList, PhysicsDetail,
               PhysicsSubjects, PhysicsForm, ScienceResult,
               UnitGroups, UnitFrom, UnitTo, UnitValue, UnitResult, Favourites,
               ToolForm, ToolOutput, DecimalPlaces, CalcHistory, CalcExtras } Page;
typedef struct { int row, first; } Selection;
typedef struct {
    char text[EXPRESSION_SIZE];
    int cursor, anchor;
} Field;
typedef struct {
    char question[EXPRESSION_SIZE];
    Operation operation;
    Solution solution;
} HistoryEntry;
typedef struct {
    int length;
    bool highlight;
    const char *source;
} WrappedLine;
typedef enum { TrigSolver, TriangleSolver, PhysicsRearrange, ExtraToolCount } ExtraTool;
typedef struct { int course, chapter, row; bool equation, chemistry; } Favourite;
typedef struct { char input[EXPRESSION_SIZE]; CalculationValue result; } CalcEntry;
typedef struct { const char *label; char text[EXPRESSION_SIZE], reuse[EXPRESSION_SIZE]; } ExtraResult;
static const char *const extra_tool_names[] = {"Trigonometric equation solver", "Triangle solver",
                                                "Physics formula rearranger"};

static Page page, form_parent, result_parent, settings_parent, reading_parent;
static Selection course_selection, chapter_selection, topic_selection, topic_action, cas_selection, settings_selection;
static Selection history_selection, example_selection, physics_selection, physics_subject;
static Selection unit_group, unit_from, unit_to;
static Selection favourite_selection, decimal_selection;
static Field physics_fields[SCIENCE_VARIABLES], unit_input;
static int physics_focus, physics_first, physics_offset, science_result_offset;
static bool chemistry_sheet, unit_input_error, unit_copied;
static bool physics_answer_ok;
static char physics_notes[SCIENCE_REFERENCE_SIZE], unit_expression[112], unit_work[112], unit_number[32];
static ScienceWorking science_work;
static Field prompt, forms[6];
static const char *form_labels[6];
static int field_count, active_field, read_offset, read_length, shortcuts_offset;
static const Topic *form_topic;
static Operation form_operation, result_operation;
static ExtraTool extra_tool;
static Solution tool_solution;
static int tool_first, tool_offset;
static Favourite favourites[FAVOURITE_LIMIT];
static int favourite_count;
static bool favourite_origin;
static const char *pin_notice;
static CalcEntry calculations[CALCULATION_LIMIT];
static ExtraResult extra_results[EXTRA_LIMIT];
static int calc_count, calc_row, calc_first, calc_column, extras_count, extras_row, extras_first;
static HistoryEntry history[HISTORY_LIMIT];
static Solution solution;
static int history_count;
static bool has_result, degrees = true, decimal, show_steps = true, shift;
static int decimal_places = 5;
static enum { AlphaOff, AlphaOnce, AlphaLocked } alpha;
static char clipboard[EXPRESSION_SIZE];
static char ans[EXPRESSION_SIZE], result_question[EXPRESSION_SIZE];
static const char *read_content, *read_title;
static WrappedLine wrapped[WRAP_LIMIT];

static void rectangle(int x, int y, int width, int height, eadk_color_t colour) {
    if (width <= 0 || height <= 0) return;
    eadk_display_push_rect_uniform((eadk_rect_t){x, y, width, height}, colour);
}

static const uint8_t *font_glyph(unsigned char ch) {
    if (ch >= MathMultiply && ch <= MathApprox) return glyphs[95+ch-MathMultiply];
    if (ch < ' ' || ch > '~') ch = '?';
    return glyphs[ch-' '];
}

static unsigned blend_channel(unsigned ink, unsigned paper, unsigned bits, unsigned alpha) {
    unsigned padding = 8 - bits;
    ink = (ink << padding) | (ink >> (bits - padding));
    paper = (paper << padding) | (paper >> (bits - padding));
    return (ink * alpha + paper * (256 - alpha)) >> (8 + padding);
}

static eadk_color_t blend_colour(eadk_color_t ink, eadk_color_t paper, unsigned alpha) {
    unsigned red = blend_channel(ink >> 11, paper >> 11, 5, alpha);
    unsigned green = blend_channel((ink >> 5) & 63, (paper >> 5) & 63, 6, alpha);
    unsigned blue = blend_channel(ink & 31, paper & 31, 5, alpha);
    return (red << 11) | (green << 5) | blue;
}

// Use the EADK display syscall directly. Firmware text drawing requires
// a separate userland trampoline supplied by the app installer.
static void draw_text(int x, int y, const char *text, eadk_color_t ink,
                      eadk_color_t paper, bool bold) {
    enum { GLYPHS_PER_TILE = 8, TILE_WIDTH = GLYPHS_PER_TILE * CHAR_WIDTH };
    if (x < 0 || x >= EADK_SCREEN_WIDTH || y < 0 || y + CHAR_HEIGHT > EADK_SCREEN_HEIGHT) return;
    int remaining = (EADK_SCREEN_WIDTH - x) / CHAR_WIDTH;
    if (!remaining || !*text) return;
    eadk_color_t pixels[TILE_WIDTH * CHAR_HEIGHT];
    eadk_color_t palette[16];
    palette[0] = paper;
    palette[15] = ink;
    for (unsigned i = 1; i < 15; i++) palette[i] = blend_colour(ink, paper, 17 * i);
    while (remaining && *text) {
        int count = 0;
        while (count < GLYPHS_PER_TILE && count < remaining && text[count]) count++;
        int width = count * CHAR_WIDTH;
        for (int i = 0; i < count; i++) {
            const uint8_t *glyph = font_glyph((unsigned char)text[i]);
            for (int row = 0; row < CHAR_HEIGHT; row++) {
                eadk_color_t *cell = pixels + row * width + i * CHAR_WIDTH;
                unsigned previous = 0;
                for (int col = 0; col < CHAR_WIDTH; col++) {
                    int index = row * CHAR_WIDTH + col;
                    unsigned coverage = (glyph[index / 2] >> (index % 2 ? 0 : 4)) & 15;
                    unsigned weight = bold ? (2 * previous) / 3 : 0;
                    cell[col] = palette[coverage > weight ? coverage : weight];
                    previous = coverage;
                }
            }
        }
        eadk_display_push_rect((eadk_rect_t){x, y, width, CHAR_HEIGHT}, pixels);
        x += width;
        text += count;
        remaining -= count;
    }
}

static void string_at(int x, int y, const char *text, eadk_color_t ink, eadk_color_t paper) {
    draw_text(x, y, text, ink, paper, false);
}

static void clipped(int x, int y, const char *text, int columns, eadk_color_t ink, eadk_color_t paper) {
    char visible[TEXT_WIDTH + 1];
    int n = (int)strcspn(text, "\n");
    if (n > columns) {
        memcpy(visible, text, (size_t)(columns - 3));
        memcpy(visible + columns - 3, "...", 4);
    } else {
        memcpy(visible, text, (size_t)n);
        visible[n] = 0;
    }
    string_at(x, y, visible, ink, paper);
}

static const char *higher_suffix(const char *title) {
    const char *suffix = strstr(title, " (higher only)");
    return suffix && !strcmp(suffix + 1, higher_label) ? suffix : NULL;
}

static void title_with_tier(int x, int y, const char *title, int columns,
                            eadk_color_t ink, eadk_color_t paper) {
    const char *suffix = higher_suffix(title);
    if (suffix) {
        char base[TEXT_WIDTH + 1];
        int length = (int)(suffix - title);
        snprintf(base, sizeof(base), "%.*s", length, title);
        int available = columns - (int)strlen(higher_label) - 1;
        clipped(x, y, base, available, ink, paper);
        int label_x = x + ((length < available ? length : available) + 1) * CHAR_WIDTH;
        string_at(label_x, y, higher_label, ink, paper);
    } else clipped(x, y, title, columns, ink, paper);
}

static void header(const char *title, const char *tag, bool filled) {
    eadk_color_t paper = filled ? Gold : White;
    rectangle(0, 0, 320, 21, paper);
    int margin = 8;
    if (tag) {
        int width = (int)strlen(tag) * CHAR_WIDTH;
        string_at(312 - width, 3, tag, Ink, paper);
        margin += width + 7;
    }
    int columns = (320 - 2*margin)/CHAR_WIDTH;
    char centered[TEXT_WIDTH + 1];
    const char *suffix = higher_suffix(title);
    int len = (int)strlen(title);
    if (len <= columns) memcpy(centered,title,(size_t)len + 1);
    else if (suffix) {
        int tail = (int)strlen(suffix);
        if (tail+3 >= columns) tail = 0;
        int head = columns-tail-3;
        memcpy(centered,title,(size_t)head);
        memcpy(centered+head,"...",3);
        if (tail) memcpy(centered+head+3,suffix,(size_t)tail);
        centered[columns] = 0;
        len = columns;
    } else {
        memcpy(centered,title,(size_t)(columns-3));
        memcpy(centered+columns-3,"...",4);
        len = columns;
    }
    for (int i = 0; i < len; i++) centered[i] = (char)toupper((unsigned char)centered[i]);
    string_at((320-len*CHAR_WIDTH)/2,3,centered,filled ? White : Blue,paper);
    rectangle(0, 21, 320, 2, Gold);
}

static void home_header(void) {
    rectangle(0, 0, 320, 21, Gold);
    string_at(6, 3, degrees ? "deg" : "rad", White, Gold);
    const char *modifier = shift ? (alpha ? "ABC" : "SHIFT") :
                           alpha == AlphaLocked ? "abc*" : alpha ? "abc" : "";
    string_at(38, 3, modifier, White, Gold);
    string_at((320 - 5*CHAR_WIDTH)/2, 3, "NUMAI", White, Gold);
}

static void footer(const char *label) {
    if (pin_notice) label = pin_notice;
    eadk_color_t paper = Light;
    rectangle(0, FOOTER, 320, 14, paper);
    const char *modifier = page == Prompt ? "" : shift ? (alpha ? "ABC " : "SHIFT ") :
                           alpha == AlphaLocked ? "abc* " : alpha ? "abc " : "";
    const int prefix = (int)strlen(modifier);
    string_at(5, FOOTER, modifier, Grey, paper);
    clipped(5 + prefix * CHAR_WIDTH, FOOTER, label, TEXT_WIDTH - prefix, Grey, paper);
}

static void scrollbar(int first, int total, int visible, int y, int height) {
    if (total <= visible) return;
    int bar = height * visible / total;
    if (bar < 7) bar = 7;
    rectangle(317, y, 2, height, Light);
    rectangle(317, y + (height - bar) * first / (total - visible), 2, bar, Line);
}

static void wrap(const char *text, int width, bool highlight, int *count) {
    while (*text && *count < WRAP_LIMIT) {
        const char *start = text;
        int length = 0, space = -1;
        while (*text && *text != '\n' && length < width) {
            if (*text == ' ') space = length;
            text++;
            length++;
        }
        if (length == width && *text && *text != '\n' && *text != ' ' && space > width / 2) {
            length = space;
            text = start + length;
        }
        wrapped[(*count)++] = (WrappedLine){length, highlight, start};
        if (*text == '\n') text++;
        else while (*text == ' ') text++;
    }
}

static void reader(int top, int bottom) {
    int visible = (bottom - top) / 16;
    int max = read_length > visible ? read_length - visible : 0;
    if (read_offset > max) read_offset = max;
    if (read_offset < 0) read_offset = 0;
    for (int i = 0; i < visible && i + read_offset < read_length; i++) {
        WrappedLine l = wrapped[i + read_offset];
        int y = top + 16 * i;
        char text[TEXT_WIDTH + 1];
        memcpy(text, l.source, (size_t)l.length);
        text[l.length] = 0;
        if (l.highlight) {
            rectangle(5, y, 308, 16, PaleGold);
            rectangle(5, y, 3, 16, Gold);
        }
        eadk_color_t paper = l.highlight ? PaleGold : White;
        if (page == PhysicsDetail && physics_reference_heading(text)) {
            draw_text(11, y + 1, text, Ink, paper, true);
            rectangle(11, y + 15, l.length * CHAR_WIDTH, 1, Ink);
        } else string_at(11, y + 1, text, Ink, paper);
    }
    scrollbar(read_offset, read_length, visible, top, bottom - top);
}

static int selection_start(const Field *field) {
    return field->cursor < field->anchor ? field->cursor : field->anchor;
}

static int selection_end(const Field *field) {
    return field->cursor > field->anchor ? field->cursor : field->anchor;
}

static void entry(int x, int y, int width, const Field *field, const char *placeholder, bool focused) {
    int columns = (width - 8) / CHAR_WIDTH;
    int start = field->cursor >= columns ? field->cursor - columns + 1 : 0;
    eadk_color_t edge = focused ? Gold : Line;
    rectangle(x, y, width, 18, edge);
    rectangle(x + 1, y + 1, width - 2, 16, White);
    char content[TEXT_WIDTH + 1];
    if (!*field->text && placeholder) {
        clipped(x + 4, y + 2, placeholder, columns, Grey, White);
    } else {
        int count = (int)strlen(field->text + start);
        if (count > columns) count = columns;
        int lo = focused ? selection_start(field) - start : 0;
        int hi = focused ? selection_end(field) - start : 0;
        if (lo < 0) lo = 0;
        if (hi > count) hi = count;
        if (lo > count) lo = count;
        if (hi < lo) hi = lo;
        const int boundaries[] = {0, lo, hi, count};
        for (int i = 0; i < 3; i++) {
            int offset = boundaries[i], n = boundaries[i + 1] - offset;
            if (!n) continue;
            memcpy(content, field->text + start + offset, (size_t)n);
            content[n] = 0;
            string_at(x + 4 + offset * CHAR_WIDTH, y + 2, content, Ink, i == 1 ? PaleGold : White);
        }
    }
    if (focused && field->anchor == field->cursor)
        rectangle(x + 4 + (field->cursor - start) * CHAR_WIDTH, y + 3, 1, 12, Blue);
}

static const Course *course(void) { return &courses[course_selection.row]; }
static const Chapter *chapter(void) { return &course()->chapters[chapter_selection.row]; }
static const Topic *topic(void) { return &chapter()->topics[topic_selection.row]; }

static const PhysicsEquation *equation(void) {
    return &(chemistry_sheet ? chemistry_equations : physics_equations)[physics_selection.row];
}

static int science_subject(int row) {
    const PhysicsEquation *item = equation();
    for (int i = 0; i < item->count; i++)
        if (item->variables[i].formula && row-- == 0) return i;
    return 0;
}

static const UnitGroup *selected_unit_group(void) {
    return &unit_groups[unit_group.row];
}

static int physics_slot(int field) {
    return field >= science_subject(physics_subject.row) ? field + 1 : field;
}

static const char *favourite_title(const Favourite *item) {
    if (item->equation)
        return (item->chemistry ? chemistry_equations : physics_equations)[item->row].title;
    return courses[item->course].chapters[item->chapter].topics[item->row].title;
}

static const char *selection_text(Page menu, int i) {
    switch (menu) {
        case Toolbox: return i == course_count ? "Favourites" : courses[i].title;
        case Chapters: return course()->chapters[i].title;
        case Topics: return chapter()->topics[i].title;
        case TopicMenu: {
            static const char *const actions[] = {"Course notes", "Method and working", "Enter your values", "Worked examples"};
            return actions[i == 2 && !topic()->labels[0] ? 3 : i];
        }
        case Examples:
            return i == 0 ? (topic()->example ? "Original example" : "Calculator example") :
                            topic()->detail->examples[i - 1].title;
        case CasMenu:
            if (i == 0) return physics_sheet_title;
            if (i == 1) return chemistry_sheet_title;
            if (i == 2) return "Unit conversions";
            if (i < CAS_TOOL_COUNT + OperationCount) return operation_names[i - CAS_TOOL_COUNT];
            if (i == CAS_TOOL_COUNT + OperationCount) return "History";
            return extra_tool_names[i - CAS_TOOL_COUNT - OperationCount - 1];
        case Favourites: return favourite_title(&favourites[i]);
        case DecimalPlaces: {
            static char number[24];
            snprintf(number,sizeof(number),"%d%s",i,i == 5 ? " (default)" : "");
            return number;
        }
        case UnitGroups: return unit_groups[i].name;
        case UnitFrom: case UnitTo: return selected_unit_group()->units[i].symbol;
        case PhysicsSubjects: {
            static char label[80];
            const PhysicsVariable *variable = &equation()->variables[science_subject(i)];
            snprintf(label, sizeof(label), "%s: %s [%s]", variable->symbol, variable->name, variable->unit);
            return label;
        }
        case History: return history[i].question;
        case Settings: {
            if (i == 0) return degrees ? "Angle unit                    Degrees" : "Angle unit                    Radians";
            if (i == 1) return decimal ? "Numeric answers               Decimal" : "Numeric answers              Fractions";
            if (i == 2) return show_steps ? "Worked steps                      On" : "Worked steps                     Off";
            if (i == 3) return "Keyboard shortcuts";
            if (i == 4) return "CAS help";
            static char label[43];
            snprintf(label,sizeof(label),"Decimal places                     %d",decimal_places);
            return label;
        }
        default: return "";
    }
}

static int count_for(Page menu) {
    switch (menu) {
        case Toolbox: return course_count + 1;
        case Chapters: return course()->count;
        case Topics: return chapter()->count;
        case TopicMenu: return topic()->labels[0] ? 4 : 3;
        case Examples: return 1 + EXTRA_EXAMPLES;
        case CasMenu: return CAS_TOOL_COUNT + OperationCount + 1 + ExtraToolCount;
        case Favourites: return favourite_count;
        case DecimalPlaces: return 11;
        case PhysicsList: return chemistry_sheet ? chemistry_equation_count : physics_equation_count;
        case PhysicsSubjects: {
            const PhysicsEquation *item = equation();
            int count = 0;
            for (int i = 0; i < item->count; i++) count += item->variables[i].formula != NULL;
            return count;
        }
        case UnitGroups: return unit_group_count;
        case UnitFrom: case UnitTo: return selected_unit_group()->count;
        case History: return history_count;
        case Settings: return 6;
        default: return 0;
    }
}

static Selection *selection_for(Page menu) {
    switch (menu) {
        case Toolbox: return &course_selection;
        case Chapters: return &chapter_selection;
        case Topics: return &topic_selection;
        case TopicMenu: return &topic_action;
        case Examples: return &example_selection;
        case CasMenu: return &cas_selection;
        case PhysicsList: return &physics_selection;
        case PhysicsSubjects: return &physics_subject;
        case UnitGroups: return &unit_group;
        case UnitFrom: return &unit_from;
        case UnitTo: return &unit_to;
        case Favourites: return &favourite_selection;
        case DecimalPlaces: return &decimal_selection;
        case History: return &history_selection;
        case Settings: return &settings_selection;
        default: return NULL;
    }
}

static void draw_list(const char *title, const char *tag, bool filled) {
    const int rows = 9, y0 = 25, row_height = 21;
    Selection *s = selection_for(page);
    int count = count_for(page);
    header(title, tag, filled);
    if (s->row < s->first) s->first = s->row;
    if (s->row >= s->first + rows) s->first = s->row - rows + 1;
    for (int row = 0; row < rows && s->first + row < count; row++) {
        int i = s->first + row, y = y0 + row * row_height;
        bool active = s->row == i;
        eadk_color_t paper = active ? PaleGold : White;
        if (active) {
            rectangle(4, y, 311, 20, paper);
            string_at(8, y + 3, ">", Blue, paper);
        } else if (page == Toolbox) {
            static const eadk_color_t stripes[] = {Blue, 0x5ca9, 0x9275, 0xc369, 0x46b1, 0x6b96};
            rectangle(4, y + 1, 3, 18, stripes[i % (sizeof(stripes) / sizeof(*stripes))]);
        }
        title_with_tier(21, y + 3, selection_text(page, i), 41, active ? Blue : Ink, paper);
    }
    if (page == Toolbox) {
        string_at(8, 181, "Course notes, methods and examples", Grey, White);
        string_at(8, 197, "Shift+VAR pins topics and equations.", Grey, White);
    }
    if (!count) string_at(8, 36, page == Favourites ?
                          "No favourites yet. Pin with Shift+VAR." : "No saved calculations yet.", Grey, White);
    scrollbar(s->first, count, rows, y0, row_height * rows);
    footer(page == Settings ? "OK: change   BACK: return" :
           page == Favourites ? "OK: open   SHIFT+VAR: unpin   BACK" :
           page == Topics || page == TopicMenu ? "OK: open   SHIFT+VAR: pin   BACK" :
           "OK: choose   UP/DOWN: move   BACK");
}

static const struct { const char *key; const char *function; } shortcuts[] = {
    {"TOOLBOX", "Browse courses and topics"},
    {"VAR", "Open CAS, equations, history"},
    {"SHIFT+EXE", "Open or close Settings"},
    {"OK / EXE", "Open item or calculate"},
    {"UP / DOWN", "Move, scroll or change field"},
    {"BACK / HOME", "Go back / return to launcher"},
    {"x,n,t", "Enter x"},
    {"SHIFT+pi", "Enter ="},
    {"SHIFT+x^2", "Enter >"},
    {"SHIFT+sqrt", "Enter <"},
    {"ALPHA", "Enter a letter"},
    {"SHIFT, ALPHA", "Enter capital letters"},
    {"ALPHA twice", "Lock letter entry"},
    {"SHIFT+arrows", "Select text"},
    {"SHIFT+x,n,t", "Cut selected text"},
    {"SHIFT+VAR", "Copy, or pin a topic"},
    {"SHIFT+TOOLBOX", "Paste copied text"},
    {"SHIFT+DEL", "Clear text to the right"},
    {"SHIFT+power", "Enter -> to store a value"},
    {"SHIFT+trig", "Enter inverse trig"},
    {"ANS", "Insert the previous value"},
    {">*", "Factorise"},
    {">/", "Simplify"},
    {"=>", "Solve for x"},
    {">**", "Complete the square"}
};

enum { SHORTCUT_ROWS = 8, SHORTCUT_COUNT = sizeof(shortcuts) / sizeof(*shortcuts) };

static void draw_shortcuts(void) {
    const int table_x = 6, function_x = 109, top = 25, head_height = 20, row_height = 22;
    header("Keyboard shortcuts", NULL, false);
    rectangle(table_x, top, 308, head_height, Light);
    draw_text(10, top + 3, "Key bind", Blue, Light, true);
    draw_text(function_x + 4, top + 3, "Function", Blue, Light, true);
    for (int row = 0; row < SHORTCUT_ROWS; row++) {
        int y = top + head_height + row * row_height;
        int i = shortcuts_offset + row;
        const eadk_color_t paper = row % 2 ? Light : White;
        rectangle(table_x, y, 308, row_height, paper);
        draw_text(10, y + 4, shortcuts[i].key, Ink, paper, true);
        string_at(function_x + 4, y + 4, shortcuts[i].function, Ink, paper);
        rectangle(table_x, y + row_height - 1, 308, 1, Line);
    }
    rectangle(function_x, top, 1, head_height + SHORTCUT_ROWS * row_height, Line);
    scrollbar(shortcuts_offset, SHORTCUT_COUNT, SHORTCUT_ROWS,
              top + head_height, SHORTCUT_ROWS * row_height);
    footer("UP/DOWN: scroll    BACK: settings");
}

static void equation_title(const PhysicsEquation *item, char *title, size_t size) {
    snprintf(title, size, "%s%s", item->title, item->higher ? " (higher only)" : "");
}

static void display_wrapped_line(const WrappedLine *line, int x, int y, eadk_color_t color, eadk_color_t paper) {
    char body[TEXT_WIDTH + 1];
    snprintf(body, sizeof(body), "%.*s", line->length, line->source);
    string_at(x, y, body, color, paper);
}

static void draw_physics_list(void) {
    const int rows = 2, height = 98, top = 25;
    Selection *sel = &physics_selection;
    int count = count_for(PhysicsList);
    header(chemistry_sheet ? chemistry_sheet_title : physics_sheet_title, chemistry_sheet ? "8462" : "8463", false);
    if (sel->row < sel->first) sel->first = sel->row;
    if (sel->row >= sel->first + rows) sel->first = sel->row - rows + 1;
    for (int offset = 0; offset < rows && sel->first + offset < count; offset++) {
        int row = sel->first + offset, y = top + offset * height;
        bool active = row == sel->row;
        const PhysicsEquation *item = &(chemistry_sheet ? chemistry_equations : physics_equations)[row];
        eadk_color_t paper = active ? PaleGold : White;
        if (active) {
            rectangle(4, y, 311, height - 1, paper);
            string_at(8, y + 2, ">", Blue, paper);
        }
        char title[80];
        equation_title(item, title, sizeof(title));
        title_with_tier(21, y + 2, title, 41, active ? Blue : Ink, paper);
        char words[SCIENCE_WORD_SIZE];
        physics_word_equation(item, words);
        int line_count = 0;
        wrap(words, 41, false, &line_count);
        for (int i = 0; i < line_count && i < 5; i++) {
            if (i == 4 && line_count > 5) string_at(21, y + 18 + i*14, "...  OK for full equation", Grey, paper);
            else display_wrapped_line(&wrapped[i], 21, y + 18 + i*14, Grey, paper);
        }
    }
    scrollbar(sel->first, count, rows, top, rows * height);
    footer("OK: equation   UP/DOWN: browse   BACK");
}

static void draw_physics_detail(void) {
    const PhysicsEquation *item = equation();
    char title[80], topic_label[TEXT_WIDTH + 1];
    equation_title(item, title, sizeof(title));
    header(title, NULL, false);
    snprintf(topic_label, sizeof(topic_label), "Topic: %s", item->topic);
    string_at(8, 25, topic_label, Ink, White);
    snprintf(topic_label, sizeof(topic_label), "TOOLBOX: open topic %s", item->section);
    string_at(8, 40, topic_label, Grey, White);
    read_length = 0;
    read_offset = physics_offset;
    wrap(physics_notes, TEXT_WIDTH, false, &read_length);
    reader(56, 200);
    physics_offset = read_offset;
    rectangle(6, 201, 308, 22, PaleGold);
    string_at(12, 205, "Solver                          OK / EXE", Blue, PaleGold);
    footer("UP/DOWN: read   SHIFT+VAR: pin   BACK");
}

static void draw_physics_form(void) {
    const PhysicsEquation *item = equation();
    int subject_index = science_subject(physics_subject.row);
    const PhysicsVariable *subject = &item->variables[subject_index];
    header(chemistry_sheet ? "Chemistry solver" : "Physics solver", subject->symbol, false);
    char caption[112];
    snprintf(caption, sizeof(caption), "Find %s [%s]", subject->name, subject->unit);
    clipped(8, 25, caption, TEXT_WIDTH, Blue, White);
    physics_subject_equation(item, subject_index, caption, sizeof(caption));
    int formula_lines = 0;
    wrap(caption, TEXT_WIDTH, false, &formula_lines);
    for (int i = 0; i < formula_lines; i++) display_wrapped_line(&wrapped[i], 8, 39 + 14*i, Ink, White);
    const int first_field_y = 40 + 14*formula_lines, field_spacing = 35, visible = 3;
    int fields = item->count - 1;
    if (physics_focus < physics_first) physics_first = physics_focus;
    if (physics_focus >= physics_first + visible) physics_first = physics_focus - visible + 1;
    for (int i = physics_first; i < fields && i < physics_first + visible; i++) {
        const PhysicsVariable *v = &item->variables[physics_slot(i)];
        int y = first_field_y + (i - physics_first)*field_spacing;
        snprintf(caption, sizeof(caption), "%s: %s [%s]", v->symbol, v->name, v->unit);
        clipped(8, y, caption, TEXT_WIDTH, i == physics_focus ? Blue : Grey, White);
        const char *hint = v->flags & ScienceOptional ? "Optional (0 if blank)" :
                           i == physics_focus ? "Value or value + unit" : NULL;
        entry(7, y + 14, 306, &physics_fields[physics_slot(i)], hint, i == physics_focus);
    }
    scrollbar(physics_first, fields, visible, first_field_y, visible*field_spacing);
    if (*science_work.answer) {
        int shown = fields < visible ? fields : visible;
        int y = first_field_y + shown*field_spacing, count = 0;
        if (*science_work.substituted) {
            char substituted[SCIENCE_SUBSTITUTION_SIZE + 13];
            snprintf(substituted, sizeof(substituted), "Substitute: %s", science_work.substituted);
            wrap(substituted, TEXT_WIDTH, false, &count);
            for (int i = 0; i < count && i < 3; i++, y += 14)
                display_wrapped_line(&wrapped[i], 8, y, Grey, White);
        }
        count = 0;
        wrap(science_work.answer, TEXT_WIDTH, false, &count);
        eadk_color_t paper = physics_answer_ok ? PaleGold : Light;
        rectangle(6, y, 308, 14*count + 4, paper);
        for (int i = 0; i < count; i++)
            display_wrapped_line(&wrapped[i], 10, y + 2 + i*14, Ink, paper);
    }
    footer("UP/DOWN: field   EXE: solve   BACK");
}

static void draw_science_result(void) {
    header(chemistry_sheet ? "Chemistry working" : "Physics working", NULL, false);
    char formula[112], substitution[SCIENCE_SUBSTITUTION_SIZE + 13];
    physics_subject_equation(equation(), science_subject(physics_subject.row), formula, sizeof(formula));
    snprintf(substitution, sizeof(substitution), "Substitute: %s", science_work.substituted);
    read_length = 0;
    read_offset = science_result_offset;
    wrap(formula, TEXT_WIDTH, false, &read_length);
    if (*science_work.conversions) {
        wrap("\nUnit conversions", TEXT_WIDTH, false, &read_length);
        wrap(science_work.conversions, TEXT_WIDTH, false, &read_length);
    }
    if (*science_work.steps) {
        wrap("\nWorking", TEXT_WIDTH, false, &read_length);
        wrap(science_work.steps, TEXT_WIDTH, false, &read_length);
    }
    if (*science_work.substituted) wrap(substitution, TEXT_WIDTH, false, &read_length);
    wrap(science_work.answer, TEXT_WIDTH, physics_answer_ok, &read_length);
    reader(27, 222);
    science_result_offset = read_offset;
    footer("UP/DOWN: scroll   BACK: inputs");
}

static void draw_unit_conversion(void) {
    const Unit *units = selected_unit_group()->units;
    char title[60];
    snprintf(title, sizeof(title), "%s to %s", units[unit_from.row].symbol, units[unit_to.row].symbol);
    header("Unit conversions", NULL, true);
    clipped(8, 30, selected_unit_group()->name, TEXT_WIDTH, Ink, White);
    clipped(8, 48, title, TEXT_WIDTH, Blue, White);
    if (page == UnitValue) {
        entry(7, 74, 306, &unit_input, "Enter a number or expression", true);
        if (unit_input_error) string_at(8, 102, "Check the value or its size.", Ink, White);
        footer("EXE: convert   BACK: change unit");
        return;
    }
    read_offset = 0;
    read_length = 0;
    wrap(unit_expression, TEXT_WIDTH, false, &read_length);
    wrap(unit_work, TEXT_WIDTH, false, &read_length);
    char answer[60];
    snprintf(answer, sizeof(answer), "%s %s", unit_number, units[unit_to.row].symbol);
    wrap(answer, TEXT_WIDTH, true, &read_length);
    reader(72, 190);
    if (unit_copied) string_at(8, 196, "Number copied to NumAI clipboard", Blue, White);
    footer("SHIFT+VAR: copy number   BACK");
}

static void draw_tool(void) {
    header(extra_tool_names[extra_tool],NULL,false);
    if (page == ToolOutput) {
        read_length = 0;
        read_offset = tool_offset;
        if (show_steps) wrap(tool_solution.steps,TEXT_WIDTH,false,&read_length);
        wrap(tool_solution.answer,TEXT_WIDTH,tool_solution.ok,&read_length);
        reader(27,222);
        tool_offset = read_offset;
        footer("UP/DOWN: read   BACK: inputs");
        return;
    }
    const char *hint = extra_tool == TriangleSolver ? "Leave unknown sides or angles blank." :
                       extra_tool == TrigSolver ? "Use sin, cos or tan. Set both bounds." :
                       "Use [name] for multi-letter symbols.";
    string_at(8,26,hint,Grey,White);
    const int visible = 4, top = 48, spacing = 39;
    if (active_field < tool_first) tool_first = active_field;
    if (active_field >= tool_first+visible) tool_first = active_field-visible+1;
    for (int row = 0; row < visible && tool_first+row < field_count; row++) {
        int i = tool_first+row, y = top+row*spacing;
        char angle_label[20];
        const char *label = form_labels[i];
        if (extra_tool == TriangleSolver && i >= 3) {
            snprintf(angle_label,sizeof(angle_label),"Angle %c [%s]",'A'+i-3,degrees?"deg":"rad");
            label = angle_label;
        }
        string_at(8,y,label,i == active_field ? Blue : Grey,White);
        entry(7,y+15,306,&forms[i],NULL,i == active_field);
    }
    scrollbar(tool_first,field_count,visible,top,visible*spacing);
    footer("UP/DOWN: field   EXE: solve   BACK");
}

#include "calculator_view.inc"

static void draw(void) {
    eadk_display_push_rect_uniform(eadk_screen_rect, White);
    switch (page) {
        case Prompt:
        case CalcHistory: case CalcExtras:
            draw_calculator();
            if (page == CalcExtras) draw_calc_extras();
            break;
        case Toolbox: draw_list("Toolbox", NULL, true); break;
        case Chapters: draw_list(course()->title, course()->code, false); break;
        case Topics: draw_list(chapter()->title, NULL, false); break;
        case TopicMenu: draw_list(topic()->title, NULL, false); break;
        case Examples: draw_list("Worked examples", NULL, false); break;
        case PhysicsList: draw_physics_list(); break;
        case PhysicsDetail: draw_physics_detail(); break;
        case PhysicsSubjects: draw_list("Choose the unknown", NULL, false); break;
        case PhysicsForm: draw_physics_form(); break;
        case ScienceResult: draw_science_result(); break;
        case UnitGroups: draw_list("Unit conversions", NULL, true); break;
        case UnitFrom: draw_list("Convert from", NULL, false); break;
        case UnitTo: draw_list("Convert to", NULL, false); break;
        case UnitValue: case UnitResult: draw_unit_conversion(); break;
        case CasMenu: draw_list("CAS and history", NULL, true); break;
        case History: draw_list("History", NULL, false); break;
        case Settings: draw_list("Settings", NUMAI_VERSION, false); break;
        case Favourites: draw_list("Favourites",NULL,false); break;
        case DecimalPlaces: draw_list("Decimal places",NULL,false); break;
        case ToolForm: case ToolOutput: draw_tool(); break;
        case Shortcuts: draw_shortcuts(); break;
        case Form:
            header(form_topic ? form_topic->title : operation_names[form_operation], NULL, false);
            for (int i = 0; i < field_count; i++) {
                int y = 27 + i * 38;
                string_at(8, y, form_labels[i], i == active_field ? Blue : Grey, White);
                entry(7, y + 15, 306, &forms[i], NULL, i == active_field);
            }
            string_at(8, 32 + field_count * 38, "Enter your values, then press EXE.", Grey, White);
            footer("UP/DOWN: field    EXE: work it out");
            break;
        case Reading:
            header(read_title, NULL, false);
            read_length = 0;
            wrap(read_content, TEXT_WIDTH, false, &read_length);
            reader(30, 223);
            footer("UP/DOWN: scroll    BACK: return");
            break;
        case Result:
            header(operation_names[result_operation], NULL, false);
            read_length = 0;
            wrap(result_question, TEXT_WIDTH, false, &read_length);
            if (show_steps && solution.steps[0]) wrap(solution.steps, TEXT_WIDTH, false, &read_length);
            wrap(solution.answer, TEXT_WIDTH, true, &read_length);
            reader(27, 197);
            entry(4, 202, 312, &prompt, "Type a question, or press TOOLBOX", page == Result);
            footer("UP/DOWN: read   TOOLBOX   VAR   EXE");
            break;
    }
}

static void set_field(Field *field, const char *value, bool selected) {
    snprintf(field->text, sizeof(field->text), "%s", value ? value : "");
    field->cursor = (int)strlen(field->text);
    field->anchor = selected ? 0 : field->cursor;
}

static void replace_range(Field *field, int from, int to, const char *text) {
    size_t length = strlen(text), tail = strlen(field->text + to);
    if ((size_t)from + length + tail >= sizeof(field->text)) return;
    memmove(field->text + from + length, field->text + to, tail + 1);
    memcpy(field->text + from, text, length);
    field->cursor = field->anchor = from + (int)length;
}

static void insert(Field *field, const char *text) {
    replace_range(field, selection_start(field), selection_end(field), text);
}

static const char *skip_spaces(const char *text) {
    while (isspace((unsigned char)*text)) text++;
    return text;
}

static Operation prompt_operation(const char *text, char operand[EXPRESSION_SIZE]) {
    static const struct { const char *prefix; Operation operation; } commands[] = {
        {">**", CompleteSquare}, {">*", Factorise}, {">/", Simplify}, {"=>", Solve}
    };
    text = skip_spaces(text);
    for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
        size_t prefix = strlen(commands[i].prefix);
        if (strncmp(text, commands[i].prefix, prefix)) continue;
        const char *first = skip_spaces(text + prefix), *end = first + strlen(first);
        while (end > first && isspace((unsigned char)end[-1])) end--;
        if (end - first >= 2 && *first == '{' && end[-1] == '}') {
            first++;
            end--;
        }
        snprintf(operand, EXPRESSION_SIZE, "%.*s", (int)(end - first), first);
        return commands[i].operation;
    }
    snprintf(operand, EXPRESSION_SIZE, "%s", text);
    return strchr(text, '=') && !strpbrk(text, "<>") ? Solve : Calculate;
}

static void form_for(Operation operation, const Topic *item, Page parent) {
    static const char *const defaults[] = {
        "2/3+1/6", "x^2-5x+6", "3x+2(x-1)", "(x+1)(x-3)", "x^2-5x+6=0",
        "x^3-4x+2", "3x^2-4", "x^2+1", "x^2+1", "cos(x)=x", "2x^2+12x+11"
    };
    form_parent = parent;
    form_operation = operation;
    form_topic = item;
    active_field = 0;
    field_count = 0;
    if (item) {
        while (field_count < MAX_FIELDS && item->labels[field_count]) {
            form_labels[field_count] = item->labels[field_count];
            set_field(&forms[field_count], item->values[field_count], true);
            field_count++;
        }
    } else {
        form_labels[0] = (operation == Solve || operation == FindRoot) ? "Equation in x" : "Expression in x";
        if (operation == Calculate) form_labels[0] = "Expression";
        if (operation == CompleteSquare) form_labels[0] = "Quadratic in x";
        if (operation == Integrate || operation == Definite) form_labels[0] = "Integrand f(x)";
        char operand[EXPRESSION_SIZE];
        prompt_operation(prompt.text, operand);
        set_field(&forms[0], *operand ? operand : defaults[operation], !*operand);
        field_count = (operation == Definite || operation == FindRoot) ? 3 : operation == Substitute ? 2 : 1;
        if (operation == Definite) {
            form_labels[1] = "Lower limit";
            form_labels[2] = "Upper limit";
            set_field(&forms[1], "0", true);
            set_field(&forms[2], "2", true);
        }
        if (operation == FindRoot) {
            form_labels[1] = "Lower bound";
            form_labels[2] = "Upper bound";
            set_field(&forms[1], "0", true);
            set_field(&forms[2], "1", true);
        }
        if (operation == Substitute) {
            form_labels[1] = "Value of x";
            set_field(&forms[1], "2", true);
        }
    }
    page = Form;
}

static void record(HistoryEntry *entries, int *count, int limit, const char *question,
                   Operation operation, const Solution *answer) {
    if (*count < limit) (*count)++;
    for (int i = *count - 1; i > 0; i--) entries[i] = entries[i - 1];
    snprintf(entries[0].question, sizeof(entries[0].question), "%s", question);
    entries[0].operation = operation;
    entries[0].solution = *answer;
}

static void update_ans(char *dest, const Solution *answer) {
    if (!answer->ok) return;
    const char *str = answer->answer;
    if (strncmp(str, "approx. ", 8) == 0) str += 8;
    if (strspn(str, "-+0123456789.e/") == strlen(str) && strlen(str) < EXPRESSION_SIZE)
        strcpy(dest, str);
}

static bool build_formula(char output[EXPRESSION_SIZE * 4], const char *formula) {
    size_t written = 0, limit = EXPRESSION_SIZE * 4;
    for (; *formula; formula++) {
        if (*formula != '$') {
            if (written + 1 >= limit) return false;
            output[written++] = *formula;
            continue;
        }
        int field = *++formula - '1';
        if (field < 0 || field >= field_count || !forms[field].text[0]) return false;
        size_t size = strlen(forms[field].text);
        if (written + 2 + size >= limit) return false;
        output[written++] = '(';
        memcpy(output + written, forms[field].text, size);
        written += size;
        output[written++] = ')';
    }
    output[written] = 0;
    return true;
}

static void show_result(const char *question, Operation operation, Page parent, bool save) {
    result_operation = operation;
    snprintf(result_question, sizeof(result_question), "%s", question);
    if (save && solution.ok) {
        record(history, &history_count, HISTORY_LIMIT, result_question, operation, &solution);
        if (operation == Calculate || operation == Definite || operation == Substitute)
            update_ans(ans, &solution);
    }
    has_result = true;
    read_offset = 0;
    read_length = 0;
    result_parent = parent;
    page = Result;
}

static void submit_form(bool example) {
    char generated[EXPRESSION_SIZE * 4];
    const char *input = forms[0].text;
    bool formula = form_topic && form_topic->formula;
    if (formula && !build_formula(generated, form_topic->formula)) {
        solution = (Solution){0};
        strcpy(solution.answer, "Enter a value in each field and keep expressions short.");
    } else {
        if (formula) input = generated;
        bool input_degrees = degrees;
        for (int i = 0; i < field_count; i++) {
            if (strstr(form_labels[i], "(degrees)")) input_degrees = true;
            if (strstr(form_labels[i], "(radians)")) input_degrees = false;
        }
        solve_expression(form_operation, input, forms[1].text, forms[2].text,
                         input_degrees, decimal, &solution);
        if (formula && solution.ok)
            snprintf(solution.steps, sizeof(solution.steps), "%s\nSubstitute: %.300s", form_topic->method, input);
    }
    show_result(formula ? form_topic->title : input, form_operation, example ? Examples : Form, !example);
}

static void submit_prompt(void) {
    const char *text = prompt.text;
    if (!*text) return;
    char operand[EXPRESSION_SIZE];
    Operation operation = prompt_operation(text, operand);
    CalculationValue value;
    if (operation == Calculate && cas_calculation(operand,degrees,&value)) {
        if (calc_count<CALCULATION_LIMIT) calc_count++;
        memmove(calculations+1,calculations,(size_t)(calc_count-1)*sizeof(*calculations));
        snprintf(calculations[0].input,sizeof(calculations[0].input),"%s",operand);
        calculations[0].result=value;
        if (*value.exact) snprintf(ans,sizeof(ans),"%s",value.exact);
        else snprintf(ans,sizeof(ans),"%.17g",value.value);
        solve_expression(operation,operand,"","",degrees,decimal,&solution);
        if (solution.ok) record(history,&history_count,HISTORY_LIMIT,operand,operation,&solution);
        calc_first=0;
        set_field(&prompt,"",false);
        page=Prompt;
        return;
    }
    solve_expression(operation, operand, "", "", degrees, decimal, &solution);
    show_result(text, operation, Prompt, true);
    if (solution.ok) set_field(&prompt, "", false);
}

static void begin_read(const char *title, const char *content) {
    reading_parent = page;
    read_title = title;
    read_content = content;
    read_offset = 0;
    read_length = 0;
    page = Reading;
}

static void open_equation(void) {
    physics_offset = 0;
    physics_subject = (Selection){0};
    for (int i = 0; i < equation()->count; i++) set_field(&physics_fields[i],"",false);
    physics_reference(equation(),physics_notes);
    page = PhysicsDetail;
}

static bool pin_current(void) {
    Favourite item = {0};
    if (page == Favourites) {
        if (!favourite_count) return true;
        item = favourites[favourite_selection.row];
    } else if (page == PhysicsDetail || page == PhysicsList) {
        item = (Favourite){.row=physics_selection.row,.equation=true,.chemistry=chemistry_sheet};
    } else if (page == Topics || page == TopicMenu ||
               (page == Reading && (reading_parent == Topics || reading_parent == Favourites ||
                                     reading_parent == TopicMenu || reading_parent == PhysicsDetail))) {
        item = (Favourite){course_selection.row,chapter_selection.row,topic_selection.row,false,false};
    } else return false;
    for (int i = 0; i < favourite_count; i++) {
        Favourite *saved = &favourites[i];
        if (saved->row != item.row || saved->equation != item.equation ||
            (item.equation ? saved->chemistry != item.chemistry :
             saved->course != item.course || saved->chapter != item.chapter)) continue;
        memmove(saved,saved+1,(size_t)(--favourite_count-i)*sizeof(*saved));
        if (favourite_selection.row >= favourite_count)
            favourite_selection.row = favourite_count ? favourite_count-1 : 0;
        pin_notice = "Removed from Favourites";
        return true;
    }
    if (favourite_count < FAVOURITE_LIMIT) {
        favourites[favourite_count++] = item;
        pin_notice = "Pinned in TOOLBOX > Favourites";
    } else pin_notice = "Favourites list is full";
    return true;
}

static void open_favourite(void) {
    Favourite item = favourites[favourite_selection.row];
    if (item.equation) {
        favourite_origin = true;
        chemistry_sheet = item.chemistry;
        physics_selection = (Selection){item.row,0};
        open_equation();
        return;
    }
    course_selection = (Selection){item.course,0};
    chapter_selection = (Selection){item.chapter,0};
    topic_selection = (Selection){item.row,0};
    if (!topic()->method) begin_read(topic()->title,topic()->course);
    else {
        favourite_origin = true;
        topic_action = (Selection){0};
        page = TopicMenu;
    }
}

static void open_tool(ExtraTool selection) {
    extra_tool = selection;
    active_field = tool_first = 0;
    if (selection == TriangleSolver) {
        static const char *const labels[] = {"Side a (opposite A)","Side b (opposite B)","Side c (opposite C)",
                                             "Angle A","Angle B","Angle C"};
        field_count = 6;
        for (int i = 0; i < field_count; i++) {
            form_labels[i] = labels[i];
            set_field(&forms[i],"",false);
        }
    } else {
        field_count = selection == TrigSolver ? 3 : 2;
        form_labels[0] = selection == TrigSolver ? "Equation in x" : "Physics equation";
        set_field(&forms[0],selection == TrigSolver ? "sin(x)=0.5" : "v^2=u^2+2as",true);
        form_labels[1] = selection == TrigSolver ? "Lower bound" : "Symbol to isolate";
        set_field(&forms[1],selection == TrigSolver ? "0" : "a",true);
        if (selection == TrigSolver) {
            form_labels[2] = "Upper bound";
            set_field(&forms[2],degrees ? "360" : "2pi",true);
        }
    }
    page = ToolForm;
}

static void submit_tool(void) {
    if (extra_tool == TrigSolver)
        cas_trig_equation(forms[0].text,forms[1].text,forms[2].text,degrees,&tool_solution);
    else if (extra_tool == PhysicsRearrange)
        rearrange_physics(forms[0].text,forms[1].text,&tool_solution);
    else {
        const char *inputs[6];
        for (int i = 0; i < 6; i++) inputs[i] = forms[i].text;
        solve_triangle(inputs,degrees,&tool_solution);
    }
    tool_offset = 0;
    page = ToolOutput;
}

static void open_physics_topic(void) {
    const char *section = equation()->section;
    size_t length = strlen(section);
    for (int i = 0; i < course_count; i++) {
        const Course *entry = &courses[i];
        if (strcmp(entry->code, chemistry_sheet ? "8462" : "8463")) continue;
        for (int j = 0; j < entry->count; j++) {
            const Chapter *group = &entry->chapters[j];
            const Topic *match = NULL;
            for (int k = 0; k < group->count; k++) {
                const Topic *card = &group->topics[k];
                if (!strncmp(card->course, section, length) && card->course[length] == '\n') {
                    if (!match) match = card;
                    if (chemistry_sheet && equation()->higher && higher_suffix(card->title)) {
                        match = card;
                        break;
                    }
                }
            }
            if (match) {
                course_selection = (Selection){i,0};
                chapter_selection = (Selection){j,0};
                topic_selection = (Selection){(int)(match-group->topics),0};
                begin_read(match->title, match->course);
                return;
            }
        }
        return;
    }
}

static const char cas_help[] =
    "In a form, UP/DOWN moves between values. Typing replaces a selected default. EXE works it out.\n"
    "CAS uses one variable x. Solve handles real polynomial roots. The interval tool bisects a continuous function. "
    "Indefinite integrals cover polynomials and selected elementary functions. Definite integrals accept polynomials.\n"
    "Numeric functions include ncr(n,r), npr(n,r), binom(n,r,p), and norm(z).";

static void back(void) {
    switch (page) {
        case Toolbox: case CasMenu: page = Prompt; break;
        case Chapters: page = Toolbox; break;
        case Topics: page = Chapters; break;
        case TopicMenu:
            page = favourite_origin ? Favourites : Topics;
            favourite_origin = false;
            break;
        case Examples: page = TopicMenu; break;
        case PhysicsList: page = CasMenu; break;
        case PhysicsDetail:
            page = favourite_origin ? Favourites : PhysicsList;
            favourite_origin = false;
            break;
        case PhysicsSubjects: page = PhysicsDetail; break;
        case PhysicsForm: page = PhysicsSubjects; break;
        case ScienceResult:
            science_work.answer[0] = 0;
            page = PhysicsForm;
            break;
        case UnitGroups: page = CasMenu; break;
        case UnitFrom: page = UnitGroups; break;
        case UnitTo: page = UnitFrom; break;
        case UnitValue: page = UnitTo; break;
        case UnitResult: page = UnitValue; break;
        case Form: page = form_parent; break;
        case Result: page = result_parent; break;
        case Settings: page = settings_parent; break;
        case DecimalPlaces: page = Settings; break;
        case Favourites: page = Toolbox; break;
        case ToolForm: page = CasMenu; break;
        case ToolOutput: page = ToolForm; break;
        case CalcHistory: page = Prompt; break;
        case CalcExtras: page = CalcHistory; break;
        case Shortcuts: page = Settings; break;
        case History: page = CasMenu; break;
        case Reading: page = reading_parent; break;
        default: break;
    }
}

static void choose(void) {
    switch (page) {
        case Toolbox:
            if (course_selection.row == course_count) {
                favourite_selection = (Selection){0};
                page = Favourites;
                break;
            }
            chapter_selection = (Selection){0};
            page = Chapters;
            break;
        case Chapters:
            topic_selection = (Selection){0};
            page = Topics;
            break;
        case Topics:
            if (!topic()->method) begin_read(topic()->title, topic()->course);
            else {
                topic_action = (Selection){0};
                page = TopicMenu;
            }
            break;
        case TopicMenu: {
            const Topic *item = topic();
            int row = topic_action.row;
            if (row == 0) begin_read(item->title, item->detail->notes);
            else if (row == 1) begin_read(item->title, item->method);
            else if (row == 2 && item->labels[0]) form_for(item->operation, item, TopicMenu);
            else {
                example_selection = (Selection){0};
                page = Examples;
            }
            break;
        }
        case Examples: {
            const Topic *item = topic();
            if (example_selection.row > 0) {
                const WorkedExample *example = &item->detail->examples[example_selection.row - 1];
                begin_read(example->title, example->working);
            } else if (item->example) begin_read(item->title, item->example);
            else {
                form_for(item->operation, item, Examples);
                submit_form(true);
            }
            break;
        }
        case CasMenu:
            if (cas_selection.row < 2) {
                chemistry_sheet = cas_selection.row == 1;
                favourite_origin = false;
                physics_selection = (Selection){0};
                page = PhysicsList;
            } else if (cas_selection.row == 2) {
                unit_group = (Selection){0};
                set_field(&unit_input, "", false);
                page = UnitGroups;
            } else if (cas_selection.row == CAS_TOOL_COUNT + OperationCount) {
                history_selection = (Selection){0};
                page = History;
            } else if (cas_selection.row < CAS_TOOL_COUNT + OperationCount)
                form_for((Operation)(cas_selection.row - CAS_TOOL_COUNT),NULL,CasMenu);
            else open_tool((ExtraTool)(cas_selection.row-CAS_TOOL_COUNT-OperationCount-1));
            break;
        case Favourites:
            if (favourite_count) open_favourite();
            break;
        case DecimalPlaces:
            decimal_places = decimal_selection.row;
            if (settings_parent == CalcExtras) prepare_calc_extras();
            page = Settings;
            break;
        case UnitGroups:
            unit_from = (Selection){0};
            page = UnitFrom;
            break;
        case UnitFrom:
            unit_to = (Selection){unit_from.row == 0 ? 1 : 0, 0};
            page = UnitTo;
            break;
        case UnitTo:
            unit_input_error = false;
            page = UnitValue;
            break;
        case PhysicsList:
            open_equation();
            break;
        case PhysicsSubjects:
            physics_focus = physics_first = 0;
            science_work.answer[0] = 0;
            page = PhysicsForm;
            break;
        case History:
            if (history_count) {
                const HistoryEntry *item = &history[history_selection.row];
                solution = item->solution;
                show_result(item->question, item->operation, History, false);
            }
            break;
        case Settings:
            if (settings_selection.row == 0) degrees = !degrees;
            if (settings_selection.row == 1) decimal = !decimal;
            if (settings_selection.row == 2) show_steps = !show_steps;
            if (settings_selection.row == 3) {
                shortcuts_offset = 0;
                page = Shortcuts;
            }
            if (settings_selection.row == 4) begin_read("CAS help", cas_help);
            if (settings_selection.row == 5) {
                decimal_selection = (Selection){decimal_places,0};
                page = DecimalPlaces;
            }
            break;
        default: break;
    }
}

static const char *key_text(int key) {
    static const char *const keys[53] = {
        [eadk_key_xnt] = "x", [eadk_key_exp] = "exp(", [eadk_key_ln] = "ln(",
        [eadk_key_log] = "log(", [eadk_key_imaginary] = "i", [eadk_key_comma] = ",", [eadk_key_power] = "^",
        [eadk_key_sine] = "sin(", [eadk_key_cosine] = "cos(", [eadk_key_tangent] = "tan(",
        [eadk_key_pi] = "pi", [eadk_key_sqrt] = "sqrt(", [eadk_key_square] = "^2",
        [eadk_key_seven] = "7", [eadk_key_eight] = "8", [eadk_key_nine] = "9",
        [eadk_key_left_parenthesis] = "(", [eadk_key_right_parenthesis] = ")",
        [eadk_key_four] = "4", [eadk_key_five] = "5", [eadk_key_six] = "6",
        [eadk_key_multiplication] = "*", [eadk_key_division] = "/",
        [eadk_key_one] = "1", [eadk_key_two] = "2", [eadk_key_three] = "3",
        [eadk_key_plus] = "+", [eadk_key_minus] = "-", [eadk_key_zero] = "0",
        [eadk_key_dot] = ".", [eadk_key_ee] = "e"
    };
    return key >= 0 && key < 53 ? keys[key] : NULL;
}

static const char *shift_text(int key) {
    switch (key) {
        case eadk_key_exp: return "[";
        case eadk_key_ln: return "]";
        case eadk_key_log: return "{";
        case eadk_key_imaginary: return "}";
        case eadk_key_comma: return "_";
        case eadk_key_power: return "->";
        case eadk_key_sine: return "asin(";
        case eadk_key_cosine: return "acos(";
        case eadk_key_tangent: return "atan(";
        case eadk_key_pi: return "=";
        case eadk_key_sqrt: return "<";
        case eadk_key_square: return ">";
        default: return NULL;
    }
}

static char alpha_character(int key, bool capitals) {
    static const char letters[] = "abcdefghijklmnopq\0rstuv\0wxyz";
    if (key >= eadk_key_exp && key <= eadk_key_plus) {
        char letter = letters[key - eadk_key_exp];
        return capitals ? (char)toupper((unsigned char)letter) : letter;
    }
    switch (key) {
        case eadk_key_var: return ';';
        case eadk_key_toolbox: return '"';
        case eadk_key_backspace: return alpha == AlphaLocked ? '\0' : '%';
        case eadk_key_minus: return ' ';
        case eadk_key_zero: return '?';
        case eadk_key_dot: return '!';
        default: return '\0';
    }
}

static void edit(Field *field, int key, bool shifted, bool alphabetic, char letter) {
    int from = selection_start(field), to = selection_end(field);
    if (letter) {
        const char text[] = {letter, 0};
        insert(field, text);
        return;
    }
    bool function = shifted && !alphabetic;
    if (key <= eadk_key_right && (function || key == eadk_key_left || key == eadk_key_right)) {
        int end = (int)strlen(field->text);
        if (!function && from != to) {
            field->anchor = field->cursor;
            return;
        }
        if (key == eadk_key_up) field->cursor = 0;
        if (key == eadk_key_down) field->cursor = end;
        if (key == eadk_key_left && field->cursor > 0) field->cursor--;
        if (key == eadk_key_right && field->cursor < end) field->cursor++;
        if (!function) field->anchor = field->cursor;
        return;
    }
    if (key == eadk_key_backspace) {
        if (to > from) replace_range(field, from, to, "");
        else if (function) {
            if (field->text[field->cursor])
                replace_range(field, field->cursor, (int)strlen(field->text), "");
            else set_field(field, "", false);
        } else if (field->cursor > 0) replace_range(field, field->cursor - 1, field->cursor, "");
        return;
    }
    if (function && (key == eadk_key_xnt || key == eadk_key_var)) {
        if (to > from) {
            snprintf(clipboard, sizeof(clipboard), "%.*s", to - from, field->text + from);
            if (key == eadk_key_xnt) replace_range(field, from, to, "");
        } else if (key == eadk_key_var && page == Result && !*field->text &&
                   solution.ok && strlen(solution.answer) < sizeof(clipboard)) strcpy(clipboard, solution.answer);
        return;
    }
    if (function && key == eadk_key_toolbox) {
        if (*clipboard) insert(field, clipboard);
        return;
    }
    const char *text = function ? shift_text(key) : NULL;
    char previous[EXPRESSION_SIZE + 2];
    if (key == eadk_key_ans && *ans) {
        snprintf(previous, sizeof(previous), "(%s)", ans);
        text = previous;
    }
    if (!text) text = key_text(key);
    if (text) insert(field, text);
}

static void submit_conversion(void) {
    double value, result;
    const Unit *units = selected_unit_group()->units;
    const Unit *from = &units[unit_from.row], *to = &units[unit_to.row];
    unit_input_error = !cas_read_number(unit_input.text, &value) || !unit_convert(value, from, to, &result);
    if (unit_input_error) return;
    snprintf(unit_number, sizeof(unit_number), "%.12g", result);
    snprintf(unit_expression, sizeof(unit_expression), "%.12g %s", value, from->symbol);
    unit_working(unit_work, sizeof(unit_work), value, from, to, result);
    unit_copied = false;
    page = UnitResult;
}

static void open_calculation_history(void) {
    calc_first=calc_row=0;
    calc_column=default_calc_column(&calculations[0]);
    page=CalcHistory;
}

static void handle_calculation_history(int key,bool copy) {
    if (key==eadk_key_up || key==eadk_key_down) {
        if (key==eadk_key_down && calc_row==0) { page=Prompt;return; }
        int row=calc_row+(key==eadk_key_up?1:-1);
        if (row<calc_count) {
            calc_row=row;
            calc_column=default_calc_column(&calculations[calc_row]);
        }
        return;
    }
    const CalcEntry *item=&calculations[calc_row];
    if (key==eadk_key_left || key==eadk_key_right) {
        int step=key==eadk_key_left?-1:1;
        for (int next=calc_column+step;next>=CalcInputColumn && next<=CalcDotsColumn;next+=step) {
            if (!calc_column_available(item,next)) continue;
            calc_column=next;
            break;
        }
        return;
    }
    if (key!=eadk_key_ok && key!=eadk_key_exe && !copy) return;
    if (calc_column==CalcDotsColumn) {
        if (!copy) {
            prepare_calc_extras();
            if (extras_count) page=CalcExtras;
        }
        return;
    }
    char rendered[32];
    const char *value=item->input;
    if (calc_column==CalcExactColumn) value=item->result.exact;
    if (calc_column==CalcDecimalColumn) {
        format_calc_decimal(rendered,sizeof(rendered),item->result.value);
        value=rendered;
    }
    if (copy) snprintf(clipboard,sizeof(clipboard),"%s",value);
    else {
        insert(&prompt,value);
        page=Prompt;
    }
}

static bool handle_key(int key, bool shifted, bool alphabetic) {
    pin_notice = NULL;
    if (key == eadk_key_xnt && alphabetic) return true;
    if (key == eadk_key_exe && shifted) {
        if (page == Settings || page == Shortcuts || page == DecimalPlaces ||
            (page == Reading && reading_parent == Settings)) page = settings_parent;
        else {
            settings_parent = page;
            settings_selection = (Selection){0};
            page = Settings;
        }
        return true;
    }
    char letter = alphabetic ? alpha_character(key, shifted) : '\0';
    bool function = shifted && !alphabetic;
    if (function && key == eadk_key_var && pin_current()) return true;
    if (page == CalcHistory && function && key == eadk_key_var) {
        handle_calculation_history(key,true);
        return true;
    }
    if (page == UnitResult && function && key == eadk_key_var) {
        strcpy(clipboard, unit_number);
        unit_copied = true;
        return true;
    }
    Field *field = page == PhysicsForm ? &physics_fields[physics_slot(physics_focus)] :
                   page == UnitValue ? &unit_input :
                   (page == Form || page == ToolForm) ? &forms[active_field] :
                   (page == Prompt || page == Result) ? &prompt : NULL;
    if (field && (letter || (function && (key <= eadk_key_right || key == eadk_key_xnt ||
                                          key == eadk_key_var || key == eadk_key_toolbox)))) {
        edit(field, key, shifted, alphabetic, letter);
        if (page == PhysicsForm) science_work.answer[0] = 0;
        if (page == UnitValue) unit_input_error = false;
        return true;
    }
    if (letter || (function && (key == eadk_key_var || key == eadk_key_toolbox))) return true;

    if (key == eadk_key_back) {
        if (page == Prompt) {
            if (*prompt.text) set_field(&prompt,"",false);
            else if (!calc_count) return false;
            return true;
        }
        back();
        return true;
    }
    if ((key == eadk_key_toolbox || key == eadk_key_var) &&
        (page == Prompt || page == Result || page == Toolbox || page == CasMenu || page == CalcHistory)) {
        Page target = key == eadk_key_toolbox ? Toolbox : CasMenu;
        if (target == CasMenu && page != CasMenu) cas_selection = (Selection){0};
        page = page == target ? Prompt : target;
        return true;
    }
    if (page == CalcHistory) {
        handle_calculation_history(key,false);
        return true;
    }
    if (page == CalcExtras) {
        if (key == eadk_key_up && extras_row>0) extras_row--;
        else if (key == eadk_key_down && extras_row+1<extras_count) extras_row++;
        else if ((key==eadk_key_ok || key==eadk_key_exe) && *extra_results[extras_row].reuse) {
            insert(&prompt,extra_results[extras_row].reuse);
            page=Prompt;
        }
        return true;
    }
    if (page == Shortcuts) {
        if (key == eadk_key_up && shortcuts_offset > 0) shortcuts_offset--;
        else if (key == eadk_key_down && shortcuts_offset < SHORTCUT_COUNT - SHORTCUT_ROWS) shortcuts_offset++;
        return true;
    }
    Selection *selection = selection_for(page);
    if (selection) {
        int count = count_for(page);
        if (key == eadk_key_up && selection->row > 0) selection->row--;
        else if (key == eadk_key_down && selection->row + 1 < count) selection->row++;
        else if (key == eadk_key_ok || key == eadk_key_right || key == eadk_key_exe) choose();
        else if (key == eadk_key_left) back();
        return true;
    }
    if (page == PhysicsDetail) {
        if (key == eadk_key_up || key == eadk_key_down)
            physics_offset += key == eadk_key_up ? -1 : 1;
        else if (key == eadk_key_toolbox) open_physics_topic();
        else if (key == eadk_key_ok || key == eadk_key_exe) page = PhysicsSubjects;
        return true;
    }
    if (page == PhysicsForm) {
        int count = equation()->count - 1;
        if (key == eadk_key_up && physics_focus > 0) physics_focus--;
        else if (key == eadk_key_down && physics_focus + 1 < count) physics_focus++;
        else if (key == eadk_key_exe || key == eadk_key_ok) {
            const char *values[SCIENCE_VARIABLES] = {0};
            for (int i = 0; i < count; i++) values[physics_slot(i)] = physics_fields[physics_slot(i)].text;
            physics_answer_ok = physics_solve(equation(), science_subject(physics_subject.row), values, &science_work);
            if (*science_work.substituted && (chemistry_sheet || *science_work.conversions)) {
                science_result_offset = 0;
                page = ScienceResult;
            }
        } else if (key != eadk_key_up && key != eadk_key_down) {
            edit(&physics_fields[physics_slot(physics_focus)], key, shifted, alphabetic, letter);
            if (key != eadk_key_left && key != eadk_key_right) science_work.answer[0] = 0;
        }
        return true;
    }
    if (page == ScienceResult) {
        if (key == eadk_key_up || key == eadk_key_down)
            science_result_offset += key == eadk_key_up ? -1 : 1;
        return true;
    }
    if (page == UnitValue) {
        if (key == eadk_key_exe || key == eadk_key_ok) submit_conversion();
        else {
            edit(&unit_input, key, shifted, alphabetic, letter);
            unit_input_error = false;
        }
        return true;
    }
    if (page == UnitResult) return true;
    if (page == ToolForm) {
        if (key == eadk_key_up && active_field > 0) active_field--;
        else if (key == eadk_key_down && active_field+1 < field_count) active_field++;
        else if (key == eadk_key_ok || key == eadk_key_exe) submit_tool();
        else if (key != eadk_key_up && key != eadk_key_down)
            edit(&forms[active_field],key,shifted,alphabetic,letter);
        return true;
    }
    if (page == ToolOutput) {
        if (key == eadk_key_up || key == eadk_key_down) tool_offset += key == eadk_key_up ? -1 : 1;
        return true;
    }
    if (page == Reading || page == Result) {
        if (key == eadk_key_up || key == eadk_key_down) {
            read_offset += key == eadk_key_up ? -1 : 1;
            return true;
        }
        if (page == Reading) return true;
    }
    if (page == Form) {
        if (key == eadk_key_up && active_field > 0) active_field--;
        else if (key == eadk_key_down && active_field + 1 < field_count) active_field++;
        else if (key == eadk_key_exe || key == eadk_key_ok) submit_form(false);
        else edit(&forms[active_field], key, shifted, alphabetic, letter);
        return true;
    }
    if (key == eadk_key_exe || key == eadk_key_ok) submit_prompt();
    else if (key == eadk_key_up && page == Prompt && calc_count) open_calculation_history();
    else if (key == eadk_key_up && page == Prompt && has_result) {
        result_parent = Prompt;
        page = Result;
    }
    else edit(&prompt, key, shifted, alphabetic, letter);
    return true;
}

static bool repeats(int key) {
    return (key >= eadk_key_left && key <= eadk_key_right) || key == eadk_key_backspace;
}

int main(void) {
    eadk_keyboard_state_t previous = eadk_keyboard_scan();
    const uint64_t shift_bit = UINT64_C(1) << eadk_key_shift;
    const uint64_t alpha_bit = UINT64_C(1) << eadk_key_alpha;
    int repeat_key = -1, repeat_ticks = 0;
    bool repeat_shift = false, repeat_alpha = false, used_shift = false, used_alpha = false;
    draw();
    for (;;) {
        eadk_keyboard_state_t state = eadk_keyboard_scan();
        eadk_keyboard_state_t pressed = state & ~previous;
        previous = state;
        if (eadk_keyboard_key_down(pressed, eadk_key_home)) return 0;

        bool redraw = false;
        if (used_shift && !(state & shift_bit)) {
            used_shift = shift = false;
            redraw = true;
        }
        if (used_alpha && !(state & alpha_bit)) {
            used_alpha = false;
            if (alpha != AlphaLocked) alpha = AlphaOff;
            redraw = true;
        }
        if (pressed & shift_bit) {
            shift = !shift;
            redraw = true;
        }
        if (pressed & alpha_bit) {
            alpha = (alpha + 1) % 3;
            if (alpha == AlphaOff) shift = false;
            redraw = true;
        }
        pressed &= ~(shift_bit | alpha_bit);

        if (repeat_key >= 0 && eadk_keyboard_key_down(state, (eadk_key_t)repeat_key)) {
            if (repeat_shift != (shift || (state & shift_bit)) ||
                repeat_alpha != (alpha != AlphaOff || (state & alpha_bit))) repeat_key = -1;
            else if (++repeat_ticks >= 22 && (repeat_ticks - 22) % 4 == 0) {
                handle_key(repeat_key, repeat_shift, repeat_alpha);
                redraw = true;
            }
        } else repeat_key = -1;

        for (int key = 0; pressed && key <= eadk_key_exe; key++) {
            if (!eadk_keyboard_key_down(pressed, (eadk_key_t)key)) continue;
            bool shifted = shift || (state & shift_bit);
            bool alphabetic = alpha != AlphaOff || (state & alpha_bit);
            if (!handle_key(key, shifted, alphabetic)) return 0;
            used_shift = (state & shift_bit) != 0;
            used_alpha = (state & alpha_bit) != 0;
            if (alpha != AlphaLocked) {
                if (!used_shift) shift = false;
                if (!used_alpha) alpha = AlphaOff;
            }
            if (repeats(key) && !(selection_for(page) && (key == eadk_key_left || key == eadk_key_right))) {
                repeat_key = key;
                repeat_ticks = 0;
                repeat_shift = shifted;
                repeat_alpha = alphabetic;
            }
            redraw = true;
        }
        if (redraw) draw();
        eadk_timing_msleep(15);
    }
}
