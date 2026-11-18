# NumAI for NumWorks

Revision notes, equation sheets, solvers and a CAS calculator in a native 320 × 240 NumWorks external app. Current release **v0.1.2-r1**.

Download `numai-0.1.2-r1.nwa` from [Releases](https://github.com/cis5123467-sudo/numai/releases/latest). The target calculator is **NumWorks N0120 running Epsilon 24.8.0**. See the [release notes](https://github.com/cis5123467-sudo/numai/releases/tag/0.1.2-r1) for the changes and validation for this revision.

## Install

1. Download the `.nwa` release asset.
2. Connect the calculator by USB and open the [NumWorks app installer](https://my.numworks.com/apps) in a browser with WebUSB support, such as Chrome or Edge.
3. Using your calculator's existing external-app installation setup, select the calculator, add the `.nwa` file, and complete installation.
4. Open NumAI from the calculator's application menu.

Back up Python scripts, including NumAI's session scripts, before replacing an installation or resetting the calculator. The session storage implementation recognises the N0120 Epsilon 24.8.0 layout. On an unrecognised layout, saving and opening additional sessions are unavailable.

## Revision and solving

- **Maths courses** cover Edexcel IGCSE Mathematics A, OCR Level 3 Additional Mathematics and Edexcel A level Mathematics. The catalogue includes 112 maths topics, 224 additional worked examples and 58 calculator examples.
- **Separate AQA GCSE sciences** cover Physics, Biology and Chemistry, with specification references, supporting explanations, bold key terminology and Higher-only labels.
- **Chemical Changes** has a first-entry Key Ideas page with ten quick facts and the four acid reaction patterns. All 17 existing Topic 4 pages have expanded notes, balanced examples, practical methods, gas tests, titration calculations, extraction explanations and exam tips.
- **Topic search** matches entered words across course names, specification references, topic titles and note content. **Equation search** matches equation names, symbols, units, topics and specification references.
- **Physics and Chemistry equation sheets** provide word and symbolic equations, variables and units, unit names, worked examples, use cases and rearrangements. Their solvers let you choose the unknown and enter the known quantities, with compatible unit conversion, substituted working and units on answers.
- **Related equations** appear near the top of supported topic notes. Var opens them. Toolbox on a fact sheet opens its related course topic.
- **Unit conversions** cover length, area, volume, mass, time, temperature and other supported science quantities. Converted numbers can be copied to NumAI's clipboard.
- **Additional solvers** include a trigonometric equation solver with interval bounds, a triangle solver that handles the ambiguous two-triangle case, and a Physics formula rearranger.
- **CAS operations** include factorisation, simplification, solving for x, differentiation, indefinite and definite integration, substitution, interval root finding and completing the square.
- **Calculation input** supports factorials such as `5!/2!`, stored numeric variables, Ans and automatic completion of missing closing parentheses when EXE is pressed.
- **Answer display** supports exact fractions and roots where available, alongside decimal approximations. Decimal places are selectable from 0 to 10, with a default of 5. Contextual additional results include supported alternative formats and plots.
- **Named sessions and History** save calculations and supported solver inputs. Course favourites and separate Physics and Chemistry equation favourites provide quick access during a run.

The CAS uses x as its symbolic variable. Polynomial operations are bounded to supported degrees, with a general polynomial limit of 12. Indefinite integration supports polynomials and selected elementary functions. The definite-integral form currently accepts polynomials. The Physics rearranger and trigonometric solver handle their supported equation forms.

## NumWorks controls

| Key or action | NumAI function |
| --- | --- |
| TOOLBOX | Open Courses |
| TOOLBOX again in Courses | Search topics |
| VAR | Open Equations and CAS |
| VAR again in equation menus | Search equations |
| VAR on supported topic notes | Open related equations |
| TOOLBOX on an equation fact sheet | Open the related course topic |
| SHIFT + EXE | Open or close the general NumAI menu |
| OK / EXE | Open the selected item or submit a calculation |
| Up / Down | Move through lists, scroll notes or change input field |
| Up from calculation input | Select a previous calculation |
| Left / Right on a calculation row | Select the expression, supported answer or additional-results button |
| OK on a selected answer | Insert it into the calculation input |
| OK on the three dots | Open contextual additional results |
| OK on reusable History working | Reopen the original tool with its saved inputs |
| ALPHA | Enter a letter |
| ALPHA twice | Lock letter entry |
| SHIFT with ALPHA | Enter capital letters |
| ALPHA + decimal point | Enter factorial `!` |
| x,n,t | Enter x |
| SHIFT + pi | Enter `=` |
| SHIFT + square / square root | Enter `>` / `<` |
| SHIFT + sin / cos / tan | Enter inverse trigonometric functions |
| SHIFT + arrows | Select text |
| SHIFT + x,n,t | Cut selected text |
| SHIFT + VAR | Copy selected text, favourite a topic/equation, or copy a supported result |
| SHIFT + TOOLBOX | Paste NumAI's clipboard |
| SHIFT + DEL | Clear text to the right, or clear the field when at its end |
| SHIFT + power | Enter the value-store arrow `->` |
| ANS | Insert the previous value |
| ALPHA + ANS | Return to the calculation home screen from any page |
| BACK / HOME | Return to the previous page / exit to the calculator launcher |

The clipboard belongs to NumAI. Shift and Alpha indicators appear in the header. Angle mode defaults to degrees.

The general Shift + EXE menu contains Courses, Equations and CAS, New Session, Open Session, History, Keyboard Shortcuts, Preferences and About. Preferences include angle units, numeric answer style, worked steps and decimal places.

### Commands in calculation input

| Prefix | Operation | Example |
| --- | --- | --- |
| `>*` | Factorise | `>*{x^2-5x+6}` |
| `>/` | Simplify | `>/{3x+2(x-1)}` |
| `=>` | Solve for x | `=>{2x+4=0}` |
| `>**` | Complete the square | `>**{2x^2+12x+11}` |

Enter a numeric value followed by the store arrow and a supported letter to save a variable, for example `12->A`. Use `A+1` to reuse it.

## Display and saved work

The interface uses centred uppercase headers, a gold menu bar, the app's bundled font, superscript powers, supported subscripts and stacked fractions. Calculation rows show exact results and decimal approximations where supported. The application icon and display assets are included in the source.

NumAI saves named sessions as **`session_<name>.py`** records, such as `session_algebra.py`, in Epsilon's Python script store. They appear in the Python app and contain NumAI data in Python comments. NumAI reads them when it launches, restores the most recently active session, and saves after calculations, equation/CAS operations and preference changes.

- Up to **four sessions** are supported, with **10 KB reserved per session**, subject to the calculator's available shared script storage.
- Each session retains up to **12 calculation rows** and **five CAS/equation History entries**, plus stored numeric variables.
- Newly recorded History from v0.1.2 onwards can reopen the saved equation or solver, chosen unknown, input values, units and angle mode.
- Existing v0.1.1 sessions are readable. Older History entries can be viewed, while reusable inputs require an entry that recorded them.
- Favourites are held for the current run of the app.

Keep the session scripts to retain work. Epsilon's script store is RAM-backed, and a hardware reset can clear it. A full script store can prevent saving, in which case NumAI displays a message.

## Build from source

Install an ARM embedded GCC toolchain, Node.js/npm and Make. On macOS, the toolchain can be installed with:

```sh
brew install numworks/tap/arm-none-eabi-gcc node
```

From the repository directory:

```sh
make build
make check
```

The Makefile pins **nwlink 1.0.0** and targets Cortex-M7 with hardware floating point. It generates the EADK header through nwlink and compiles the C sources with warnings treated as errors.

For the current version, the output is **`../numai-0.1.2-r1.nwa`**. Object files and the loader-linked test ELF go in `build/0.1.2-r1/`. Override `VERSION` to build a separately named version:

```sh
make check VERSION=0.1.2-r1
```

Build output and `.nwa` files are ignored by Git. Downloadable calculator binaries are attached to GitHub releases.

## Test and validation

The repository includes a Cortex-M7 emulator harness using Unicorn, pyelftools and Pillow:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install unicorn pyelftools Pillow
make check
python tools/smoke.py build/0.1.2-r1/numai.elf
```

The harness runs the loader-linked ARM app with simulated EADK keyboard and display calls. Its checks cover calculation and CAS behaviour, courses and examples, equation fact sheets, unit conversions, searches, related-topic navigation, session persistence and migration, preservation of unrelated Python scripts, reusable History, focus and cursor behaviour. Screenshots are saved beside the test ELF in the versioned build directory.

For v0.1.2-r1, package inspection and focused emulator checks passed for all 18 Chemical Changes entries, all ten Key Ideas and four reaction patterns, Higher-only labels, both renamed headers, CAS calculations, sessions, search, reusable solver inputs and general navigation. Physical N0120 verification of this revision is pending.

## Source and related projects

- [NumAI for Casio Graph Math+](https://github.com/cis5123467-sudo/numai-casio) provides the related Casio port with its own controls, packaging and storage implementation.
- [NumWorks EADK](https://github.com/numworks/epsilon/tree/master/epsilon/eadk) and [nwlink](https://github.com/numworks/nwlink) provide the external-app tooling used to build this app.
- [NumPlay](https://github.com/Mason363/NumPlay) and [KhiCAS for NumWorks](https://github.com/nwagyu/khicas) informed the investigation into saving session data in Epsilon's script store.
- The science notes are original revision summaries keyed to the separate AQA [Biology](https://www.aqa.org.uk/subjects/biology/gcse/biology-8461/specification), [Chemistry](https://www.aqa.org.uk/subjects/chemistry/gcse/chemistry-8462/specification) and [Physics](https://www.aqa.org.uk/subjects/physics/gcse/physics-8463/specification) specifications.

See [the build queue](NEXT_BUILD.md) for recorded feature requests and [Releases](https://github.com/cis5123467-sudo/numai/releases) for version-specific changes and downloads.
