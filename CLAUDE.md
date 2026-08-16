# Clash of Valor

A turn-based terminal RPG in C++11 with a full-screen raw-mode UI and SQLite
persistence. Build with `make`; test with `make test`. See `README.md` for
layout and run instructions.

## Design choices

These are standing preferences, not a one-time decision — apply them to any
future UI work in this codebase, not just the feature that introduced them.

- **Everything full-screen is centered, live to the current terminal size.**
  The splash art, the pinned header, and every menu's option list are all
  horizontally centered (and the splash also vertically) by measuring the
  real terminal at the moment they draw — never left-flush, never assuming a
  fixed width. If you add a new banner or menu, center it the same way.
- **Solid Unicode block glyphs (`█▄▀▌`), not thin ASCII figlet fonts, for
  banner art.** A figlet-substituted header was explicitly rejected as not
  matching "the same text as I had originally." When regenerating or adding
  banner art, prefer the block-glyph aesthetic (chafa's `--symbols block`
  output, or the same style by hand) over ASCII-line fonts.
- **True continuous scaling beats fixed size tiers where practical.** The
  header renders through `chafa` sized to the live terminal on every launch,
  rather than jumping between a few fixed breakpoints. Prefer this pattern
  for new scalable content; fixed tiers (like the splash's wide/compact/plain
  `.txt` files) are an acceptable fallback when a continuous renderer isn't
  available, not the first choice.
- **No `>` prompt text in interactive menus.** Arrow keys move a highlighted
  (reverse-video) selection; number keys still jump straight to and confirm
  an option, since that's the expected default way to play. The one
  exception is free-text entry (character names), which uses an ordinary
  line-editing prompt.
- **Menu transitions must be clean.** Selecting an option erases that menu's
  own drawn lines before the next thing prints, so old option lists don't
  pile up in scrollback across a session. This is scoped to the enumerated
  options themselves — caption lines printed above a menu (e.g. "Choose your
  next move") are left alone and behave like ordinary combat narration.
- **The non-interactive fallback is a hard invariant, not an afterthought.**
  Whenever stdout/stdin aren't real terminals (`make test`, a piped
  playthrough, CI), the game must behave exactly like a plain sequential CLI
  app: no raw mode, no ANSI, no external tool invocation. Every new
  interactive feature needs an explicit non-interactive path gated on
  `terminal::isInteractive()` / `rawterm::isActive()` — this is what keeps
  the regression suite passing without ever touching raw-mode code.
- **External tools (`ttfx`, `chafa`) are optional enhancements, never hard
  dependencies.** Both are located via `externaltool::findExecutable()`
  (PATH search plus a documented fallback location) and both have a native
  or plain-text fallback when unavailable. Any new external tool integration
  should follow the same shape: locate defensively, degrade gracefully,
  never assume installed.

## Known gotchas

Hard-won, easy to rediscover the hard way — read before touching raw-mode or
ANSI-escape code.

- **`\x1b7` / `\x1b8` (DECSC/DECRC) merge into a single bad hex escape** in a
  C++ string literal — `\x` consumes every following hex digit, so `\x1b7`
  parses as one out-of-range escape, not ESC followed by `'7'`. Always split:
  `"\x1b" "7"`.
- **DECRC is not reliably idempotent.** A *second* consecutive restore to
  the same DECSC-saved position is not guaranteed to reuse those
  coordinates — confirmed directly against a real terminal emulation
  library, where it silently reset to the screen origin instead. Don't
  design anything that calls DECRC more than once per DECSC save. Use a
  plain relative cursor move (`\x1b[NA` / `\x1b[NB`) instead when you need to
  return to a position repeatedly — see `Menu::eraseOwnLines()` and
  `animate::potionFlourish()` for the established pattern.
- **Running a child process can silently reset termios out from under you.**
  `ttfx` flips `ICANON`/`ECHO` back on as a side effect of running, even
  though nothing in this codebase calls `suspend()`. Termios belongs to the
  tty device, not to any one process. Call `rawterm::reassert()` after
  running any child that shares the terminal, regardless of whether you
  expect it to touch termios.
- **`ttfx` has no built-in skip.** Implemented here via fork + poll
  (`rawterm::keyAvailable`) racing the child's exit, then SIGTERM→SIGKILL.
- **A killed splash child must be reaped, not orphaned.** SIGINT during the
  splash has to kill the ttfx child too (`rawterm::registerChildForCleanup`)
  — otherwise it keeps animating into the pty for the rest of its run after
  the game has already exited, visibly garbling the user's shell prompt.
