# pickupandgo

Handoff for the Moon Patrol desk buddy on a Seeed Wio Terminal.
Read this file at the start of a session, then `git log` and `git status`.

## Status

**Awaiting approval of milestones M1–M6.** Do not write firmware, install PlatformIO, or flash the board until that list is approved.

| Milestone | State |
| --- | --- |
| M0 Handoff and local repo | done in this commit |
| M1 Toolchain and device link | proposed |
| M2 Atari 2600 still scene | proposed |
| M3 Scroll, holes, and jump | proposed |
| M4 Shooting and score | proposed |
| M5 Time and date in the game HUD | proposed |
| M6 Desk-buddy pass | proposed |

On approval, start M1 only. Finish a milestone, record its test results under that milestone, commit, then start the next one. Update the table above in the same commit.

## What this is

A desk toy that plays a small Moon Patrol scene on the Wio Terminal and looks like the Atari 2600 version. The buggy jumps holes and shoots rocks and attackers. The yellow score stays on screen. The date and the clock occupy the same HUD slots the 2600 game uses for lives and TIME.

The look to match is `reference/atari-2600.png` (320×240, the same size as the panel):

- Black sky, sparse white stars, a few yellow specks
- Two magenta attackers in the sky
- One rolling blue hill band
- Flat green ground
- Red buggy on the left, facing right
- Olive rock on the ground
- Yellow HUD along the bottom: centered score, a second line, a segmented course bar

The reference second line reads like `A- 2  TIME 032`. On this buddy that line is the date and the clock, for example `10-12  TIME 12:47`. The score above it stays the game score. The course bar stays under that line.

## Hardware

Seeed Wio Terminal, ATSAMD51P19 (the SAMD51 board this project calls the D51). Confirmed on USB at the time of M0.

| Item | Value |
| --- | --- |
| Port | `/dev/cu.usbmodem124301` |
| USB serial | `78DC45D45336464E4E202020FF132417` |
| Vendor / product | Seeed Studio, idVendor 10374 (0x2886), idProduct 32813 (0x802D) |
| MCU | ATSAMD51P19, Cortex-M4F, 120 MHz, 512 KB flash, 192 KB RAM, 4 MB external flash |
| Display | 2.4 inch, 320×240, ILI9341 |
| Input | Top buttons `WIO_KEY_A` / `WIO_KEY_B` / `WIO_KEY_C`, five-way `WIO_5S_UP` `DOWN` `LEFT` `RIGHT` `PRESS` |
| Also on the board | Buzzer `WIO_BUZZER`, backlight `LCD_BACKLIGHT`, LIS3DH, light sensor, microphone, microSD, RTL8720DN Wi-Fi/BLE |

`/dev/cu.usbmodem21401` is a Particle Xenon on the same computer. Do not flash it.

The SAMD51 clock resets when USB power drops. There is no battery-backed RTC. M5 sets the clock from this Mac over serial.

PlatformIO is not installed yet. M1 installs it. Board id `seeed_wio_terminal`, platform `atmelsam`, framework `arduino`. Display library `seeed-studio/Seeed_Arduino_LCD`. Clock library `seeed-studio/Seeed Arduino RTC`.

## Choices in this plan

Approving M1–M6 approves these choices. Say so if one of them should change before M1.

- Draw on a 2-pixel grid so the scene stays chunky like the 2600. One blue hill, not the arcade’s two ranges.
- Cruise speed is fixed. The buggy stays on the left.
- Jump: five-way up or button A. Shoot: five-way press or button B. A shot fires a ground bullet and an air bullet. Only one of each is in flight.
- Rock destroyed: 100 points. Attacker destroyed: 200 points. A crash does not clear the score.
- With no button pressed, the buddy jumps holes and shoots threats itself. A press still jumps or shoots immediately.
- HUD, top to bottom, yellow on black: centered score; `MM-DD  TIME HH:MM` with a blinking colon; segmented course bar. Seconds are the blink, not extra digits.
- Clock source is the Mac, sent as one line: `T2026-10-12 12:47:03`. Compile time is only a fallback before that line arrives.
- Sound, Wi-Fi, NTP, saved high score, lives, and game over are outside this plan.

## How to test

Every code milestone has a compile test and a device test. A milestone stays open until both are recorded here with the date and a pass or fail.

- Compile: `pio run` from the repo root exits 0.
- Flash: `pio run -t upload --upload-port /dev/cu.usbmodem124301` exits 0. Reconfirm the port with `ioreg` if the number changed. The callout device under the node named `Seeed Wio Terminal` is the port.
- Serial: `pio device monitor --port /dev/cu.usbmodem124301 --baud 115200`. Firmware lines that tests look for are listed on each milestone.
- Screen: a person looks at the panel and checks the boxes. This session cannot see the LCD. Do not mark a visual test passed without that check.

Firmware should print `READY` after boot, then event lines such as `JUMP`, `CRASH`, `RECOVER`, `ROCK 100`, `ATTACKER 200`, `SCORE n`, and `NOW YYYY-MM-DD HH:MM:SS`.

## Git

Local repository only, branch `main`, no remote. One commit per finished milestone, including the `pickupandgo.md` update that records the test results. Commit message says why the milestone landed.

Do not commit `.pio/`, build output, or secrets. `.gitignore` already excludes those. Do not change git config. Do not push unless asked.

## M0 — Handoff and local repo

**State:** done

The repo was empty. This commit is the plan, the reference image, and the ignore rules.

**Tests**

- [x] `pickupandgo.md` describes the device, the look, the milestones, and the tests
- [x] `reference/atari-2600.png` is in the repo
- [x] Wio Terminal identified as `/dev/cu.usbmodem124301` (2026-10-12)
- [x] Commit is on local `main`

## M1 — Toolchain and device link

**State:** proposed

A PlatformIO project builds and flashes. The panel lights, the screen is black, and serial prints `READY`. No playfield yet.

**Tests**

- [ ] `pio run` exits 0
- [ ] Upload to the Wio port exits 0
- [ ] Serial shows `READY`
- [ ] Backlight is on and the screen is black
- [ ] Results recorded in this section, then commit

## M2 — Atari 2600 still scene

**State:** proposed

A static frame matches the reference layout. Nothing moves yet.

**Tests**

- [ ] `pio run` exits 0
- [ ] Upload exits 0
- [ ] Screen: black sky and white stars
- [ ] Screen: two magenta attackers
- [ ] Screen: one blue rolling hill
- [ ] Screen: green ground, red buggy on the left, one olive rock
- [ ] Screen: yellow centered score, a second HUD line, a segmented course bar
- [ ] Results recorded in this section, then commit

## M3 — Scroll, holes, and jump

**State:** proposed

The ground scrolls. Holes open in the green. The jump control clears a hole. Landing in a hole crashes, the scroll pauses, the buggy recovers, and the score is unchanged.

**Tests**

- [ ] `pio run` exits 0
- [ ] Serial shows `JUMP` when button A or five-way up is pressed on the ground
- [ ] A jump started before a hole clears it (screen check)
- [ ] Landing in a hole prints `CRASH` then `RECOVER`, and the scene is moving again
- [ ] `SCORE` after recover equals `SCORE` before the crash
- [ ] Results recorded in this section, then commit

## M4 — Shooting and score

**State:** proposed

Ground shots remove rocks. Air shots remove attackers. The yellow score matches the serial score.

**Tests**

- [ ] `pio run` exits 0
- [ ] Button B or five-way press prints a shot event and only one ground bullet and one air bullet exist at a time
- [ ] Destroying a rock prints `ROCK 100` and increases `SCORE` by 100
- [ ] Destroying an attacker prints `ATTACKER 200` and increases `SCORE` by 200
- [ ] The centered HUD number matches the latest `SCORE` line (screen check)
- [ ] A crash still does not clear the score
- [ ] Results recorded in this section, then commit

## M5 — Time and date in the game HUD

**State:** proposed

The second HUD line shows the date in the lives slot and the clock in the TIME slot. A small host script sets the SAMD51 clock from the Mac.

**Tests**

- [ ] `pio run` exits 0
- [ ] Sending `T2026-10-12 12:47:03` (with the actual current time) makes serial `NOW` match the Mac within 2 seconds
- [ ] Screen second line shows that month-day and `TIME HH:MM`
- [ ] The colon blinks
- [ ] Score and course bar are still on their own rows
- [ ] About 10 seconds after the script exits, a new `NOW` has moved forward
- [ ] Results recorded in this section, then commit

## M6 — Desk-buddy pass

**State:** proposed

Left alone, the buggy jumps holes and shoots rocks and attackers. Buttons still work during that run. Score climbs. The clock stays on the HUD.

**Tests**

- [ ] `pio run` exits 0
- [ ] Hands off for one full course bar: serial shows at least one `JUMP` and at least one of `ROCK` or `ATTACKER`, and `SCORE` is higher at the end
- [ ] During a later pass, a jump press prints `JUMP` and a shoot press fires
- [ ] Screen still matches the reference layout, with a live score and a live `MM-DD  TIME HH:MM`
- [ ] No lockup over that run
- [ ] Results recorded in this section, then commit

## Next session

1. Read this file.
2. If M1–M6 are still `proposed`, stop and wait for approval.
3. If they are approved, do the first milestone that is not `done`.
4. Reconfirm the Wio port before flashing. Skip `/dev/cu.usbmodem21401`.
5. Write test results into the milestone before committing.
