#include "eadk.h"
#include "topics.h"
#include "physics.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumAI";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

enum {
    White = 0xffff, Ink = 0x2104, Grey = 0x6b4d, Light = 0xef7d,
    Line = 0xd6ba, Gold = 0xfda6, PaleGold = 0xffb8, Blue = 0x32b8
};
static const char higher_label[] = "(higher only)";

enum { CHAR_WIDTH = 7, TEXT_WIDTH = 43, FOOTER = 226, HISTORY_LIMIT = 5, WRAP_LIMIT = 80 };

typedef enum { Prompt, Toolbox, Chapters, Topics, TopicMenu, Examples, Form, Reading,
               Result, CasMenu, History, Settings, PhysicsList, PhysicsDetail,
               PhysicsSubjects, PhysicsForm } Page;
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

static Page page, form_parent, result_parent, settings_parent, reading_parent;
static Selection course_selection, chapter_selection, topic_selection, topic_action, cas_selection, settings_selection;
static Selection history_selection, example_selection, physics_selection, physics_subject;
static Field physics_fields[PHYSICS_VARIABLES];
static int physics_focus, physics_offset;
static bool physics_answer_ok;
static char physics_notes[PHYSICS_REFERENCE_SIZE], physics_answer[EXPRESSION_SIZE];
static Field prompt, forms[MAX_FIELDS];
static const char *form_labels[MAX_FIELDS];
static int field_count, active_field, read_offset, read_length;
static const Topic *form_topic;
static Operation form_operation, result_operation;
static HistoryEntry history[HISTORY_LIMIT];
static Solution solution;
static int history_count;
static bool has_result, degrees = true, decimal, show_steps = true, shift;
static enum { AlphaOff, AlphaOnce, AlphaLocked } alpha;
static char clipboard[EXPRESSION_SIZE];
static char ans[EXPRESSION_SIZE], result_question[EXPRESSION_SIZE];
static const char *read_content, *read_title;
static WrappedLine wrapped[WRAP_LIMIT];

static void rectangle(int x, int y, int width, int height, eadk_color_t colour) {
    if (width <= 0 || height <= 0) return;
    eadk_display_push_rect_uniform((eadk_rect_t){x, y, width, height}, colour);
}

static void string_at(int x, int y, const char *text, eadk_color_t ink, eadk_color_t paper) {
    if (!*text) return;
    eadk_display_draw_string(text, (eadk_point_t){x, y}, false, ink, paper);
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
    int columns = TEXT_WIDTH;
    if (tag) {
        int width = (int)strlen(tag) * CHAR_WIDTH;
        string_at(312 - width, 3, tag, Ink, paper);
        columns -= (width / CHAR_WIDTH) + 2;
    }
    title_with_tier(8, 3, title, columns, filled ? White : Blue, paper);
    rectangle(0, 21, 320, 2, Gold);
}

static void footer(const char *label) {
    eadk_color_t paper = Light;
    rectangle(0, FOOTER, 320, 14, paper);
    char line[TEXT_WIDTH + 1];
    const char *modifier = shift ? (alpha ? "ABC " : "SHIFT ") :
                           alpha == AlphaLocked ? "abc* " : alpha ? "abc " : "";
    snprintf(line, sizeof(line), "%s%s", modifier, label);
    clipped(5, FOOTER, line, TEXT_WIDTH, Grey, paper);
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
        string_at(11, y + 1, text, Ink, l.highlight ? PaleGold : White);
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
    return &physics_equations[physics_selection.row];
}

static int physics_slot(int field) {
    return field >= physics_subject.row ? field + 1 : field;
}

static const char *selection_text(Page menu, int i) {
    switch (menu) {
        case Toolbox: return courses[i].title;
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
            if (i < OperationCount) return operation_names[i];
            return i == OperationCount ? "Physics equation sheet" : "History";
        case PhysicsSubjects: {
            static char label[80];
            const PhysicsVariable *variable = &equation()->variables[i];
            snprintf(label, sizeof(label), "%s: %s [%s]", variable->symbol, variable->name, variable->unit);
            return label;
        }
        case History: return history[i].question;
        case Settings: {
            if (i == 0) return degrees ? "Angle unit                    Degrees" : "Angle unit                    Radians";
            if (i == 1) return decimal ? "Numeric answers               Decimal" : "Numeric answers              Fractions";
            if (i == 2) return show_steps ? "Worked steps                      On" : "Worked steps                     Off";
            return "Help and keyboard";
        }
        default: return "";
    }
}

static int count_for(Page menu) {
    switch (menu) {
        case Toolbox: return course_count;
        case Chapters: return course()->count;
        case Topics: return chapter()->count;
        case TopicMenu: return topic()->labels[0] ? 4 : 3;
        case Examples: return 1 + EXTRA_EXAMPLES;
        case CasMenu: return OperationCount + 2;
        case PhysicsList: return physics_equation_count;
        case PhysicsSubjects: return equation()->count;
        case History: return history_count;
        case Settings: return 4;
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
        string_at(8, 163, "Course notes, methods and examples", Grey, White);
        string_at(8, 179, "Choose a course to browse its topics.", Grey, White);
    }
    if (!count) string_at(8, 36, "No saved calculations yet.", Grey, White);
    scrollbar(s->first, count, rows, y0, row_height * rows);
    footer(page == Settings ? "OK: change   BACK: return" : "OK: choose   UP/DOWN: move   BACK");
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
    const int rows = 3, height = 65, top = 25;
    Selection *sel = &physics_selection;
    header("Physics equation sheet", "8463", false);
    if (sel->row < sel->first) sel->first = sel->row;
    if (sel->row >= sel->first + rows) sel->first = sel->row - rows + 1;
    for (int offset = 0; offset < rows && sel->first + offset < physics_equation_count; offset++) {
        int row = sel->first + offset, y = top + offset * height;
        bool active = row == sel->row;
        const PhysicsEquation *item = &physics_equations[row];
        eadk_color_t paper = active ? PaleGold : White;
        if (active) {
            rectangle(4, y, 311, height - 1, paper);
            string_at(8, y + 2, ">", Blue, paper);
        }
        char title[80];
        equation_title(item, title, sizeof(title));
        title_with_tier(21, y + 2, title, 41, active ? Blue : Ink, paper);
        int line_count = 0;
        wrap(item->word, 41, false, &line_count);
        for (int i = 0; i < line_count && i < 3; i++)
            display_wrapped_line(&wrapped[i], 21, y + 18 + i * 14, Grey, paper);
    }
    scrollbar(sel->first, physics_equation_count, rows, top, rows * height);
    footer("OK: equation   UP/DOWN: browse   BACK");
}

static void draw_physics_detail(void) {
    char title[80];
    equation_title(equation(), title, sizeof(title));
    header(title, NULL, false);
    read_length = 0;
    read_offset = physics_offset;
    wrap(physics_notes, TEXT_WIDTH, false, &read_length);
    reader(26, 199);
    physics_offset = read_offset;
    rectangle(6, 201, 308, 22, PaleGold);
    string_at(12, 205, "Solver                          OK / EXE", Blue, PaleGold);
    footer("UP/DOWN: read                       BACK");
}

static void draw_physics_form(void) {
    const PhysicsEquation *item = equation();
    const PhysicsVariable *subject = &item->variables[physics_subject.row];
    header("Physics solver", subject->symbol, false);
    char caption[80];
    snprintf(caption, sizeof(caption), "Find %s [%s]", subject->name, subject->unit);
    clipped(8, 27, caption, TEXT_WIDTH, Blue, White);
    for (int i = 0; i < item->count - 1; i++) {
        const PhysicsVariable *v = &item->variables[physics_slot(i)];
        int y = 47 + i*39;
        snprintf(caption, sizeof(caption), "%s: %s [%s]", v->symbol, v->name, v->unit);
        clipped(8, y, caption, TEXT_WIDTH, i == physics_focus ? Blue : Grey, White);
        entry(7, y + 15, 306, &physics_fields[physics_slot(i)], NULL, i == physics_focus);
    }
    if (*physics_answer) {
        int count = 0;
        wrap(physics_answer, TEXT_WIDTH, false, &count);
        eadk_color_t paper = physics_answer_ok ? PaleGold : Light;
        rectangle(6, 168, 308, 14*count + 4, paper);
        for (int i = 0; i < count; i++)
            display_wrapped_line(&wrapped[i], 10, 170 + i*14, Ink, paper);
    }
    footer("UP/DOWN: field   EXE: solve   BACK");
}

static void draw(void) {
    eadk_display_push_rect_uniform(eadk_screen_rect, White);
    switch (page) {
        case Prompt:
            header("NumAI", degrees ? "UK DEG" : "UK RAD", true);
            string_at(8, 36, "Your exercises on NumWorks", Ink, White);
            string_at(8, 59, "TOOLBOX  Courses and exercises", Grey, White);
            string_at(8, 77, "VAR      CAS and physics equations", Grey, White);
            string_at(8, 95, "SHIFT+EXE  Settings", Grey, White);
            string_at(8, 126, ">* Factorise  >/ Simplify  => Solve", Grey, White);
            string_at(8, 143, ">** Complete the square", Grey, White);
            entry(4, 202, 312, &prompt, "Type a calculation, or press TOOLBOX", true);
            footer("TOOLBOX: courses   VAR: CAS   EXE");
            break;
        case Toolbox: draw_list("Toolbox", "UK", true); break;
        case Chapters: draw_list(course()->title, course()->code, false); break;
        case Topics: draw_list(chapter()->title, NULL, false); break;
        case TopicMenu: draw_list(topic()->title, NULL, false); break;
        case Examples: draw_list("Worked examples", NULL, false); break;
        case PhysicsList: draw_physics_list(); break;
        case PhysicsDetail: draw_physics_detail(); break;
        case PhysicsSubjects: draw_list("Choose the unknown", NULL, false); break;
        case PhysicsForm: draw_physics_form(); break;
        case CasMenu: draw_list("CAS and history", NULL, false); break;
        case History: draw_list("History", NULL, false); break;
        case Settings: draw_list("Settings", NUMAI_VERSION, false); break;
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

static const char help[] =
    "TOOLBOX opens courses. VAR opens algebra, calculus and history. SHIFT+EXE opens settings.\n"
    "For direct CAS, start with >* (factorise), >/ (simplify), => (solve) or >** (complete the square).\n"
    "Use x,n,t for x, SHIFT+pi for = and SHIFT+x^2 for >.\n"
    "ALPHA enters letters. SHIFT then ALPHA gives capitals. Press ALPHA twice to lock.\n"
    "SHIFT+arrows selects text. SHIFT+x,n,t cuts, SHIFT+VAR copies and SHIFT+TOOLBOX pastes. "
    "SHIFT+DEL clears. SHIFT+power stores a numeric result in a letter for this session.\n"
    "In a form, UP/DOWN moves between values. Typing replaces a selected default. EXE works it out.\n"
    "CAS uses one variable x. Solve handles real polynomial roots. The interval tool bisects a continuous function. "
    "Indefinite integrals cover polynomials and selected elementary functions. Definite integrals accept polynomials.\n"
    "Numeric toolbox functions include ncr(n,r), npr(n,r), binom(n,r,p), and norm(z).\n"
    "BACK moves up one screen. HOME returns to the launcher.";

static void back(void) {
    switch (page) {
        case Toolbox: case CasMenu: page = Prompt; break;
        case Chapters: page = Toolbox; break;
        case Topics: page = Chapters; break;
        case TopicMenu: page = Topics; break;
        case Examples: page = TopicMenu; break;
        case PhysicsList: page = CasMenu; break;
        case PhysicsDetail: page = PhysicsList; break;
        case PhysicsSubjects: page = PhysicsDetail; break;
        case PhysicsForm: page = PhysicsSubjects; break;
        case Form: page = form_parent; break;
        case Result: page = result_parent; break;
        case Settings: page = settings_parent; break;
        case History: page = CasMenu; break;
        case Reading: page = reading_parent; break;
        default: break;
    }
}

static void choose(void) {
    switch (page) {
        case Toolbox:
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
            if (cas_selection.row == OperationCount) {
                physics_selection = (Selection){0};
                page = PhysicsList;
            } else if (cas_selection.row == OperationCount + 1) {
                history_selection = (Selection){0};
                page = History;
            } else form_for((Operation)cas_selection.row, NULL, CasMenu);
            break;
        case PhysicsList:
            physics_offset = 0;
            physics_subject = (Selection){0};
            for (int i = 0; i < equation()->count; i++) set_field(&physics_fields[i], "", false);
            physics_reference(equation(), physics_notes);
            page = PhysicsDetail;
            break;
        case PhysicsSubjects:
            physics_focus = 0;
            physics_answer[0] = 0;
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
            if (settings_selection.row == 3) begin_read("Help and keyboard", help);
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

static bool handle_key(int key, bool shifted, bool alphabetic) {
    if (key == eadk_key_xnt && alphabetic) return true;
    if (key == eadk_key_exe && shifted) {
        if (page == Settings) page = settings_parent;
        else {
            settings_parent = page;
            settings_selection = (Selection){0};
            page = Settings;
        }
        return true;
    }
    char letter = alphabetic ? alpha_character(key, shifted) : '\0';
    bool function = shifted && !alphabetic;
    Field *field = page == PhysicsForm ? &physics_fields[physics_slot(physics_focus)] :
                   page == Form ? &forms[active_field] :
                   (page == Prompt || page == Result) ? &prompt : NULL;
    if (field && (letter || (function && (key <= eadk_key_right || key == eadk_key_xnt ||
                                          key == eadk_key_var || key == eadk_key_toolbox)))) {
        edit(field, key, shifted, alphabetic, letter);
        if (page == PhysicsForm) physics_answer[0] = 0;
        return true;
    }
    if (letter || (function && (key == eadk_key_var || key == eadk_key_toolbox))) return true;

    if (key == eadk_key_back) {
        if (page == Prompt) return false;
        back();
        return true;
    }
    if ((key == eadk_key_toolbox || key == eadk_key_var) &&
        (page == Prompt || page == Result || page == Toolbox || page == CasMenu)) {
        Page target = key == eadk_key_toolbox ? Toolbox : CasMenu;
        page = page == target ? Prompt : target;
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
        else if (key == eadk_key_ok || key == eadk_key_exe) page = PhysicsSubjects;
        return true;
    }
    if (page == PhysicsForm) {
        int count = equation()->count - 1;
        if (key == eadk_key_up && physics_focus > 0) physics_focus--;
        else if (key == eadk_key_down && physics_focus + 1 < count) physics_focus++;
        else if (key == eadk_key_exe || key == eadk_key_ok) {
            const char *values[PHYSICS_VARIABLES] = {0};
            for (int i = 0; i < count; i++) values[physics_slot(i)] = physics_fields[physics_slot(i)].text;
            physics_answer_ok = physics_solve(equation(), physics_subject.row, values,
                                              physics_answer, sizeof(physics_answer));
        } else if (key != eadk_key_up && key != eadk_key_down) {
            edit(&physics_fields[physics_slot(physics_focus)], key, shifted, alphabetic, letter);
            if (key != eadk_key_left && key != eadk_key_right) physics_answer[0] = 0;
        }
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
