"""Run the loader-linked NWA on a Cortex-M7 with simulated EADK inputs/display."""
from collections import deque
from pathlib import Path
import math
import re
from struct import pack, unpack
import sys

from elftools.elf.elffile import ELFFile
from PIL import Image, ImageDraw
from unicorn import (
    Uc, UC_ARCH_ARM, UC_HOOK_CODE, UC_HOOK_INTR, UC_MODE_MCLASS, UC_MODE_THUMB,
    UC_PROT_ALL, UC_PROT_NONE,
)
from unicorn.arm_const import (
    UC_ARM_REG_C1_C0_2, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_SP, UC_CPU_ARM_CORTEX_M7,
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
)

KEY = {
    "LEFT": 0, "UP": 1, "DOWN": 2, "RIGHT": 3, "OK": 4, "BACK": 5, "HOME": 6,
    "SHIFT": 12, "ALPHA": 13, "XNT": 14, "VAR": 15, "TOOLBOX": 16, "DEL": 17,
    "EXP": 18, "LN": 19, "LOG": 20, "I": 21, ",": 22, "^": 23,
    "SIN": 24, "COS": 25, "TAN": 26, "PI": 27, "SQRT": 28, "SQUARE": 29, "7": 30, "8": 31, "9": 32, "(": 33, ")": 34,
    "4": 36, "5": 37, "6": 38, "*": 39, "/": 40, "1": 42, "2": 43, "3": 44,
    "+": 45, "-": 46, "0": 48, ".": 49, "ANS": 51, "EXE": 52,
}
REGS = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
STACK = 0x20040000
STOP = 0x1000


class Simulator:
    def __init__(self, path):
        with path.open("rb") as stream:
            elf = ELFFile(stream)
            self.symbol = {r.name: r["st_value"] for r in elf.get_section_by_name(".symtab").iter_symbols()}
            self.entry = elf["e_entry"]
            self.cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
            self.cpu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
            self.cpu.mem_map(0x90000000, 0x800000)
            self.cpu.mem_map(0x20000000, 0x40000)
            self.cpu.mem_map(STOP, 0x1000)
            for segment in elf.iter_segments():
                if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
                    self.cpu.mem_write(segment["p_paddr"], segment.data())
        self.cpu.reg_write(UC_ARM_REG_C1_C0_2, 0xF00000)
        self.cpu.reg_write(UC_ARM_REG_SP, STACK)
        self.cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        self.keyboard = deque()
        self.screen = Image.new("RGB", (320, 240), "white")
        self.draw = ImageDraw.Draw(self.screen)
        self.strings = []
        self.waiting = False
        self.exited = False
        if "draw_text" in self.symbol:
            address = self.symbol["draw_text"] & ~1
            self.cpu.hook_add(UC_HOOK_CODE, self.capture_text, begin=address, end=address)
        self.cpu.hook_add(UC_HOOK_INTR, self.syscall)
        self.cpu.hook_add(UC_HOOK_CODE, self.exit, begin=STOP, end=STOP)

    def syscall(self, cpu, interrupt, _):
        assert interrupt == 2, f"Unexpected ARM exception {interrupt}"
        pc = cpu.reg_read(UC_ARM_REG_PC)
        number, opcode = cpu.mem_read(pc - 2, 2)
        assert opcode == 0xDF, hex(pc)
        a, b, c, d = (cpu.reg_read(reg) for reg in REGS)
        if number == 19:  # eadk_display_push_rect
            self.blit(a, b, c)
        elif number == 20:  # eadk_display_push_rect_uniform
            self.rect(a, b, c, d)
        elif number == 34:  # eadk_keyboard_scan
            self.scan()
        elif number == 49:  # eadk_timing_msleep
            self.sleep()
        else:
            raise AssertionError(f"Unexpected EADK syscall {number}")

    def sleep(self, *_):
        if not self.keyboard:
            self.waiting = True
            self.cpu.emu_stop()

    def scan(self, *_):
        state = self.keyboard.popleft() if self.keyboard else 0
        self.cpu.reg_write(UC_ARM_REG_R0, state & 0xFFFFFFFF)
        self.cpu.reg_write(UC_ARM_REG_R1, state >> 32)

    @staticmethod
    def colour(c):
        return ((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31

    def rect(self, a, b, colour, _):
        x, y, w, h = a & 65535, a >> 16, b & 65535, b >> 16
        assert x + w <= 320 and y + h <= 240, (x, y, w, h)
        if (x, y, w, h) == (0, 0, 320, 240):
            self.strings.clear()
        self.draw.rectangle((x, y, x + w - 1, y + h - 1), fill=self.colour(colour))

    def c_string(self, address, limit=128):
        text = bytearray()
        for i in range(limit):
            char = self.cpu.mem_read(address + i, 1)[0]
            if not char:
                break
            text.append(char)
        else:
            raise AssertionError("No string terminator")
        return text.decode()

    def capture_text(self, cpu, *_):
        x, y, address = (cpu.reg_read(reg) for reg in REGS[:3])
        text = self.c_string(address)
        assert x + len(text) * 7 <= 320 and y + 14 <= 240, (x, y, text)
        if text:
            self.strings.append(text)

    def blit(self, a, b, address):
        x, y, w, h = a & 65535, a >> 16, b & 65535, b >> 16
        assert x + w <= 320 and y + h <= 240 and w and h, (x, y, w, h)
        raw = self.cpu.mem_read(address, w * h * 2)
        tile = Image.frombytes("RGB", (w, h), bytes(raw), "raw", "BGR;16")
        self.screen.paste(tile, (x, y))

    def exit(self, cpu, *_):
        self.exited = True
        cpu.emu_stop()

    def run(self, address=None):
        self.waiting = False
        pc = self.cpu.reg_read(UC_ARM_REG_PC) | 1 if address is None else address
        # A batch of key presses can redraw the screen once per key.
        self.cpu.emu_start(pc, 0, count=18_000_000 + len(self.keyboard) * 500_000)
        assert self.waiting or self.exited, hex(self.cpu.reg_read(UC_ARM_REG_PC))

    def press(self, *keys):
        for key in keys:
            if isinstance(key, str):
                key = (key,)
            self.keyboard.extend((sum(1 << KEY[k] for k in key), 0))
        self.run()

    def see(self, *phrases):
        screen = "\n".join(self.strings)
        for phrase in phrases:
            assert phrase.casefold() in screen.casefold(), f"{phrase!r} missing from screen:\n{screen}"


def check_ui(path, screenshots):
    app = Simulator(path)
    # Stress the launcher and shortcuts with a 4 KiB stack budget.
    guard_base, guard_length = STACK - 0x4000, 0x3000
    app.cpu.mem_protect(guard_base, guard_length, UC_PROT_NONE)
    app.run(app.entry)
    app.see("NUMAI", "deg", "TOOLBOX", "VAR")
    assert app.screen.getpixel((0, 0)) == app.colour(0xfda6)
    assert app.screen.getpixel((319, 0)) == app.colour(0xfda6)
    assert not any("UK" in line for line in app.strings)
    app.screen.save(screenshots / "startup.png")
    app.press(("ALPHA", "XNT"))
    app.see("NUMAI", "deg")
    app.press(("SHIFT", "EXE"))
    app.see("Settings", "Degrees")
    app.press("DOWN", "DOWN", "DOWN", "OK")
    app.see("Keyboard shortcuts", "Key bind", "Function", "TOOLBOX", "Browse courses and topics")
    app.screen.save(screenshots / "keyboard-shortcuts.png")
    app.cpu.mem_protect(guard_base, guard_length, UC_PROT_ALL)
    app.press(*["DOWN"] * 28)
    app.see(">**", "Complete the square")
    assert "Browse courses and topics" not in app.strings
    app.press("BACK")
    app.see("Settings", "Keyboard shortcuts")
    app.press("EXE")
    app.see("TOOLBOX", "Browse courses and topics")
    app.press(("SHIFT", "EXE"))
    app.see("deg")
    app.press(("SHIFT", "EXE"))
    app.press("BACK", "SIN", "3", "0", ")", "EXE")
    app.see("sin", "30", "0.5")
    app.press("BACK")
    app.press(("SHIFT", "EXE"), "OK", "BACK")
    app.see("NUMAI", "rad")
    app.press("TOOLBOX")
    app.see("Edexcel IGCSE Maths A", "OCR Level 3 Additional Maths", "Edexcel A Level Maths",
            "AQA Physics", "AQA Biology", "AQA Chemistry")
    app.screen.save(screenshots / "toolbox.png")
    app.press("OK", "OK", "OK")
    app.see("Integers, primes and HCF", "Course notes", "Method and working", "Enter your values")
    app.press("DOWN", "DOWN", "OK")
    app.see("First integer", "84", "Second integer", "126")
    app.press("1", "8", "DOWN", "2", "4")
    app.see("18", "24")
    app.screen.save(screenshots / "exercise.png")
    app.press("EXE")
    app.see("gcd((18),(24))", "6")
    app.press("BACK")
    app.see("18", "24")
    app.press("BACK", "BACK", "BACK", "BACK", "DOWN", "OK")
    app.see("6993", "Enumeration", "Numerical methods")
    app.press("BACK", "DOWN", "OK", *["DOWN"] * 18)
    app.see("9MA0", "Mechanics: Moments", "Stats: Hypothesis testing")
    app.press("OK", "OK", "DOWN", "DOWN", "OK", "EXE")
    app.see("Moment", "21")
    app.press("VAR", *["DOWN"] * 4, "OK", "EXE")
    app.see("Factorise", "(x - 2)(x - 3)")
    app.screen.save(screenshots / "worked-solution.png")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Simplify", "5*x - 2")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Expand", "x^2 - 2*x - 3")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Solve for x", "x = 2, 3")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Differentiate", "3*x^2 - 4")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Integrate", "x^3 - 4*x + C")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Definite integral", "14/3")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Substitute x", "Substitute x = 2", "5")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Root in an interval", "x approx. 0.7390851332")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Complete the square", "2*(x + 3)^2 - 7")
    app.press("BACK", "BACK", "DOWN", "OK", "DOWN", "DOWN", "OK")
    app.see("Substitute x", "Substitute x = 2")
    app.press(("SHIFT", "EXE"))
    app.see("Settings", "Radians", "Fractions", "Worked steps")
    app.press("OK", "DOWN", "OK", "DOWN", "OK", "BACK")
    app.see("Substitute x", "5")
    assert "Substitute x = 2" not in app.strings
    app.press("SHIFT", "EXE")
    app.see("Settings", "Degrees", "Decimal", "Off")
    app.press("BACK", "BACK", "BACK", "BACK")
    app.see("NUMAI")
    app.press("XNT", "SQUARE", "-", "2", "SHIFT", "PI", "0", "EXE")
    app.see("x^2-2=0", "1.414213562")
    app.press("HOME")
    assert app.exited
    print("Cortex-M7 keypad and display smoke tests passed")


def check_keyboard(path, screenshots):
    app = Simulator(path)
    app.run(app.entry)
    greater = ("SHIFT", "SQUARE")
    equal = ("SHIFT", "PI")
    app.press(greater, "*", "XNT", "SQUARE", "-", "5", "XNT", "+", "6", "EXE")
    app.see("Factorise", "(x - 2)(x - 3)")
    app.press("BACK", greater, "/", "3", "XNT", "+", "2", "(", "XNT", "-", "1", ")", "EXE")
    app.see("Simplify", "5*x - 2")
    app.press("BACK", equal, greater, "2", "XNT", "+", "4", equal, "0", "EXE")
    app.see("Solve for x", "x = -2")
    app.press("BACK", "SHIFT", "SQUARE", "*", "*", ("SHIFT", "LOG"),
              "2", "XNT", "SQUARE", "+", "1", "2", "XNT", "+", "1", "1",
              ("SHIFT", "I"), "EXE")
    app.see("Complete the square", "2*(x + 3)^2 - 7")
    app.screen.save(screenshots / "completed-square.png")
    app.press("BACK", ("SHIFT", "SIN"), "0", ".", "5", ")", "EXE")
    app.see("asin", "0.5", "30")
    app.press("BACK", "1", "2", "3", ("SHIFT", "LEFT"), ("SHIFT", "LEFT"))
    draft = app.symbol["prompt"]
    app.see("23")
    assert unpack("<2i", app.cpu.mem_read(draft + 112, 8)) == (1, 3)
    app.press(("SHIFT", "VAR"), ("SHIFT", "XNT"))
    assert app.c_string(draft) == "1"
    app.press(("SHIFT", "TOOLBOX"), ("SHIFT", "TOOLBOX"))
    assert app.c_string(draft) == "12323"
    app.press(("SHIFT", "UP"), "4", ("SHIFT", "DEL"))
    assert app.c_string(draft) == ""
    app.press(("SHIFT", "EXP"), ("SHIFT", "LN"), ("SHIFT", "LOG"), ("SHIFT", "I"), ("SHIFT", ","))
    assert app.c_string(draft) == "[]{}_"
    app.press(("SHIFT", "DEL"), "ALPHA", "VAR", "ALPHA", "TOOLBOX", "ALPHA", "0", "ALPHA", ".")
    assert app.c_string(draft) == ';"?!'
    app.press(("SHIFT", "DEL"), "ALPHA", "ALPHA", "EXP", "LN", "DEL", "ALPHA")
    assert app.c_string(draft) == "a"
    app.press(("SHIFT", "DEL"), "1", "2", ("SHIFT", "^"), "SHIFT", "ALPHA", "EXP", "EXE")
    app.see("12->A", "Store the result in A")
    app.press("BACK", "SHIFT", "ALPHA", "EXP", "+", "1", "EXE")
    app.see("A", "+", "13")
    app.press("BACK", "2", "5", "ALPHA", "DEL", "EXE")
    app.see("25%", "0.25")
    app.press("BACK", "1", ("SHIFT", "SQRT"), "2", "EXE")
    app.see("1<2", "True")
    app.press("BACK", "HOME")
    assert app.exited
    print("CAS prefixes and NumWorks keypad mappings passed on ARM")


def check_engine(path):
    app = Simulator(path)
    app.run(app.entry)
    stage, stack = 0x20035000, STACK - 0x1000
    cases = [
        (0, "1/3+1/6", "", "", "1/2", True),
        (0, "-2^2+2^-3", "", "", "-31/8", True),
        (0, "x^0", "", "", "numeric expression", False),
        (0, "1/0", "", "", "real domain", False),
        (0, "binom(5,2,1/3)", "", "", "80/243", True),
        (1, "2x^2-7x+3=0", "", "", "(2x - 1)(x - 3) = 0", True),
        (1, "2x^2+2", "", "", "2*(x^2 + 1)", True),
        (1, "x^2/2-3x/2+1", "", "", "1/2*(x - 1)(x - 2)", True),
        (2, "(x^2-1)/(x-1)", "", "", "x + 1\nx - 1 != 0", True),
        (2, "2/(x-1)-1/(x+1)", "", "", "(x + 3)/(x^2 - 1)", True),
        (2, "0/(x-1)", "", "", "0\nx - 1 != 0", True),
        (3, "(x+4)(x-3)", "", "", "x^2 + x - 12", True),
        (4, "x^2-8=0", "", "", "+/- 2*sqrt(2)", True),
        (4, "x^2+1=0", "", "", "No real solutions", True),
        (4, "x=x", "", "", "All real x", True),
        (4, "2x+1=0", "", "", "-1/2", True),
        (4, "5x^2-10x+5=0", "", "", "x = 1", True),
        (4, "sin(x)=0.5", "", "", "polynomial", False),
        (5, "x^2*exp(-x)", "", "", "2*x*exp(-x)", True),
        (6, "1/(3x-1)", "", "", "ln(abs(3*x - 1))/3 + C", True),
        (6, "x^12", "", "", "1/13*x^13 + C", True),
        (7, "4-x^2", "-2", "2", "32/3", True),
        (8, "x^3+1", "-2", "", "-7", True),
        (9, "cos(x)=x", "0", "1", "0.7390851332", True),
        (9, "1/(x-1)", "0", "2", "discontinuity", False),
        (10, "2x^2+12x+11", "", "", "2*(x + 3)^2 - 7", True),
        (10, "-x^2+3x+1/8", "", "", "-(x - 3/2)^2 + 19/8", True),
        (10, "x^2+4x=-3", "", "", "(x + 2)^2 - 1 = 0", True),
        (10, "x^2", "", "", "x^2", True),
        (10, "(x+1)(2x-1)", "", "", "2*(x + 1/4)^2 - 9/8", True),
        (10, "x^2+2x+1", "", "", "(x + 1)^2", True),
        (10, "x+1", "", "", "Enter a quadratic", False),
        (10, "x^3", "", "", "Enter a quadratic", False),
    ]
    for operation, source, second, third, expected, ok in cases:
        args = []
        for i, value in enumerate((source, second, third)):
            address = stage + 256 * i
            app.cpu.mem_write(address, value.encode() + b"\0")
            args.append(address)
        result = stage + 0x400
        app.cpu.mem_write(stack, pack("<III", 0, 0, result))
        app.cpu.reg_write(UC_ARM_REG_SP, stack)
        app.cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        for reg, value in zip(REGS, (operation, *args)):
            app.cpu.reg_write(reg, value)
        app.exited = False
        app.run(app.symbol["solve_expression"] | 1)
        actual = app.c_string(result + 512, 512)
        assert app.exited and bool(app.cpu.mem_read(result + 1024, 1)[0]) == ok, (source, actual)
        assert expected in actual, (source, actual, expected)
    print(f"{len(cases)} CAS cases passed on ARM")


def check_catalog(path):
    app = Simulator(path)
    app.run(app.entry)
    app.press("TOOLBOX")
    course_total, = unpack("<i", app.cpu.mem_read(app.symbol["course_count"], 4))
    entries = 0
    forms = examples = science_cards = higher_cards = 0
    for course in range(course_total):
        course_addr = app.symbol["courses"] + 16 * course
        _, _, chapter_addr, chapter_count = unpack("<4I", app.cpu.mem_read(course_addr, 16))
        app.press("OK")
        for chapter in range(chapter_count):
            _, topic_addr, topic_count = unpack("<3I", app.cpu.mem_read(chapter_addr + 12*chapter, 12))
            app.press("OK")
            for topic in range(topic_count):
                title, note, method, example, *_, detail = unpack(
                    "<15I", app.cpu.mem_read(topic_addr + 60*topic, 60)
                )
                name = app.c_string(title)
                app.press("OK")
                if method:
                    app.see(name[:40], "Worked examples")
                    note_address, first_title, first_work, second_title, second_work = unpack(
                        "<5I", app.cpu.mem_read(detail, 20)
                    )
                    notes = app.c_string(note_address, 950)
                    assert notes.count("- ") >= 4 and len(notes) > 150, name
                    app.press("OK")
                    app.see(notes.split()[1])
                    if name == "Integers, primes and HCF":
                        app.screen.save(path.parent / "maths-notes.png")
                    app.press(*["DOWN"] * 16, "BACK")
                    app.press(*(["DOWN"] * (2 if example else 3)), "OK", "OK")
                    if not example:
                        forms += 1
                        solution = app.symbol["solution"]
                        status = app.cpu.mem_read(solution + 1024, 1)[0]
                        assert status, (name, app.c_string(solution + 512))
                    app.press("BACK")
                    for heading, working in [(first_title, first_work), (second_title, second_work)]:
                        example_name = app.c_string(heading)
                        example_text = app.c_string(working, 700)
                        app.press("DOWN", "OK")
                        app.see(example_name[:40], example_text.split()[0][:18])
                        assert len(example_text.splitlines()) >= 2 and len(example_text) >= 56, example_name
                        if name == "Integers, primes and HCF":
                            app.screen.save(path.parent / "maths-example.png")
                        app.press("BACK")
                        examples += 1
                    entries += 1
                    app.press("BACK", "BACK")
                else:
                    base, label, _ = name.partition(" (higher only)")
                    science_notes = app.c_string(note, 850)
                    reference = science_notes.splitlines()[0]
                    assert science_notes.count("\n- ") >= 2, base
                    app.see(base[:40], reference)
                    if label:
                        app.see("(higher only)")
                        higher_cards += 1
                        if name.startswith("Momentum ("):
                            app.screen.save(path.parent / "higher-physics.png")
                    else:
                        assert "(higher only)" not in app.strings
                    app.press("BACK")
                    if label:
                        app.see(base, "(higher only)")
                    science_cards += 1
                if topic + 1 < topic_count:
                    app.press("DOWN")
            app.press("BACK")
            if chapter + 1 < chapter_count:
                app.press("DOWN")
        app.press("BACK")
        if course + 1 < course_total:
            app.press("DOWN")
    assert (entries, forms, examples, science_cards, higher_cards) == (112, 58, 224, 397, 76)
    print(f"{entries} maths topics, {examples} new worked examples, {forms} calculator examples, "
          f"and {science_cards} science cards checked ({higher_cards} Higher-only labels)")


def check_physics(path):
    app = Simulator(path)
    app.run(app.entry)
    sheet = app.symbol["physics_equations"]
    count, = unpack("<I", app.cpu.mem_read(app.symbol["physics_equation_count"], 4))
    groups = [
        (9, 2, 3), (1, 200, 0.1), (58.8, 3, 9.8, 2), (16800, 0.5, 4200, 8),
        (300, 2400, 8), (150, 600, 4), (0.8, 360, 450), (0.75, 45, 60),
        (30, 0.25, 120), (6, 0.3, 20), (6, 12, 0.5), (12, 2, 3),
        (18000, 900, 20), (120, 20, 6), (2500, 0.3, 0.00012), (66800, 0.2, 334000),
        (400, 100000, 0.004), (49, 5, 9.8), (48, 12, 4), (12, 300, 0.04),
        (12, 30, 0.4), (20000, 600, 0.03), (19600, 2, 1000, 9.8), (150, 5, 30),
        (-3, -9, 3), (8, 4, 2, 12), (8, 2, 4), (-8, 4, -2),
        (-100, 0.2, -15, 0.03), (0.2, 5), (10, 50, 0.2), (3, 0.018, 0.006),
        (0.4, 0.4, 5, 0.2), (120, 240, 300, 600), (12, 2, 48, 0.5),
    ]
    assert len(groups) == count == 35
    work, result, reference = 0x20035000, 0x20035500, 0x20036000
    substitution, conversions, steps = result + 112, result + 332, result + 732
    stack = STACK - 0x1000

    def call(function, *arguments):
        for reg, value in zip(REGS, arguments):
            app.cpu.reg_write(reg, value)
        app.cpu.mem_write(stack, pack(f"<{len(arguments[4:])}I", *arguments[4:]))
        app.cpu.reg_write(UC_ARM_REG_SP, stack)
        app.cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        app.exited = False
        app.run(app.symbol[function] | 1)

    def solve(eq, subject, data):
        addresses = []
        for i, value in enumerate(data):
            address = work + 0x20 + 32*i
            app.cpu.mem_write(address, str(value).encode() + b"\0")
            addresses.append(0 if i == subject else address)
        app.cpu.mem_write(work, pack("<6I", *addresses, *[0]*(6-len(addresses))))
        call("physics_solve", eq, subject, work, result)
        return bool(app.cpu.reg_read(UC_ARM_REG_R0)), app.c_string(result), app.c_string(substitution, 220)

    equation_size, variable_size = 156, 20
    tiers = []
    sections = []
    for i, data in enumerate(groups):
        eq = sheet + equation_size*i
        title, _, section, _, _, _, higher, terms = unpack("<8I", app.cpu.mem_read(eq, 32))
        sections.append(app.c_string(section))
        assert terms == len(data)
        if higher:
            tiers.append(i + 1)
        call("physics_word_equation", eq, reference)
        words = app.c_string(reference, 432)
        app.cpu.mem_write(work, pack("<I", 0))
        call("wrap", reference, 41, 0, work)
        assert unpack("<I", app.cpu.mem_read(work, 4))[0] <= 5, app.c_string(title)
        call("physics_reference", eq, reference)
        facts = app.c_string(reference, 4096)
        assert words in facts and "*" not in facts and facts.endswith(".\n"), app.c_string(title)
        assert facts.count("Use: Find ") == terms, app.c_string(title)
        app.cpu.mem_write(work, pack("<I", 0))
        call("wrap", reference, 43, 0, work)
        assert unpack("<I", app.cpu.mem_read(work, 4))[0] < 80, app.c_string(title)
        for subject, expected in enumerate(data):
            variable = eq + 32 + variable_size*subject
            symbol_ptr, name_ptr, unit_ptr, _, _ = unpack("<5I", app.cpu.mem_read(variable, variable_size))
            symbol, name, unit = (app.c_string(pointer) for pointer in (symbol_ptr, name_ptr, unit_ptr))
            assert len(f"Find {name} [{unit}]") <= 43
            assert len(f"{symbol}: {name} [{unit}]") <= 41
            call("physics_subject_equation", eq, subject, result, 112)
            symbolic = app.c_string(result)
            assert symbolic.startswith(symbol + "=") and symbolic in facts and "*" not in symbolic
            assert name + " [" + unit + "]" in words
            assert name + " [" + unit + "] =" in facts.split("Rearrangements\n")[1]
            assert re.search(re.escape(f"- {symbol}: {name} [{unit}]\n  ") + r"[^\n]+\n", facts)
            ok, answer, substituted = solve(eq, subject, data)
            assert ok, (app.c_string(title), subject, answer)
            match = re.search(r" = (?:\+/- )?(-?[0-9.e+\-]+)", answer)
            assert match and math.isclose(float(match[1]), expected, rel_tol=2e-9, abs_tol=1e-9), (
                app.c_string(title), expected, answer
            )
            assert answer[match.end():] == (" (no unit)" if unit == "no unit" else " " + unit), answer
            assert substituted.startswith(symbol + "=") and "*" not in substituted and "$" not in substituted
    assert tiers == [23, 28, 29, 33, 34, 35]
    assert solve(sheet + 28*equation_size, 0, groups[28])[2] == "F=0.2 x (-15)/0.03"
    heating = sheet + 3*equation_size
    ok, _, substituted = solve(heating, 0, (0, 1.2345678901234567e100, 9.876543210987654e-200, -2.012345678901234e100))
    assert ok and "1.2345678901234567e+100" in substituted
    app.cpu.mem_write(reference, ("Substitute: " + substituted).encode() + b"\0")
    app.cpu.mem_write(work, pack("<I", 0))
    call("wrap", reference, 43, 0, work)
    assert unpack("<I", app.cpu.mem_read(work, 4))[0] <= 3
    assert not solve(sheet + 7*equation_size, 0, (0, 60, 45))[0]
    assert not solve(sheet + 4*equation_size, 0, (0, 10, 0))[0]
    assert not solve(sheet + 25*equation_size, 0, (0, 1, -2, 2))[0]
    print(f"{count} physics references and {sum(map(len, groups))} rearrangements checked on ARM")

    chemistry = app.symbol["chemistry_equations"]
    chemistry_count, = unpack("<I", app.cpu.mem_read(app.symbol["chemistry_equation_count"], 4))
    chemistry_examples = [
        (12, 23, 11), (35.5, 35, 37, 75), (18.96, 24, 79), (32, 16, 2),
        (74, 40, 32, 2, 0, 0), (3200/44, 32, 44), (25, 19, 6), (1, 22, 20),
        (20, 5, .25), (75, 6, 8), (100, 80, 80), (.5, 9, 18), (1.505e23, .25),
        (.6, .3, 2, 1), (1.5, .3, .2), (1.5, 3, 2, 2, 1), (5.6, 10, 100, 56, 1, 1),
        (.1, .025, .25), (8, .2, 40), (.04, .1, .025, .02, 1, 2), (3, .125),
        (10, 15, 2, 3), (.06, 1.8, 30), (2, 48, 24), (.002, .03, 15),
        (1.5, 30, 20), (.4, 2.8, 7), (-350, 1300, 1650), (100, 5, 3),
    ]
    assert len(chemistry_examples) == chemistry_count
    higher_only = set(range(11, 22)) | {24, 25, 27, 28}
    for i, values in enumerate(chemistry_examples):
        eq = chemistry + i*equation_size
        title, _, _, _, _, _, higher, terms = unpack("<8I", app.cpu.mem_read(eq, 32))
        title = app.c_string(title)
        assert bool(higher) == (i in higher_only), title
        assert terms == len(values), title
        call("physics_reference", eq, reference)
        facts = app.c_string(reference, 4096)
        assert "*" not in facts and "Worked example\n" in facts and facts.endswith(".\n"), title
        app.cpu.mem_write(work, pack("<I", 0))
        call("wrap", reference, 43, 0, work)
        assert unpack("<I", app.cpu.mem_read(work, 4))[0] < 160, title
        for subject, expected in enumerate(values):
            _, _, unit_ptr, formula, _ = unpack("<5I", app.cpu.mem_read(eq + 32 + variable_size*subject, variable_size))
            if not formula:
                continue
            ok, answer, substituted = solve(eq, subject, values)
            unit = app.c_string(unit_ptr)
            match = re.search(r" = (-?[0-9.e+\-]+)", answer)
            assert ok and match and math.isclose(float(match[1]), expected, rel_tol=2e-9, abs_tol=1e-9), (
                title, subject, answer, expected
            )
            assert answer[match.end():] == (" (no unit)" if unit == "no unit" else " " + unit), answer
            assert "$" not in substituted and "*" not in substituted

    def chemistry_eq(index):
        return chemistry + index*equation_size

    ok, answer, _ = solve(chemistry_eq(19), 0, (0, "0.1 mol/L", "25cm3", "20 mL", 1, 2))
    assert ok and answer == "cA = 0.04 mol/dm^3", answer
    assert "25 cm^3 = 0.025 dm^3" in app.c_string(conversions, 400)
    assert "Mole ratio B:A = 2:1" in app.c_string(steps, 300)
    ok, answer, _ = solve(chemistry_eq(1), 0, (0, 35, 37, "75%"))
    assert ok and answer == "Ar = 35.5 (no unit)"
    ok, answer, _ = solve(chemistry_eq(4), 0, (0, 40, 32, 2, "", ""))
    assert ok and answer == "Mr = 74 (no unit)"
    ok, answer, _ = solve(chemistry_eq(15), 0, (0, 1e20, 1, 1, 1))
    assert ok and answer == "x = 1 mol" and "B limits" in app.c_string(steps, 300)
    print(f"{chemistry_count} Chemistry fact sheets and their solver subjects checked on ARM")

    unit_examples = [
        ("250 mA", "A", .25), ("12 cm2", "m^2", .0012), ("3.5kJ", "J", 3500),
        ("3.5cl", "mL", 35), ("250cm^3", "dm^3", .25), ("(1/4)kg", "g", 250),
        ("0 degC", "K", 273.15), ("273.15K", "degC", 0), ("10 dF", "C or K", 50/9),
        ("36 km/h", "m/s", 10), ("1 atm", "Pa", 101325),
        ("80%", "no unit", .8), ("80%", "%", 80),
        ("15 kg", "V", None),
    ]
    for source, target, expected in unit_examples:
        app.cpu.mem_write(work, source.encode() + b"\0")
        app.cpu.mem_write(work + 0x80, target.encode() + b"\0")
        call("quantity_read", work, work + 0x80, result, substitution, 220)
        ok = bool(app.cpu.reg_read(UC_ARM_REG_R0))
        assert ok == (expected is not None), (source, target)
        if ok:
            actual, = unpack("<d", app.cpu.mem_read(result, 8))
            assert math.isclose(actual, expected, rel_tol=1e-10, abs_tol=1e-10), (source, target, actual)
    print("Linear, square, cubic, percent and temperature conversions checked on ARM")

    app = Simulator(path)
    app.run(app.entry)
    app.press("VAR")
    app.see("CAS and history", "AQA GCSE Physics equation sheet", "AQA GCSE Chemistry equation sheet", "Unit conversions")
    assert app.screen.getpixel((0, 0)) == app.colour(0xfda6)
    app.press("OK")
    app.see("AQA GCSE Physics equation sheet", "Kinetic energy [J]", "Mass [kg]", "Speed [m/s] squared")
    app.screen.save(path.parent / "equation-sheet.png")
    app.press("OK")
    app.see("Kinetic energy", "Topic: Energy", "TOOLBOX: open topic 4.1.1.2",
            "Ek=0.5mv^2", "Symbols and units", "joule", "kilogram", "Solver")
    ink = app.colour(0x2104)
    assert app.screen.getpixel((11, 135)) == ink
    assert app.screen.getpixel((11 + 7*len("Symbols and units") - 1, 135)) == ink
    app.screen.save(path.parent / "physics-symbols-units.png")
    app.press("TOOLBOX")
    app.see("Kinetic, elastic and GPE", "4.1.1.2")
    app.press("BACK")
    app.see("Topic: Energy", "Symbols and units", "Solver")
    headings_seen = set()
    for _ in range(36):
        headings_seen.update(app.strings)
        if "Use: Find Mass given Kinetic energy and" in app.strings:
            break
        app.press("DOWN")
    assert {"Worked example", "When to use it", "Rearrangements"} <= headings_seen
    app.see("m=2Ek/v^2", "Use: Find Mass given")
    app.screen.save(path.parent / "equation-reference.png")
    app.press("EXE")
    app.see("Choose the unknown", "Kinetic energy", "Mass", "Speed")
    app.press("DOWN", "DOWN", "OK")
    app.see("Find Speed [m/s]", "v=sqrt(2Ek/m)", "Kinetic energy", "Mass")
    assert app.screen.getpixel((7, 85)) == app.colour(0xfda6)
    assert all(app.screen.getpixel((x, y)) == (255, 255, 255)
               for y in range(86, 89) for x in range(7, 313))
    app.press("EXE")
    app.see("Check Kinetic energy [J].")
    assert not any(line.startswith("Substitute:") for line in app.strings)
    app.press("9", "DOWN", "2", "EXE")
    app.see("Substitute: v=sqrt(2 x 9/2)", "v = 3 m/s")
    app.screen.save(path.parent / "equation-solver.png")
    app.press("BACK", "UP", "OK")
    app.see("Find Mass [kg]", "m=2Ek/v^2", "9")
    app.press("DOWN", "3", "EXE")
    app.see("Substitute: m=2 x 9/3^2", "m = 2 kg")
    app.press("BACK", "BACK", "BACK", *["DOWN"] * 22)
    app.see("Liquid pressure at depth", "(higher only)")
    app.press("OK")
    app.see("(higher only)", "p=hrhog")
    app.press("TOOLBOX")
    app.see("Pressure at depth", "(higher only)", "4.5.5.1.2")
    app.press("BACK")
    app.see("Liquid pressure at depth", "Solver")
    app.press("BACK", *["UP"] * 22)
    for i, section in enumerate(sections):
        app.press("OK", "TOOLBOX")
        app.see("UP/DOWN: scroll", section)
        app.press("BACK", "BACK")
        if i + 1 < len(sections):
            app.press("DOWN")
    print(f"Checked every physics fact sheet's TOOLBOX shortcut on ARM")
    app.press(*["UP"] * 24, "OK", "EXE", "OK")
    app.see("Find Power [W]", "P=VI")
    app.press("1", "2", "DOWN", "2", "5", "0", "ALPHA", "-", "ALPHA", "7",
              "SHIFT", "ALPHA", "EXP", "EXE")
    app.see("Physics working", "250 mA = 0.25 A", "Substitute: P=12 x 0.25", "P = 3 W")
    app.screen.save(path.parent / "physics-unit-conversion.png")
    app.press(*["BACK"] * 4)
    app.press("BACK", "DOWN", "OK")
    app.see("AQA GCSE Chemistry equation sheet", "Neutron number", "Relative atomic mass")
    app.press(*["DOWN"] * 19, "OK")
    app.see("Titration with mole ratios", "(higher only)", "4.4.2.5")
    app.press("TOOLBOX")
    app.see("Titration calculations", "(higher only)")
    app.press("BACK", "EXE", "OK")
    app.see("Chemistry solver", "Find Concentration A [mol/dm^3]")
    app.press("0", ".", "1", "DOWN", "2", "5", "ALPHA", "-", "ALPHA", "7", "ALPHA", "SQUARE",
              "DOWN", "2", "0", "ALPHA", "-", "ALPHA", "7", "ALPHA", "SQUARE",
              "DOWN", "1", "DOWN", "2", "EXE")
    app.see("Chemistry working", "25 mL = 0.025 dm^3", "Mole ratio B:A = 2:1", "cA = 0.04 mol/dm^3")
    app.screen.save(path.parent / "chemistry-titration.png")
    app.press("BACK", "BACK", "BACK", "BACK", "BACK", "DOWN", "OK")
    app.see("Unit conversions", "Length", "Area", "Volume")
    app.press("DOWN", "DOWN", "OK", *["DOWN"] * 6, "OK", *["DOWN"] * 5, "OK")
    app.see("cL to mL")
    app.press("3", ".", "5", "EXE", ("SHIFT", "VAR"))
    app.see("35 mL", "Number copied to NumAI clipboard")
    app.screen.save(path.parent / "unit-converter.png")
    app.press(*["BACK"] * 6, ("SHIFT", "TOOLBOX"))
    app.see("NUMAI", "35")
    app.press("HOME")
    assert app.exited


def check_chemistry_links(path):
    app = Simulator(path)
    app.run(app.entry)
    app.press("VAR", "DOWN", "OK")
    destinations = [
        ("4.1.1.5", "Atomic size, mass and isotopes"),
        ("4.1.1.6", "Relative atomic mass"),
        ("4.1.1.6", "Relative atomic mass"),
        ("4.3.1.2", "Relative formula mass"),
        ("4.3.1.2", "Relative formula mass"),
        ("4.3.1.2", "Relative formula mass"),
        ("4.3.1.1", "Mass conservation and balancing"),
        ("4.3.1.4", "Chemical measurement uncertainty"),
        ("4.3.2.5", "Concentration in g/dm^3"),
        ("4.3.3.1", "Percentage yield"),
        ("4.3.3.2", "Atom economy"),
        ("4.3.2.1", "Moles"),
        ("4.3.2.1", "Moles"),
        ("4.3.2.2", "Amounts in equations"),
        ("4.3.2.3", "Balancing using moles"),
        ("4.3.2.4", "Limiting reactants"),
        ("4.3.3.1", "Theoretical product mass"),
        ("4.3.4", "Concentration in mol/dm^3"),
        ("4.3.4", "Concentration in mol/dm^3"),
        ("4.4.2.5", "Titration calculations"),
        ("4.3.5", "Molar gas volume"),
        ("4.3.5", "Molar gas volume"),
        ("4.6.1.1", "Reaction rates and graphs"),
        ("4.6.1.1", "Reaction rates and graphs"),
        ("4.6.1.1", "Instantaneous rate in mol/s"),
        ("4.6.1.1", "Instantaneous rate in mol/s"),
        ("4.8.1.3", "Chromatography and Rf"),
        ("4.5.1.3", "Bond energy calculations"),
        ("4.4.2.6", "Strong and weak acids"),
    ]
    count, = unpack("<I", app.cpu.mem_read(app.symbol["chemistry_equation_count"], 4))
    assert len(destinations) == count
    for i, (section, title) in enumerate(destinations):
        app.press("OK")
        app.see(f"TOOLBOX: open topic {section}", "Solver")
        app.press("DOWN", "DOWN")
        fact_sheet = app.strings.copy()
        app.press("TOOLBOX")
        app.see(section, title, "UP/DOWN: scroll")
        app.press("BACK")
        assert app.strings == fact_sheet
        app.press("BACK")
        if i + 1 < count:
            app.press("DOWN")
    app.press("HOME")
    assert app.exited
    print(f"Checked all {count} Chemistry fact-sheet shortcuts and BACK navigation on ARM")


def check_new_calculation(path):
    app = Simulator(path)
    app.run(app.entry)
    app.press("1", "/", "3", "EXE")
    app.see("0.33333", "\x1f")
    app.press("UP", "LEFT", "OK")
    assert app.c_string(app.symbol["prompt"]) == "1/3"
    app.press("BACK", "UP", "RIGHT", "OK")
    app.see("ADDITIONAL RESULTS", "Scientific notation", "Mixed fraction", "Euclidean division")
    app.screen.save(path.parent / "calc-fraction-menu.png")
    app.press("OK")
    assert app.c_string(app.symbol["prompt"]) == "3.33333*10^(-1)"
    app.press("BACK", "SQRT", "8", ")", "EXE")
    app.see("2.82843", "\x1f")
    app.press("UP", "LEFT", "OK")
    assert app.c_string(app.symbol["prompt"]) == "2*sqrt(2)"
    app.press("BACK", "UP", "RIGHT", "OK")
    app.see("ADDITIONAL RESULTS", "(8, 2.828)", "Curve equation")
    app.screen.save(path.parent / "calc-root-graph.png")
    app.press("BACK", "DOWN", "8", "4", "EXE", "UP", "RIGHT", "OK")
    app.see("0x54", "0b1010100")
    app.press(*["DOWN"] * 4)
    app.see("Prime factors")
    app.press("OK")
    assert app.c_string(app.symbol["prompt"]) == "2^2*3*7"
    app.press("BACK", ("SHIFT", "EXE"), *["DOWN"] * 5, "OK", *["DOWN"] * 5, "OK")
    app.see("Decimal places", "10")
    app.press("BACK", "1", "/", "3", "EXE")
    app.see("0.3333333333")
    app.press(("SHIFT", "EXE"), "OK", "BACK", "SIN", "PI", "/", "6", ")", "EXE")
    app.see("rad", "0.5", "=")
    app.press("UP", "RIGHT", "OK")
    app.see("ADDITIONAL RESULTS", "Unit circle")
    app.screen.save(path.parent / "calc-trig-extras.png")
    app.press("DOWN", "DOWN", "DOWN", "DOWN")
    app.see("tan")
    app.press("OK")
    assert app.c_string(app.symbol["prompt"]) == "(sqrt(3))/3"
    app.press("HOME")
    assert app.exited
    print("Structured calculation history, decimal setting and contextual results checked on ARM")


def check_new_tools(path):
    app = Simulator(path)
    app.run(app.entry)
    app.press("VAR")
    assert "CAS AND HISTORY" in app.strings
    assert app.screen.getpixel((319, 1)) == app.colour(0xfda6)
    app.press(*["DOWN"] * 15, "OK", "EXE")
    app.see("Trigonometric equation solver", "theta = 30", "150 by symmetry", "x = 30, 150 deg")
    app.press("BACK", "BACK", "DOWN", "OK", "7", "DOWN", "1", "0", "DOWN", "DOWN", "3", "0", "EXE")
    app.see("Triangle 1", "Triangle 2", "supplement")
    app.press("BACK", "BACK", "DOWN", "OK", "EXE")
    app.see("Physics formula rearranger", "Subtract", "a = (v^2 - u^2)/(2*s)")
    app.press("BACK", "BACK", "BACK", "TOOLBOX")
    assert "TOOLBOX" in app.strings
    app.press("OK", "OK", ("SHIFT","VAR"))
    app.see("Pinned in TOOLBOX")
    app.press("BACK", "BACK", "BACK", "VAR", "OK", "OK", ("SHIFT","VAR"))
    app.see("Pinned in TOOLBOX")
    app.press("BACK", "BACK", "DOWN", "OK", "OK", ("SHIFT","VAR"))
    app.see("Pinned in TOOLBOX")
    app.press("BACK", "BACK", "BACK", "TOOLBOX", *["DOWN"] * 6, "OK")
    app.see("FAVOURITES", "Integers, primes and HCF", "Kinetic energy", "Neutron number")
    app.press("DOWN", "OK")
    app.see("Topic: Energy", "TOOLBOX: open topic 4.1.1.2", "Solver")
    app.press("BACK", ("SHIFT","VAR"))
    app.see("Removed from Favourites", "Neutron number")
    assert "Kinetic energy" not in app.strings
    app.press("OK", "BACK")
    app.see("FAVOURITES", "Neutron number")
    app.press("HOME")
    assert app.exited

    app = Simulator(path)
    app.run(app.entry)
    scratch, result, stack = 0x20035000, 0x20036500, STACK - 0x1000

    def invoke(function, args):
        for reg, value in zip(REGS,args):
            app.cpu.reg_write(reg,value)
        app.cpu.mem_write(stack,pack(f"<{len(args[4:])}I",*args[4:]))
        app.cpu.reg_write(UC_ARM_REG_SP,stack)
        app.cpu.reg_write(UC_ARM_REG_LR,STOP | 1)
        app.exited = False
        app.run(app.symbol[function] | 1)

    def write_text(address, value):
        app.cpu.mem_write(address,value.encode()+b"\0")
        return address

    for expression, lo, hi, deg, expected in [
        ("cos(2x)=0", "0", "360", 1, "45, 135, 225, 315"),
        ("sin(x)^2=1/4", "0", "360", 1, "30, 150, 210, 330"),
        ("tan(-x)=1", "-pi", "pi", 0, "-0.7853981634, 2.35619449"),
        ("sin(x)=2", "-100", "100", 1, "No solutions"),
        ("cos(x)=1", "0", "360", 1, "0, 360"),
        ("sin(x)=cos(x)", "0", "360", 1, "45, 225"),
    ]:
        invoke("cas_trig_equation",[write_text(scratch,expression),write_text(scratch+128,lo),
                                    write_text(scratch+256,hi),deg,result])
        assert expected in app.c_string(result+512,512), (expression,app.c_string(result+512,512))
    for expression,subject,expected in [
        ("v^2=u^2+2as","a","a = (v^2 - u^2)/(2*s)"),
        ("F=kx+cx","x","x = F/(k + c)"),
        ("W=mg","m","m = W/g"),
        ("E=mv^2/2","v","v = +/-sqrt(2*E/m)"),
    ]:
        invoke("rearrange_physics",[write_text(scratch,expression),write_text(scratch+128,subject),result])
        assert expected in app.c_string(result+512,512), (expression,app.c_string(result+512,512))
    for expression,expected,n,d,r in [
        ("1/3","1/3",1,3,1),("sqrt(8)","2*sqrt(2)",2,1,2),("sqrt(1/8)","(sqrt(2))/4",1,4,2),
        ("sqrt(3)/sqrt(6)","(sqrt(2))/2",1,2,2),("sin(pi/6)","1/2",1,2,1),
        ("0b1010100","84",84,1,1),("0x54","84",84,1,1),
    ]:
        invoke("cas_calculation",[write_text(scratch,expression),0,result])
        assert app.c_string(result+43)==expected, (expression,app.c_string(result+43))
        assert unpack("<3i",app.cpu.mem_read(result+16,12))==(n,d,r)
    print("Trig branches, two-triangle SSA, physics isolation and favourites checked on ARM")


if __name__ == "__main__":
    target = Path(sys.argv[1])
    output = target.parent
    check_ui(target, output)
    check_keyboard(target, output)
    check_engine(target)
    check_new_calculation(target)
    check_new_tools(target)
    check_physics(target)
    check_chemistry_links(target)
    check_catalog(target)
