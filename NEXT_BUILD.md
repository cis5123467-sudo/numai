# NumAI next-build queue

## Included in v0.1.2

- Make equation and solver calculations reusable from History. Selecting a saved calculation should offer reopening the original tool with its previous equation, chosen unknown, input values, and units filled in, so users can edit values and calculate again. Preserve the details in saved sessions so reuse works after leaving and reopening NumAI.
- Add search to Toolbox / Courses so users can find topics across the courses and open a matching topic directly.
- Add equation search so users can find equations and open their fact sheets directly.
- On each course topic screen, show related equations immediately below the specification number, above the topic notes. Pressing Var from that topic opens its related equations. If there are several, offer a list from which to select the fact sheet.
- Expand every AQA Physics, Biology, and Chemistry topic with more detailed explanations. Use bold text for important words and key terms, with clear headings and readable formatting. Keep Higher-only labels visible where applicable.
- Add an app-wide Alpha + ANS shortcut that immediately returns to NumAI's calculation home screen from any page. Preserve the active session and its saved work, and include the shortcut in Keyboard Shortcuts.

## v0.1.1 build requirements

The following requirements were used for the v0.1.1 build and are retained as its reference checklist.

### Persistent calculation sessions

- Use KhiCAS on NumWorks as the model for named calculation sessions. Store each session in a calculator Python script named `session_<name>.py`, such as `session_algebra.py`, so it appears in the Python app's script list.
- Put **New Session** and **Open Session** in the Shift + EXE menu. Keep each session's entries separate.
- Always reopen the most recently active session on launch, with its prior calculations available through the calculation screen's history controls.
- Save automatically as soon as a calculation finishes, an equation tool or CAS operation runs, or a preference changes. Save expressions, inputs, and displayed results to the active session as applicable, and persist preference changes immediately.
- Preserve existing Python scripts when creating or saving sessions. Verify on the N0120 running Epsilon 24.8.0 that two sessions appear in Python, retain separate histories after leaving and reopening NumAI, and can be switched without losing calculations.

### Var menu

- Title the menu **Equations and CAS**.
- Put **Favourites** first. Group all physics and chemistry tools directly below it, including both AQA equation sheets, unit conversions, and the physics formula rearranger. Put the remaining maths and CAS tools below them.
- Move the CAS and equation history entry to the Shift + EXE menu.
- Remove the separate **Expand** entry and keep **Simplify** available for expressions that need expanding.

### Shift + EXE menu

Open the general NumAI menu with Shift + EXE. Show these entries in this order:

1. **Courses** — open the same course browser as Toolbox.
2. **Equations and CAS** — open the same tools menu as Var.
3. **New Session** — create and switch to a new named session.
4. **Open Session** — browse and resume a saved session.
5. **History** — show the active session's previous CAS operations and equation-tool uses.
6. **Keyboard Shortcuts** — show the existing shortcuts table.
7. **Preferences** — open the current settings.
8. **About** — show NumAI's app information.

### Maths input and calculation

- Use the main calculation screen's maths input formatting across CAS, equation tools, unit conversions, and solvers. Expression fields should display and edit powers, fractions, roots, and brackets consistently.
- Support postfix factorials in calculations, including `5! = 120` and `5!/2! = 60`. Check that factorial binds before division and works with parenthesised operands.
- On EXE, append the closing parentheses needed to complete a calculation with unfinished brackets. For example, `2*(3+4` becomes `2*(3+4)` before evaluation. Preserve balanced input and report syntax errors for misplaced closing brackets.

### Formulas in revision content

- Use the calculation screen's typesetting for mathematical expressions throughout course topic text, worked examples, equation sheets, and formula displays. Render exponents as superscripts and division as a stacked fraction with a horizontal bar.
- Render physics quantity labels with their intended subscripts, such as `Eₖ` for kinetic energy. Use the same notation in symbol and unit lists, rearrangements, worked examples, and solver displays.
- Keep the display formatting compatible with equation parsing, solving, and copying. Check that tall fractions, powers, and subscripts fit and scroll correctly on the calculator screen.

### Calculator layout and menu indicators

- Increase the height and text size of each calculation row to match the stock NumWorks Calculation app. Check that expressions, results, and selection fit their rows, including stacked fractions.
- Show the active Shift or Alpha indicator in the top header bar while navigating menus. Move it from the bottom-left footer.

### Favourites

- Fix Shift + Var so it toggles the favourite for the selected equation in either the AQA Physics or AQA Chemistry equation sheet.
- Put **Favourites** first in Courses and keep that list for course topics. Put **Favourites** first in Equations and CAS and use it to access the separate physics and chemistry equation lists.
- Put **Favourite Equations** first within each equation sheet. The Physics sheet shows only saved physics equations, and the Chemistry sheet shows only saved chemistry equations.
