# NumAI for NumWorks

Revision notes, equation sheets, worked examples, scientific solvers and a CAS calculator in a native **320 × 240 NumWorks external app**. This is the **original NumAI project**; [NumAI for Casio](https://github.com/cis5123467-sudo/numai-casio) is its related Graph Math+ port.

Current release: **v0.1.3-r1**. Target: **NumWorks N0120 running Epsilon 24.8.0**.

[Download the latest release](https://github.com/cis5123467-sudo/numai/releases/latest) · [Download v0.1.3-r1](https://github.com/cis5123467-sudo/numai/releases/download/0.1.3-r1/numai-0.1.3-r1.nwa) · [Detailed release notes](https://github.com/cis5123467-sudo/numai/releases/tag/0.1.3-r1) · [Report an issue](https://github.com/cis5123467-sudo/numai/issues)

## Install

1. Download **`numai-0.1.3-r1.nwa`** from GitHub Releases. Choose the `.nwa` asset rather than GitHub's automatic source-code archive.
2. Connect the calculator by USB and open the [NumWorks external-app installer](https://my.numworks.com/apps) using your existing supported installation setup and a WebUSB-capable browser, such as desktop Chrome or Edge.
3. Select the calculator, add the `.nwa` file and complete the installation.
4. Open **NumAI** from the calculator's application menu. It starts directly in **CALCULATION**; use **TOOLBOX** for courses or **VAR** for equations and CAS.

The icon, font, course notes and equation data are bundled into the app. No separate content files or online account are needed to use the installed application.

Back up existing Python scripts, including NumAI session records, before replacing an installation or resetting the calculator. Persistent sessions use the N0120 Epsilon **24.8.0** Python-store layout. Other hardware/firmware combinations are not established targets; if the app cannot recognise a supported store, persistent saving is unavailable. A hardware reset can clear Epsilon's RAM-backed scripts.

## Revision and solving

- **Three maths courses** with notes, methods, worked examples and calculator forms: Edexcel IGCSE Mathematics A, OCR Level 3 Additional Mathematics and Edexcel A level Mathematics.
- **Separate AQA GCSE Physics, Biology and Chemistry**, organised by topic, with specification references, supporting explanations, bold terminology and Higher-only labels.
- **Courses search** across course names, specification references, topic titles and note content. **Equation search** across names, symbols, units, topics and references.
- **AQA GCSE Physics and Chemistry equation sheets** with word and symbolic equations, variable definitions, unit names, worked examples, use cases, rearrangements and subject-selecting solvers.
- **Two-way topic links:** press VAR on supported course notes for related equations; press TOOLBOX on an equation fact sheet to open its course topic.
- **Unit conversions** for everyday and scientific quantities, with square/cubic unit formatting and copyable results.
- **Trigonometric equation solver, triangle solver and Physics formula rearranger**, alongside factorisation, simplification, solving, differentiation, integration and completing the square.
- **Exact fractions and roots where supported**, decimal approximations, additional answer formats, factorials, stored variables, Ans and automatic completion of missing final parentheses.
- **Named calculation sessions**, autosave, reusable solver History, course favourites and separate Physics/Chemistry equation favourites.

### Courses and notes

The Courses menu begins with **Favourites**, then **Search topics**, then the six courses. The maths catalogue contains **112 topics**, **224 additional worked examples** and **58 calculator examples**. Depending on the topic, opening it offers course notes, method and working, an input form and worked examples.

| Course | Main coverage |
| --- | --- |
| Edexcel IGCSE Mathematics A | Number, algebra, graphs, geometry, trigonometry, measures, probability and statistics |
| OCR Level 3 Additional Mathematics | Algebra, coordinate geometry, trigonometry, calculus and numerical applications |
| Edexcel A level Mathematics | Pure mathematics, statistics and mechanics |
| AQA GCSE Physics — 8463 | Energy, electricity, particle model, atomic structure, forces, waves, magnetism/electromagnetism and space physics |
| AQA GCSE Biology — 8461 | Cell biology, organisation, infection/response, bioenergetics, homeostasis/response, inheritance/variation/evolution and ecology |
| AQA GCSE Chemistry — 8462 | Atomic structure, bonding, quantitative chemistry, chemical changes, energy changes, rates/equilibrium, organic chemistry, analysis, atmosphere and resources |

Science notes include practical and scientific-skills material. **(higher only)** identifies Higher-tier entries; mixed-tier notes identify relevant Higher-only material within the text. The science course is the full separate-science course, so separate-only content remains available even where revision priorities focus on combined/Trilogy topics.

### Physics Paper 1 revision in v0.1.3-r1

Open **TOOLBOX → AQA Physics**, then choose **Energy**, **Electricity**, **Particle model of matter** or **Atomic structure**. Each chapter starts with a refreshed **Key Ideas** page. The detailed notes lead with the relevant Combined Science: Trilogy material and explain equation choice, substitutions, units and common exam mistakes. Higher-only and separate Physics content is identified.

There are also four original **6-mark practice questions with bullet-point model answers**, under **Kinetic, elastic and GPE**, **The National Grid**, **Specific latent heat** and **Contamination and irradiation**.

### Physics expansion in v0.1.3

All **116 existing Physics entries** were rewritten and expanded, with **eight new first-entry Key Ideas pages**—one for each main chapter. There are **124 Physics-specific entries**, plus five shared science-skills entries exposed through the course.

The expansion gives particular attention to combined/Trilogy Paper 1 revision:

- **Energy:** gravitational, kinetic and elastic energy; squared-variable relationships; conservation, dissipation, efficiency, power and specific heat capacity.
- **Electricity:** charge/current, potential difference, resistance, I–V characteristics, lamps, diodes, thermistors, LDRs, series/parallel circuits, electrical power/energy, mains safety and the National Grid.
- **Particle model:** density and cubic conversions; particle states; evaporation and boiling; internal energy versus temperature; heating curves; specific latent heat; gas collisions and pressure.
- **Atomic structure:** scattering evidence, isotopes, nuclear equations, alpha/beta/gamma comparisons, ionisation, penetration, contamination, irradiation, background correction and half-life.

Each chapter's Key Ideas provides a quick recall route before the detailed pages. The notes include original worked examples, substitutions and units, graph interpretation, practical methods with reasons, controlled variables, errors and common exam mistakes. Forces, Waves, Magnetism and Space also have their own expanded explanations and examples.

Transformer notes distinguish the combined Higher ideal-power relation **VₚIₚ = VₛIₛ** from the separate-Physics Higher turns-ratio relation **Vₚ/Vₛ = nₚ/nₛ**.

### Chemistry Topic 4: Chemical Changes

Chemical Changes has **Key Ideas first**, followed by the **17 detailed topic pages**. The summary includes the requested quick facts and the four acid reaction patterns:

| Reactants | Products |
| --- | --- |
| Acid + metal | Salt + hydrogen |
| Acid + metal oxide | Salt + water |
| Acid + metal hydroxide | Salt + water |
| Acid + metal carbonate | Salt + water + carbon dioxide |

Detailed notes cover the reactivity series, water and acid reactions, displacement, extraction, oxidation/reduction, salt naming, neutralisation, pH, strong/weak acids, soluble-salt preparation, titration, molten/aqueous electrolysis and aluminium extraction. Worked calculations, balanced examples, gas tests, practical reasoning and Higher-only distinctions are included.

## Calculate and use CAS

Type an expression on the calculation screen and press **EXE**. Examples include `1/3`, `sqrt(8)`, `sin(30)` in degrees, `5!`, and `5!/2!`. Missing final closing parentheses are completed when evaluating: `2*(3+4` is evaluated as `2*(3+4)`.

Supported exact answers can appear alongside a grey decimal approximation: for example, `sqrt(8)` can display **2√2 ≈ 2.82843**. Precision defaults to **five decimal places**, with **0–10** selectable in Preferences. Display precision is separate from the numerical precision used internally.

Press **UP** to select previous calculation rows, then **LEFT/RIGHT** to select the input, an available result, or the **three-dot additional-results button**. **OK** on a selected input/result inserts it into the editor. The additional-results menu depends on the calculation; supported cases include scientific notation, mixed fractions, integer representations/prime factors and root or trigonometric information.

The calculation screen uses taller mathematical rows and the full available lower display area without the routine shortcut tooltip bar. Exceptional status messages, such as an unsaved-session warning, remain visible.

### Equations and CAS menu

Press **VAR** to open **EQUATIONS AND CAS**. Its leading entries are Favourites, the Physics equation sheet, the Chemistry equation sheet, Unit conversions and the Physics formula rearranger. Maths/CAS tools follow, including:

| Tool | Purpose |
| --- | --- |
| Calculate | Evaluate a supported numerical expression |
| Factorise | Factor a supported expression in x |
| Simplify | Simplify supported algebra, including supported expansion |
| Solve for x | Solve a supported equation in x |
| Differentiate | Find a symbolic derivative for supported expressions |
| Integrate | Find an indefinite integral for supported expressions |
| Definite integral | Evaluate a supported polynomial integral between two limits |
| Substitute x | Evaluate at a supplied x-value |
| Root in an interval | Find a numerical root within supplied bounds |
| Complete the square | Rewrite a supported quadratic in completed-square form |
| Trigonometric equation solver | Find supported trigonometric solutions within specified bounds |
| Triangle solver | Solve supported combinations of known sides and angles |

The separate **Expand** menu item is omitted; use **Simplify** for supported expansion. The CAS is a bounded implementation centred on the symbolic variable **x**, with a general polynomial degree limit of **12**. Indefinite integration supports polynomials and selected elementary functions; definite integration currently accepts polynomials. The trig solver and formula rearranger handle supported forms rather than unrestricted symbolic problems. Fractions/roots are exact only when the engine can represent the result in a supported exact form.

### Prefix shortcuts and variables

These commands can be entered directly in calculation input:

| Prefix | Operation | Example |
| --- | --- | --- |
| `>*` | Factorise | `>*{x^2-5x+6}` |
| `>/` | Simplify | `>/{3x+2(x-1)}` |
| `=>` | Solve for x | `=>{2x+4=0}` |
| `>**` | Complete the square | `>**{2x^2+12x+11}` |

Store a numerical value using the arrow, for example **`12->A`**, then reuse it with **`A+1`**. **ANS** inserts the previous value. The input limit is **111 characters** per expression field.

## Equation sheets, conversions and solvers

### Physics and Chemistry fact sheets

Open either sheet through VAR. Select an equation and press **OK/EXE** to read its fact sheet. The Physics sheet contains **35 equations**; the Chemistry sheet contains **29**.

Fact sheets provide the word and symbolic equation, topic/reference, variable meanings and units, worked example, when to use it and available rearrangements. Physics headings such as **Worked example**, **When to use it**, **Symbols and units** and **Rearrangements** receive emphasis. Physics quantity indices, such as the k in Eₖ, use subscripts where supported.

Choose **Solver**, select the unknown, and enter the remaining values. The working can show the rearranged equation for that subject, substituted values, compatible unit conversions and the answer with units. For example, the energy relation Eₖ = ½mv² can find energy from mass/speed or speed from energy/mass.

Supported quantity fields accept compatible unit suffixes: for example **`250mA`** where current is expected in amperes, or **`25cm^3`** where a compatible volume is expected. Conversion depends on the selected quantity; an unrelated unit is not accepted as interchangeable.

Press **TOOLBOX** from a fact sheet to open its associated course topic. In supported topic notes, related equations appear below the specification reference; **VAR** opens the relevant equation or a choice of related equations.

### Unit conversions

Use **VAR → Unit conversions**, choose the quantity, source unit and destination unit, enter a number or expression, and press **EXE**. Conversion working and a result are shown. **SHIFT + VAR** copies the numerical answer; **SHIFT + TOOLBOX** pastes NumAI's clipboard into an input.

Supported groups include length, area, volume, mass, time, speed, acceleration/field strength, pressure, energy, temperature and temperature changes, current, voltage, charge, resistance, power, force, frequency, density, amount of substance, molar mass, concentrations, energy per mole, reaction rates, magnetic flux density, moments, spring constants, momentum, specific latent heat, specific heat capacity, pressure–volume products, ratios/percentages and angles.

Examples include **cm³ → dm³**, **cL → mL**, **g/cm³ → kg/m³**, **kWh → J** and **°C → K**. Absolute-temperature conversions use offsets; temperature-change conversions are a separate group. Unit selectors and working render powers as superscripts, while compound units retain slash separators.

### Additional solvers

- **Trigonometric equation solver:** choose suitable interval bounds and angle mode. Supported forms can return multiple branches rather than only the inverse-function principal answer.
- **Triangle solver:** enter the available sides/angles. It checks supported combinations and can report both solutions in the ambiguous side–side–angle case.
- **Physics formula rearranger:** isolate a chosen subject in supported Physics equation forms, with rearrangement steps. Its scope is Physics formulas, not arbitrary symbolic algebra in any number of variables.

## NumWorks controls

| Physical key or action | NumAI action |
| --- | --- |
| **TOOLBOX** | Open Courses |
| **TOOLBOX again in Courses** | Search topics; also available directly below Favourites |
| **VAR** | Open Equations and CAS |
| **VAR again in equation menus** | Search equations |
| **VAR on supported topic notes** | Open related equations |
| **TOOLBOX on a fact sheet** | Open its related course topic |
| **SHIFT + EXE** | Open/close the general NumAI menu |
| **OK / EXE** | Open the selection or submit a calculation |
| **UP / DOWN** | Navigate lists, scroll notes or change fields |
| **UP from calculation input** | Select a previous calculation |
| **LEFT / RIGHT on a calculation row** | Select input, available answer or additional-results button |
| **OK on a selected answer** | Insert the result into calculation input |
| **OK on the three dots** | Open contextual additional results |
| **OK on reusable History working** | Reopen the original tool with saved inputs |
| **ALPHA** | Enter a letter |
| **ALPHA twice** | Lock letter entry |
| **SHIFT with ALPHA** | Enter capital letters |
| **ALPHA + decimal point** | Enter factorial `!` |
| **x,n,t** | Enter x |
| **SHIFT + pi** | Enter `=` |
| **SHIFT + square / square root** | Enter `>` / `<` |
| **SHIFT + sin / cos / tan** | Enter inverse trigonometric functions |
| **SHIFT + arrows** | Select text |
| **SHIFT + x,n,t** | Cut selected text |
| **SHIFT + VAR** | Copy selected text, favourite a topic/equation, or copy a supported result, depending on the page |
| **SHIFT + TOOLBOX** | Paste NumAI's clipboard |
| **SHIFT + DEL** | Clear text to the right, or clear the field when at its end |
| **SHIFT + power** | Insert the value-store arrow `->` |
| **ANS** | Insert the previous value |
| **ALPHA + ANS** | Return to calculation home from any page |
| **BACK / HOME** | Return to the previous page / exit to the calculator launcher |

Shift/Alpha status appears in the header. The clipboard belongs to NumAI and is not a shared clipboard for other calculator apps. Angle mode defaults to **degrees**; radians are available in Preferences.

### General menu and preferences

**SHIFT + EXE** opens these entries in order:

1. **Courses**
2. **Equations and CAS**
3. **New Session**
4. **Open Session**
5. **History**
6. **Keyboard Shortcuts**
7. **Preferences**
8. **About**

Preferences contain angle units, numeric answer style, worked-step visibility, CAS help and decimal places. Keyboard Shortcuts presents the key/action guide in a table.

### Favourites

Use **SHIFT + VAR** on a selected topic or Physics/Chemistry equation to toggle its favourite. Courses has its own Favourites entry. Equations and CAS provides access to separate Physics and Chemistry lists; each equation sheet also starts with its own Favourite Equations entry. A Chemistry favourite does not appear in the Physics list.

Favourites are held for the current run; they are not persisted with named sessions in this version.

## Display and saved work

NumAI uses a bundled readable font, centred uppercase titles and gold menu headers. Mathematical layouts support raised powers, roots, stacked fractions and supported Physics subscripts. The default calculation layout includes exact/approximate answers where available; the app's icon and font data are part of the source.

Named sessions are stored as **`session_<name>.py`** records in Epsilon's Python script store, for example **`session_algebra.py`**. They appear in the Python app and contain NumAI data in Python comments; they are session records rather than Python programs that perform the calculations.

- **Up to four named sessions**, each reserving **10 KB**, subject to free space in the shared script store.
- **Up to 12 calculation rows** and **five CAS/equation History entries** per session, plus stored numeric variables and relevant preferences.
- The most recently active session reopens on launch.
- Calculations, CAS/equation operations and preference changes trigger autosave.
- **New Session** and **Open Session** create or switch between separate saved histories.
- Supported History entries recorded from v0.1.2 onward can reopen the tool with its equation, unknown, inputs, units and angle mode for editing and recalculation.
- Existing v0.1.1 sessions remain readable; older History records can be viewed even if they do not contain reusable inputs.

Existing unrelated Python scripts are preserved. A full store can prevent saving; NumAI displays a status message. Retain and back up the session records to retain work. Hardware resets can clear the underlying RAM-backed store. On unrecognised firmware layouts, calculations can run without persistent session storage.

The Casio port has a different save implementation: it uses files in USB storage instead of writing sessions into the calculator's Python app. Casio-specific keys, fonts, save files and newer port-only content should not be assumed to exist in this NumWorks release.

## Build from source

Required tools: **ARM embedded GCC**, **Node.js/npm** and **Make**. On macOS, the toolchain can be installed with:

```sh
brew install numworks/tap/arm-none-eabi-gcc node
```

Clone and build:

```sh
git clone https://github.com/cis5123467-sudo/numai.git
cd numai
make build
make check
```

`make build` creates **`releases/numai-0.1.3-r1.nwa`** inside the repository. `make check` links a loader-oriented test ELF at **`build/0.1.3-r1/numai.elf`** using nwlink. It is a packaging/linking check, not an automated runtime test.

The Makefile pins **nwlink 1.0.0**, generates its EADK header through nwlink and targets **Cortex-M7**, Thumb instructions and hardware floating point. C99 sources compile with size optimisation and warnings treated as errors. Icon conversion is part of the build; no local research documents or test tools are needed to compile the app.

Override the version for a separately named build:

```sh
make check VERSION=0.1.3-r1-custom
```

This changes the artifact name, build directory and compiled version string. It does not install the app. Transfer the `.nwa` separately through your authorised calculator installation setup. `make clean` removes local build directories.

### Source layout

| File or group | Purpose |
| --- | --- |
| `Makefile` | Toolchain flags, versioned objects, icon conversion and `.nwa` packaging |
| `src/main.c` | App state, menus, keyboard handling, readers and forms |
| `src/calculator_view.inc` | Calculation rows and contextual additional results |
| `src/revision_view.inc` | Scientific/mathematical typesetting within revision text |
| `src/mathview.c`, `src/mathview.h` | Structured maths layout, drawing and input carets |
| `src/cas.c`, `src/cas.h` | Numerical/symbolic engine and calculation results |
| `src/topics.c`, `src/topics.h` | Course catalogue structure and maths topics |
| `src/aqa_catalog.inc`, `src/maths_details.inc` | Science notes and extended maths examples |
| `src/physics.c`, `src/chemistry.c`, `src/physics.h` | Equation definitions, variables, solver formulas and fact-sheet data |
| `src/units.c`, `src/units.h` | Unit groups, conversions and quantity parsing |
| `src/solvers.c`, `src/solvers.h` | Additional triangle and formula-rearrangement tools |
| `src/search.inc`, `src/reuse.inc` | Topic/equation search and reopening saved tool inputs |
| `src/sessions.inc`, `src/scriptstore.c`, `src/scriptstore.h` | Session serialisation and supported Epsilon Python-store access |
| `src/ascii_font.inc`, `src/icon.png` | Bundled font data and app icon |

Local `.nwa` files are kept in `releases/` and ignored by Git, along with objects and test ELFs. Downloadable `.nwa` files are distributed as GitHub Release assets. Local queues, research, assistant instructions and validation tools remain ignored; this README is the public-documentation exception.

## Validation and compatibility

v0.1.3 passed ARM package inspection and targeted runtime checks using a **Cortex-M7/Unicorn harness with simulated EADK keyboard/display calls**. The harness is local development tooling and is not included in this source-only repository; the commands above are sufficient to build the app without it.

Checks covered:

- Calculation/CAS behaviour, 33 CAS cases, keypad mappings and command prefixes.
- Fractions, powers, cursor movement, exact/decimal history and contextual results.
- General menu, preferences, shortcuts, topic/equation search and Alpha+ANS return.
- Named-session restore, autosave and migration, preserved unrelated Python scripts, reusable solver History and favourites.
- All **35 Physics fact sheets**, **112 Physics rearrangements**, **29 Chemistry fact sheets**, and their course-topic navigation.
- Linear, square, cubic, percentage and temperature conversions; the updated unit selectors/results and Courses ordering.
- Opening and scrolling all **129 entries exposed through Physics**, with eight first-position Key Ideas pages and no page reaching the 240-line wrapping limit.
- Rendered screenshots and a final-build menu/Physics Key Ideas smoke check.

These checks exercise the ARM app but do not model every aspect of Epsilon or physical hardware. **Physical N0120 testing of v0.1.3 remains pending.** Persistent storage deliberately recognises the established Epsilon 24.8.0 layout rather than assuming all firmware versions have identical memory structures.

When reporting a problem, include the NumAI version, calculator model, Epsilon version, whether it happens after relaunch/reset, the exact input or menu sequence, and a screen photo where relevant.

## Related projects and acknowledgements

- **[NumAI for Casio Graph Math+](https://github.com/cis5123467-sudo/numai-casio)** — the related Casio port, with native Casio controls, rendering, packaging and USB-backed saves. This NumWorks repository is the original project.
- **[LCD Math for Casio](https://github.com/cis5123467-sudo/lcd-math-casio)** — the related scientific-calculator project with its own LCD-style interface, editor and save format.
- **[NumWorks EADK](https://github.com/numworks/epsilon/tree/master/epsilon/eadk)** and **[nwlink](https://github.com/numworks/nwlink)** — external-app interfaces and build/packaging tools.
- **[KhiCAS for NumWorks](https://github.com/nwagyu/khicas)** and **[NumPlay](https://github.com/Mason363/NumPlay)** — informed the investigation into Python-script-backed session storage.
- Science content consists of original revision summaries keyed to the AQA [Physics 8463](https://www.aqa.org.uk/subjects/physics/gcse/physics-8463/specification), [Biology 8461](https://www.aqa.org.uk/subjects/biology/gcse/biology-8461/specification) and [Chemistry 8462](https://www.aqa.org.uk/subjects/chemistry/gcse/chemistry-8462/specification) specifications. Physics revision priorities also draw on the combined/Trilogy scope. Textbook pages and screenshots are not bundled.

See [GitHub Releases](https://github.com/cis5123467-sudo/numai/releases) for downloadable builds and detailed version-specific features, changes, compatibility notes and validation.
