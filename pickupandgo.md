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
| M3 Scroll, move, holes, and jump | proposed |
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

## Play reference

Behavior comes from [NiccoloBalestrieri/MoonPatrol](https://github.com/NiccoloBalestrieri/MoonPatrol), a 2019 University of Parma Python project (`main`, no license). Read it for the rules. Do not copy it into this repo, and do not port `MoonPatrolGui.py`, `g2d.py`, or the sprite sheets named in `Constants.py`:

- `https://tomamic.github.io/images/sprites/moon-patrol.png`
- `https://tomamic.github.io/images/sprites/moon-patrol-bg.png`

Those sheets are the detailed arcade look: a sprite buggy and two scrolling mountain bands. This buddy draws flat 2600 shapes instead. One blue hill, not those two bands.

Reimplement these rules on the Wio:

- An arena of actors. The buggy, holes, rocks, and attackers move, then rectangles are tested for overlap.
- The ground scrolls left on its own. The buggy can also move forward and back on the screen.
- Holes come in a small and a large size. Rocks come in a one-shot size and a two-shot size. Both scroll with the ground.
- Up to three flying attackers. They reverse at the screen edges and sometimes drop a bomb. A bomb that reaches the ground can open a hole.
- A click fires one ground bullet and one air bullet, same as `Rover.attack`.
- Gravity pulls the buggy down after a jump. On this buddy, 5-way up jumps. Autoplay jumps on its own when the switch is idle.

Leave out their ground monster, the 30-second countdown, the win and lose alerts, and scoring only by survival time. Destroying a rock is still +100. Destroying an attacker is still +200. A crash still recovers and keeps the score.

Their `MoonPatrolTest.py` is the checklist for the rules: move changes position, a jump rises, a bullet removes a one-shot rock, a two-shot rock takes two hits, an attacker dies on a bullet. On the device, serial lines and a C920 frame stand in for that file.

## Arcade notes

[Computer Archeology: Moon Patrol](https://computerarcheology.com/Arcade/MoonPatrol/) is the arcade ROM write-up. Use it for behavior. Do not copy its disassembly, tile ROMs, or PROM color sets into this repo. The pixels stay the 2600 frame in `reference/atari-2600.png`.

Facts that match this plan:

- The buggy is one object with states: running, start jump, in the air, landing, and crashing in a crater. Wheels alternate two frames while it moves.
- Attract mode injects inputs, including a jump, and plays the scene by itself. That is the model for autoplay.
- A bubble-ship shot that hits the ground opens a crater.
- Rocks and boulders share one ground path, and each has an exploding version.

Facts we do not copy:

- The arcade keeps one forward shot and up to four air shots. This plan still fires one of each per click.
- Attract mode does not add to the score. This buddy does. Autoplay score stays on the HUD.
- Arcade score pops read 300, 500, 800, and 1000, and the stored score is six digits. This plan still uses +100 for a rock and +200 for an attacker.
- The arcade draws two scrolling mountain layers and a course letter above the playfield. This plan keeps one blue hill and the 2600 bottom HUD.

## Hardware

Seeed Wio Terminal, ATSAMD51P19 (the SAMD51 board this project calls the D51). Confirmed on USB at the time of M0.

| Item | Value |
| --- | --- |
| Port | `/dev/cu.usbmodem124301` |
| USB serial | `78DC45D45336464E4E202020FF132417` |
| Vendor / product | Seeed Studio, idVendor 10374 (0x2886), idProduct 32813 (0x802D) |
| MCU | ATSAMD51P19, Cortex-M4F, 120 MHz, 512 KB flash, 192 KB RAM, 4 MB external flash |
| Display | 2.4 inch, 320×240, ILI9341 |
| Input | Three top buttons `WIO_KEY_A` / `WIO_KEY_B` / `WIO_KEY_C`. The blue 5-way switch on the right of the bezel is the control meant by "joystick": `WIO_5S_UP` `DOWN` `LEFT` `RIGHT` `PRESS`. No separate joystick is attached. |
| Also on the board | Buzzer `WIO_BUZZER`, backlight `LCD_BACKLIGHT`, LIS3DH, light sensor, microphone, microSD, RTL8720DN Wi-Fi/BLE |

`/dev/cu.usbmodem21401` is a Particle Xenon on the same computer. Do not flash it.

The SAMD51 clock resets when USB power drops. There is no battery-backed RTC. M5 sets the clock from this Mac over serial.

PlatformIO is not installed yet. M1 installs it. Board id `seeed_wio_terminal`, platform `atmelsam`, framework `arduino`. Display library `seeed-studio/Seeed_Arduino_LCD`. Clock library `seeed-studio/Seeed Arduino RTC`.

## Camera

A Logitech HD Pro Webcam C920 is aimed at the Wio screen. Visual tests use a frame from that camera.

| Item | Value |
| --- | --- |
| Name | HD Pro Webcam C920 |
| AVFoundation video index | `0` |
| USB | VendorID 1133, ProductID 2093, Unique ID `0x121000046d082d` |

Checked 2026-10-12: a frame shows the terminal face-on, panel lit, blue five-way on the right of the bezel. The panel is the bright rectangle near the center. It is small in the 1920×1080 frame, enough to judge layout and colors. If HUD digits are unreadable, move the camera closer before failing the test.

Do not use the OBSBOT Meet 2, the MacBook Pro camera, or the JaketheDog camera. Those are also on this Mac.

Capture after the panel has had a second to settle:

```
mkdir -p test-captures
imagesnap -d "HD Pro Webcam C920" -w 2.0 test-captures/<milestone>.jpg
```

`imagesnap` and `ffmpeg` are installed. `test-captures/` is gitignored. The desk and the monitor behind the Wio are in the frame, so do not commit those JPEGs. Record pass or fail in this file, and what the frame showed.

## Choices in this plan

Approving M1–M6 approves these choices. Say so if one of them should change before M1.

- Draw on a 2-pixel grid so the scene stays chunky like the 2600. One blue hill, not the arcade’s two ranges.
- The terminal sits horizontally with the power port at the bottom. Screen-right is the player's right. The yellow HUD sits toward the power port. Seeed's pin names assume the USB-C is on the left, so this pose is that view turned clockwise until the USB-C is down.
- Play uses the onboard 5-way switch only. In this pose: right is `WIO_5S_DOWN`, left is `WIO_5S_UP`, up is `WIO_5S_RIGHT`, down is `WIO_5S_LEFT` and unused, press is `WIO_5S_PRESS`.
- Right moves the buggy right and scrolls the ground faster. Left moves the buggy left and scrolls the ground slower. Releasing left or right leaves the buggy where it is and leaves the speed where it is.
- Up jumps. Press shoots a ground bullet and an air bullet. Only one of each is in flight.
- Rock destroyed: 100 points. Attacker destroyed: 200 points. A crash does not clear the score.
- Autoplay runs when the 5-way has been idle for about 3 seconds: cruise speed, buggy toward the left, jumps holes, and shoots threats. Any direction or a press takes over that action immediately. Autoplay does not jump, steer, or shoot while the player is using the switch.
- HUD, top to bottom, yellow on black: centered score; `MM-DD  TIME HH:MM` with a blinking colon; segmented course bar. Seconds are the blink, not extra digits.
- Clock source is the Mac, sent as one line: `T2026-10-12 12:47:03`. Compile time is only a fallback before that line arrives.
- Sound, Wi-Fi, NTP, saved high score, lives, and game over are outside this plan.

## How to test

Every code milestone has a compile test and a device test. A milestone stays open until both are recorded here with the date and a pass or fail.

- Compile: `pio run` from the repo root exits 0.
- Flash: `pio run -t upload --upload-port /dev/cu.usbmodem124301` exits 0. Reconfirm the port with `ioreg` if the number changed. The callout device under the node named `Seeed Wio Terminal` is the port.
- Serial: `pio device monitor --port /dev/cu.usbmodem124301 --baud 115200`. Firmware lines that tests look for are listed on each milestone.
- Screen: capture a frame with the C920 command in the Camera section, then judge the milestone boxes from that frame. A screen box is passed only when the frame shows it.

Firmware should print `READY` after boot, then event lines such as `JUMP`, `POS n`, `SPEED n`, `SHOT`, `CRASH`, `RECOVER`, `ROCK 100`, `ATTACKER 200`, `SCORE n`, and `NOW YYYY-MM-DD HH:MM:SS`.

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

## M3 — Scroll, move, holes, and jump

**State:** proposed

The ground scrolls. With the power port at the bottom, 5-way right moves the buggy right and speeds the scroll up. 5-way left moves it left and slows the scroll down. 5-way up jumps. Holes open in the green, small and large. Autoplay jumps them while the switch is idle. Landing in a hole crashes, the scroll pauses, the buggy recovers, and the score is unchanged.

**Tests**

- [ ] `pio run` exits 0
- [ ] Pushing the switch to the player's right prints a higher `POS` and a higher `SPEED` than pushing it left, and the frame shows the buggy move right, then left
- [ ] 5-way up, toward the edge opposite the power port, prints `JUMP`, and the frame shows the buggy clearing a hole
- [ ] With the 5-way idle, serial shows `JUMP` before a hole and the frame shows the buggy clearing it
- [ ] Landing in a hole prints `CRASH` then `RECOVER`, and the scene is moving again
- [ ] `SCORE` after recover equals `SCORE` before the crash
- [ ] Results recorded in this section, then commit

## M4 — Shooting and score

**State:** proposed

Ground shots remove rocks. Air shots remove attackers. The yellow score matches the serial score.

**Tests**

- [ ] `pio run` exits 0
- [ ] 5-way press prints `SHOT`. Only one ground bullet and one air bullet exist at a time
- [ ] Destroying a one-shot rock prints `ROCK 100` and increases `SCORE` by 100
- [ ] A two-shot rock is still there after one `SHOT` and prints `ROCK 100` only on the second hit
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

Left alone, the buggy jumps holes and shoots rocks and attackers. Right and left change speed and position, up jumps, and a press shoots. Score climbs. The clock stays on the HUD.

**Tests**

- [ ] `pio run` exits 0
- [ ] Hands off for one full course bar: serial shows at least one `JUMP` and at least one of `ROCK` or `ATTACKER`, and `SCORE` is higher at the end
- [ ] During a later pass, right and left print `POS` and `SPEED`, up prints `JUMP`, and a press prints `SHOT`
- [ ] Screen still matches the reference layout, with a live score and a live `MM-DD  TIME HH:MM`
- [ ] No lockup over that run
- [ ] Results recorded in this section, then commit

## Next session

1. Read this file.
2. If M1–M6 are still `proposed`, stop and wait for approval.
3. If they are approved, do the first milestone that is not `done`.
4. Reconfirm the Wio port before flashing. Skip `/dev/cu.usbmodem21401`.
5. For every screen check, capture from the C920 only (`imagesnap -d "HD Pro Webcam C920"`). Leave the JPEG in `test-captures/`.
6. Write test results into the milestone before committing.
