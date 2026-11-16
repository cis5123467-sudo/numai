"""Run the loader-linked NWA on a Cortex-M7 with simulated EADK inputs/display."""
from collections import deque
from pathlib import Path
import math
import re
from struct import pack, unpack
import sys

from elftools.elf.elffile import ELFFile
from PIL import Image, ImageDraw, ImageFont
from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_MCLASS, UC_MODE_THUMB
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
        self.font = ImageFont.load_default(size=11)
        self.strings = []
        self.waiting = False
        self.exited = False
        for name, handler in (
            ("eadk_keyboard_scan", self.scan), ("eadk_timing_msleep", self.sleep),
            ("eadk_display_push_rect_uniform", self.rect), ("eadk_display_draw_string", self.text),
        ):
            address = self.symbol[name] & ~1
            self.cpu.hook_add(UC_HOOK_CODE, self.stub, handler, begin=address, end=address)
        self.cpu.hook_add(UC_HOOK_CODE, self.exit, begin=STOP, end=STOP)

    def stub(self, cpu, address, size, handler):
        a, b, c, d = (cpu.reg_read(reg) for reg in REGS)
        handler(a, b, c, d)
        cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))

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

    def text(self, address, point, large, ink):
        text = self.c_string(address)
        x, y = point & 65535, point >> 16
        paper, = unpack("<I", self.cpu.mem_read(self.cpu.reg_read(UC_ARM_REG_SP), 4))
        size = 10 if large else 7
        assert x + len(text)*size <= 320 and y + 14 <= 240, (x, y, text)
        self.strings.append(text)
        for i, char in enumerate(text):
            left = x + size*i
            self.draw.rectangle((left, y, left + size - 1, y + 13), fill=self.colour(paper))
            self.draw.text((left, y + 1), char, font=self.font, fill=self.colour(ink))

    def exit(self, cpu, *_):
        self.exited = True
        cpu.emu_stop()

    def run(self, address=None):
        self.waiting = False
        pc = self.cpu.reg_read(UC_ARM_REG_PC) | 1 if address is None else address
        self.cpu.emu_start(pc, 0, count=18_000_000)
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
            assert phrase in screen, f"{phrase!r} missing from screen:\n{screen}"


def check_ui(path, screenshots):
    app = Simulator(path)
    app.run(app.entry)
    app.see("NumAI", "UK DEG", "TOOLBOX", "VAR")
    app.screen.save(screenshots / "startup.png")
    app.press(("ALPHA", "XNT"))
    app.see("NumAI", "UK DEG")
    app.press(("SHIFT", "EXE"))
    app.see("Settings", "Degrees")
    app.press("BACK", "SIN", "3", "0", ")", "EXE")
    app.see("sin(30)", "1/2")
    app.press("BACK")
    app.press(("SHIFT", "EXE"), "OK", "BACK")
    app.see("NumAI", "UK RAD")
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
    app.press("VAR", "DOWN", "OK", "EXE")
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
    app.press("BACK", "BACK", "DOWN", "DOWN", "OK", "DOWN", "DOWN", "OK")
    app.see("Substitute x", "Substitute x = 2")
    app.press(("SHIFT", "EXE"))
    app.see("Settings", "Radians", "Fractions", "Worked steps")
    app.press("OK", "DOWN", "OK", "DOWN", "OK", "BACK")
    app.see("Substitute x", "5")
    assert "Substitute x = 2" not in app.strings
    app.press("SHIFT", "EXE")
    app.see("Settings", "Degrees", "Decimal", "Off")
    app.press("BACK", "BACK", "BACK", "BACK")
    app.see("NumAI")
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
    app.see("asin(0.5)", "30")
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
    app.see("A+1", "13")
    app.press("BACK", "2", "5", "ALPHA", "DEL", "EXE")
    app.see("25%", "1/4")
    app.press("BACK", "1", ("SHIFT", "SQRT"), "2", "EXE")
    app.see("1<2", "True")
    app.press("BACK", "BACK")
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
    work, result, stack = 0x20035000, 0x20035500, STACK - 0x1000

    def solve(eq, subject, data):
        addresses = []
        for i, value in enumerate(data):
            address = work + 0x20 + 32*i
            app.cpu.mem_write(address, str(value).encode() + b"\0")
            addresses.append(0 if i == subject else address)
        app.cpu.mem_write(work, pack("<4I", *addresses, *[0]*(4-len(addresses))))
        for reg, value in zip(REGS, (eq, subject, work, result)):
            app.cpu.reg_write(reg, value)
        app.cpu.mem_write(stack, pack("<I", 112))
        app.cpu.reg_write(UC_ARM_REG_SP, stack)
        app.cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        app.exited = False
        app.run(app.symbol["physics_solve"] | 1)
        return bool(app.cpu.reg_read(UC_ARM_REG_R0)), app.c_string(result)

    tiers = []
    for i, data in enumerate(groups):
        eq = sheet + 112*i
        title, _, _, _, _, _, higher, terms = unpack("<8I", app.cpu.mem_read(eq, 32))
        assert terms == len(data)
        if higher:
            tiers.append(i + 1)
        for subject, expected in enumerate(data):
            ok, answer = solve(eq, subject, data)
            assert ok, (app.c_string(title), subject, answer)
            match = re.search(r" = (?:\+/- )?(-?[0-9.e+\-]+)", answer)
            assert match and math.isclose(float(match[1]), expected, rel_tol=2e-9, abs_tol=1e-9), (
                app.c_string(title), expected, answer
            )
    assert tiers == [23, 28, 29, 33, 34, 35]
    assert not solve(sheet + 7*112, 0, (0, 60, 45))[0]
    assert not solve(sheet + 4*112, 0, (0, 10, 0))[0]
    assert not solve(sheet + 25*112, 0, (0, 1, -2, 2))[0]
    print(f"{count} physics references and {sum(map(len, groups))} rearrangements checked on ARM")

    app = Simulator(path)
    app.run(app.entry)
    app.press("VAR", *["DOWN"] * 11, "OK")
    app.see("Physics equation sheet", "Kinetic energy", "mass x speed", "squared")
    app.screen.save(path.parent / "equation-sheet.png")
    app.press("OK")
    app.see("Kinetic energy", "Ek = 0.5*m*v^2", "Symbols and units", "Solver")
    app.press(*["DOWN"] * 9)
    app.see("Rearrangements", "m = 2*Ek/v^2", "Worked example")
    app.screen.save(path.parent / "equation-reference.png")
    app.press("EXE")
    app.see("Choose the unknown", "Kinetic energy", "Mass", "Speed")
    app.press("DOWN", "DOWN", "OK")
    app.see("Find Speed [m/s]", "Kinetic energy", "Mass")
    app.press("9", "DOWN", "2", "EXE")
    app.see("v = 3 m/s")
    app.screen.save(path.parent / "equation-solver.png")
    app.press("BACK", "UP", "OK")
    app.see("Find Mass [kg]", "9")
    app.press("DOWN", "3", "EXE")
    app.see("m = 2 kg")
    app.press("BACK", "BACK", "BACK", *["DOWN"] * 22)
    app.see("Liquid pressure at depth", "(higher only)")
    app.press("OK")
    app.see("(higher only)", "p = h*rho*g")
    app.press("HOME")
    assert app.exited


if __name__ == "__main__":
    target = Path(sys.argv[1])
    output = target.parent
    check_ui(target, output)
    check_keyboard(target, output)
    check_engine(target)
    check_physics(target)
    check_catalog(target)
