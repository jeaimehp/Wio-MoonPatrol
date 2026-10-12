# pickupandgo

Handoff for the Moon Patrol desk buddy on a Seeed Wio Terminal.
Read this file at the start of a session, then `git log` and `git status`.

## Status

**M5 is next.** Lives, the title screen, progressive attacker types, autoplay pace, and three shots in flight were added after M4 at the user's request (see M4b). M3 and M4 landed together in one commit (the user widened M3 mid-way: moving attackers, bombs, readable HUD, sound toggle). Their tests passed on serial with injected input; the physical 5-way and buttons still need one hands-on check by the user. M5–M6 are approved. Do not stop between milestones to ask permission to continue.

Latest commit is M3+M4. The board is running that build.

| Milestone | State |
| --- | --- |
| M0 Handoff and local repo | done |
| M1 Toolchain and device link | done |
| M2 Atari 2600 still scene | done |
| M3 Scroll, move, holes, and jump | done — physical switch check pending |
| M4 Shooting and score | done — physical switch check pending |
| M5 Time and date in the game HUD | approved |
| M6 Desk-buddy pass | approved |

Finish a milestone, record its test results, commit, then start the next one.

## What this is

A desk toy that plays a small Moon Patrol scene on the Wio Terminal and looks like the Atari 2600 version. The buggy jumps holes and shoots rocks and attackers. The yellow score stays on screen. The date and the clock occupy the same HUD slots the 2600 game uses for lives and TIME.

The look to match is `reference/atari-2600.png` (320×240, the same size as the panel):

- Black sky, sparse white stars, a few yellow specks
- Two magenta attackers in the sky
- One rolling blue hill band
- Flat green ground
- Buggy on the left, facing right. The drawn rectangle buggy was rejected; the 2600 buggy cut from this frame was approved.
- Olive rock on the ground
- Yellow HUD along the bottom: centered score, a second line, a segmented course bar

The reference second line reads like `A- 2  TIME 032`. On this buddy that line is the date and the clock, for example `10-12  TIME 12:47`. The score above it stays the game score. The course bar stays under that line.

## Play reference

Behavior comes from [NiccoloBalestrieri/MoonPatrol](https://github.com/NiccoloBalestrieri/MoonPatrol), a 2019 University of Parma Python project (`main`, no license). Read it for the rules. Do not copy it into this repo, and do not port `MoonPatrolGui.py`, `g2d.py`, or the sprite sheets named in `Constants.py`:

- `https://tomamic.github.io/images/sprites/moon-patrol.png`
- `https://tomamic.github.io/images/sprites/moon-patrol-bg.png`

Those sheets are the detailed arcade look: a sprite buggy and two scrolling mountain bands. Do not use them. One blue hill, not those two bands. The hill, ground, attackers, and HUD stay flat 2600 shapes. The vehicle is the exception below.

Reimplement these rules on the Wio:

- An arena of actors. The buggy, holes, rocks, and attackers move, then rectangles are tested for overlap.
- The ground scrolls left on its own. The buggy can also move forward and back on the screen.
- Holes come in a small and a large size. Rocks come in a one-shot size and a two-shot size. Both scroll with the ground.
- Up to three flying attackers. They reverse at the screen edges and sometimes drop a bomb. A bomb that reaches the ground can open a hole.
- A click fires one ground bullet and one air bullet, same as `Rover.attack`.
- Gravity pulls the buggy down after a jump. On this buddy, 5-way up jumps. Autoplay jumps on its own when the switch is idle.

Leave out their ground monster, the 30-second countdown, the win and lose alerts, and scoring only by survival time. Destroying a rock is still +100. Destroying an attacker is still +200. A crash still recovers and keeps the score.

Their `MoonPatrolTest.py` is the checklist for the rules: move changes position, a jump rises, a bullet removes a one-shot rock, a two-shot rock takes two hits, an attacker dies on a bullet. On the device, serial lines and a C920 frame stand in for that file.

## Vehicle sprites

The rectangle buggy in `src/main.cpp` (`drawBuggy`) looked bad on the panel. Replace it with the vehicle frames from [General Sprites](https://www.spriters-resource.com/arcade/moonpatrol/asset/106392/), Spriters Resource asset 106392.

| Item | Value |
| --- | --- |
| Page | `https://www.spriters-resource.com/arcade/moonpatrol/asset/106392/` |
| Name | General Sprites |
| Game | Moon Patrol, arcade |
| File | PNG, 299×400, 13.14 KB |
| Uploaded | 2018-06-13, Seven Seas |

On 2026-10-12 Cloudflare blocked that page, so the PNG is not in the repo. Download it, cut out the buggy frames (run, the two wheel frames, jump, and crash if the sheet has them), and draw those on the left of the 2600 scene. Keep one blue hill. Do not switch the rest of the scene to the arcade mountains.

## 2600 gameplay and sound

[Moon Patrol - Atari 2600 (1080p@60fps)](https://www.youtube.com/watch?v=vGFMsMGQNuo) by Gen X Classic Gaming is the motion and sound reference. It is the 2600 game, the same look as `reference/atari-2600.png`, and it includes the sounds.

Watch it before M3. Match the jump, the shots, the holes, the rocks, and the attackers to that video. Play the cues on `WIO_BUZZER`: drive, jump, shot, hit, and crash. Do not copy arcade AY register dumps or a soundtrack rip. Wi-Fi, NTP, and a saved high score stay out. (Lives and game over were added by the user on 2026-10-12; see Choices.)

### What the video shows (watched 2026-10-12)

Measured in the browser pane: a WebAudio analyser logged loudness and pitch peaks every 40 ms over the whole 2-minute video, then still frames at each sound showed the action. Pitches are approximate.

| Action | Sound | Buzzer cue |
| --- | --- | --- |
| Driving | Silent. No engine drone at any point. | Nothing. Keep "drive" silent to match. |
| Shot (ground + air together) | Short rising chirp, about 1.2 kHz up to 2 kHz, about 0.12 s | Sweep 1200 → 2000 Hz in ~120 ms |
| Jump | Falling tone, 660 → 510 Hz over about 0.3 s | Sweep 660 → 510 Hz in ~300 ms |
| Attacker wave enters | Swoop down and back up, 980 → 690 → 980 Hz, about 0.45 s | Sweep 980 → 690 → 980 Hz in ~450 ms |
| Attacker drops a bomb | Short fall, 870 → 625 Hz, about 0.2 s | Sweep 870 → 625 Hz in ~200 ms |
| Rock destroyed (+100) | Low noisy thud, 85–100 Hz, about 0.3 s | Random 80–110 Hz steps for ~300 ms |
| Attacker destroyed (+200) | No separate cue isolated; it is lost under the shot chirps | Reuse the rock thud |
| Crash in a hole | Longer low rumble, 75–230 Hz, about 0.8–1 s. The buggy breaks into yellow checkered debris. | Random 75–230 Hz steps for ~900 ms |
| Last life lost | Rising warble 160 → 380 Hz | Out of scope (no game over) |

Motion, scaled to the 320×240 panel (video pixels × 2/3):

- Jump: the buggy rises about 10 px, hangs, and lands. The whole arc is about 1.4 s. It is a shallow, floaty jump, not a high one.
- The buggy is about 23 px wide, salmon pink, and stays near the left third.
- Ground bullet: a short white dash moving right at buggy height. Air bullet: a yellow vertical dash moving up. Several air dashes can be on screen in the 2600 game; this plan keeps one of each.
- A hole is a notch cut into the green that shows the blue of the hill through it. A crater appears where a bomb lands.
- Attackers in the video are yellow-orange; the reference frame shows magenta. Keep the reference color.
- After a crash the scene resets and the buggy drives on. The score is kept (lives drop in the 2600 game; this plan has no lives).

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
| Input | Three buttons on the edge that is on the left when the power port is at the bottom. The top one of those is the mode button, expected pin `WIO_KEY_C` (Seeed's rightmost top button when the USB-C is on the left). `WIO_KEY_A` and `WIO_KEY_B` are unused. The blue 5-way switch is the play control. |
| Also on the board | Buzzer `WIO_BUZZER`, backlight `LCD_BACKLIGHT`, LIS3DH, light sensor, microphone, microSD, RTL8720DN Wi-Fi/BLE |

`/dev/cu.usbmodem21401` is a Particle Xenon on the same computer. Do not flash it.

The SAMD51 clock resets when USB power drops. There is no battery-backed RTC. M5 sets the clock from this Mac over serial.

PlatformIO 6.2.0 is installed (`python3 -m platformio`). Board id `seeed_wio_terminal`, platform `atmelsam`, framework `arduino`. The display library is Seeed_Arduino_LCD 1.6.0 from that board package. Do not add registry `Seeed_Arduino_LCD` 2.0.3; that release targets XIAO boards and does not build for this terminal. Clock library for M5: `seeed-studio/Seeed Arduino RTC`.

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
- Play uses the onboard 5-way switch. In this pose: right is `WIO_5S_DOWN`, left is `WIO_5S_UP`, up is `WIO_5S_RIGHT`, down is `WIO_5S_LEFT` and unused, press is `WIO_5S_PRESS`.
- The top-left button, the upper button on the left edge in this pose, changes mode. From autoplay it starts a new game: score returns to 0, the field resets, and the 5-way is live. From a game it returns to autoplay and keeps the score. It does not matter that the 5-way has been idle. Expected pin `WIO_KEY_C`.
- Right moves the buggy right and scrolls the ground faster. Left moves the buggy left and scrolls the ground slower. Releasing left or right leaves the buggy where it is and leaves the speed where it is.
- Up jumps. Press shoots a ground bullet and an air bullet. Up to three of each are in flight (user, 2026-10-12). Air shots also knock out bombs (`BOMB HIT`).
- Rock destroyed: 100 points. Attacker destroyed: 200 points. Clearing a hole: 100 points (user, 2026-10-12; every score change in the 2600 video is +100, the first one on a cleared hole). A crash does not clear the score.
- Attackers drop bombs straight down (salmon 2×6 dashes, as in the video). A bomb on the buggy crashes it; one landing well ahead can open a small crater.
- Sound is off in autoplay by default, on when a new game starts, and the top middle button (`WIO_KEY_B`) toggles it any time. Serial prints `SOUND ON` / `SOUND OFF`.
- HUD text is the 8×8 font at 2× (user asked for readable text); score at y 181, date/time at y 200, course bar at y 222. The reference's 5–7 px lines were too small on the desk.
- The buddy boots in autoplay: buggy toward the left, jumps holes, shoots rocks, attackers, and bombs overhead. Autoplay picks its pace like a player: fast on open road, slow to line up a hole or rock, middle under nearby attackers, and it brakes or speeds up to get out from under a bomb. It holds its pace while airborne. A new game stays a new game until the top-left button is pressed again.
- Lives (user, 2026-10-12): a game starts with 3, gains one at every 10,000 points up to 5 (`EXTRA LIFE`, `LIVES n`). Each crash in a game costs one. At 0: `GAME OVER` for 3 s, the title screen, then a fresh autoplay run at score 0. Autoplay has no lives.
- During a game the second HUD line is the 2600 layout: the lives icon from the reference (14×5 at 2×), the count, and `TIME HH:MM`. Autoplay shows `MM-DD  TIME HH:MM`.
- Title screen (traced from the video's first seconds): the MOON/PATROL logo and `©1983 ATARI` cycling green, pink, yellow, red, purple, magenta, white, blue every half second, with two of each attacker type flying around. It shows for 5 s before each new game (the top-left button or a press skips it) and between autoplay runs; one autoplay run is one course, about 3 minutes. Not at power-up.
- Attacker types join as the score climbs (user, 2026-10-12): magenta from the start, the yellow saucer (16×7) from 3,000, the white pair (two 6×4 sprites) from 6,000. Each later type is faster, swings wider, and bombs more often. All are worth 200.
- HUD, top to bottom, yellow on black: centered score; `MM-DD  TIME HH:MM` with a blinking colon; segmented course bar. Seconds are the blink, not extra digits.
- Clock source is the Mac, sent as one line: `T2026-10-12 12:47:03`. Compile time is only a fallback before that line arrives.
- Sound follows the 2600 video above and plays on `WIO_BUZZER`. Wi-Fi, NTP, and a saved high score stay out.

## How to test

Every code milestone has a compile test and a device test. A milestone stays open until both are recorded here with the date and a pass or fail.

- Compile: `pio run` from the repo root exits 0.
- Flash: `pio run -t upload --upload-port /dev/cu.usbmodem124301` exits 0. Reconfirm the port with `ioreg` if the number changed. The callout device under the node named `Seeed Wio Terminal` is the port.
- Serial: `pio device monitor --port /dev/cu.usbmodem124301 --baud 115200`. Firmware lines that tests look for are listed on each milestone.
- Screen: capture a frame with the C920 command in the Camera section, then judge the milestone boxes from that frame. A screen box is passed only when the frame shows it.

Firmware should print `READY` after boot, then event lines such as `AUTO`, `NEW`, `JUMP`, `POS n`, `SPEED n`, `SHOT`, `CRASH`, `RECOVER`, `ROCK 100`, `ROCK HIT`, `ATTACKER 200`, `HOLE 100`, `SCORE n`, `SOUND ON`/`SOUND OFF`, `FPS n`, and `NOW YYYY-MM-DD HH:MM:SS`.

Serial input for tests: a line `I` followed by letters injects controls, the same as the hardware: `R`/`L` hold right/left for 10 frames, `U` jumps, `P` presses, `C` is the mode button, `S` the sound button. For example `IC` starts a new game. Results from injected input must say so; the physical switch is still checked by hand.

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

**State:** done

A PlatformIO project builds and flashes. The panel lights, the screen is black, and serial prints `READY`. No playfield yet.

**Tests** (2026-10-12)

- [x] `python3 -m platformio run` exits 0. Seeed_Arduino_LCD 1.6.0. RAM 4000 bytes, flash 48976 bytes.
- [x] Upload exits 0. SAM-BA failed and dropped the board off USB. After a reset the bootloader volume `/Volumes/Arduino` was up (`UF2 Bootloader v3.7.0`, family `0x55114460`). Copying a UF2 of `firmware.bin` at address `0x4000` rebooted the board as `Seeed Wio Terminal` on `/dev/cu.usbmodem124301` (idProduct 32813).
- [x] Serial shows `READY` four times at 115200.
- [x] C920 frame `test-captures/m1.jpg` (not committed): backlight is on and the panel is black. The blue glow is the webcam's own light reflected in the glass.
- [x] Results recorded in this section, then commit

Later flashes: `python3 tools/flash_uf2.py`. It uses the UF2 bootloader. If the volume never appears after the 1200-baud touch, double-click reset until `/Volumes/Arduino` is mounted and run it again. Do not flash `/dev/cu.usbmodem21401`. Do not use `bossac`.

## M2 — Atari 2600 still scene

**State:** done

The layout is on the device and nothing moves yet. `setRotation(3)` puts the yellow HUD toward the power port. Rotation 1 had put it on the far edge. The buggy, the attackers, and the rock are bitmaps cut pixel for pixel from `reference/atari-2600.png` and drawn by `drawBits`. Drawn shapes (rectangles, circles, rounded UFOs) looked rough on the panel; the user approved the cut sprites.

**Tests** (2026-10-12)

- [x] `python3 -m platformio run` exits 0. RAM 4000 bytes, flash 51472 bytes.
- [x] UF2 copy exits 0. `tools/flash_uf2.py` touched `/dev/cu.usbmodem124301` at 1200 baud, then wrote `/Volumes/Arduino/moonpatrol.uf2` (51472 bytes at 0x4000). Serial prints `SCENE`.
- [x] Screen: black sky and white stars. C920 `test-captures/m2b.jpg` (not committed).
- [x] Screen: two attackers in the sky. The webcam blows the light magenta (208, 132, 192) out toward white.
- [x] Screen: one blue rolling hill.
- [ ] Screen: green ground, buggy on the left, one rock on the right. Ground, side, and rock position matched. The user rejected the drawn buggy. Replace it from the General Sprites sheet and shoot the panel again.
  - 2026-10-12, second pass: General Sprites could not be fetched. Cloudflare's human check would not pass in the in-app browser or in the user's Chrome, and the Internet Archive was offline. The buggy is now the 2600 sprite itself, cut pixel for pixel from `reference/atari-2600.png` (28×9, 4×1 blocks, salmon 208,112,112), plus a second wheel frame and a jump frame traced from the 1080p gameplay video. It is drawn at x 108, wheels on the ground line, as in the reference.
  - `pio run` exits 0 (flash 51456 bytes). UF2 flash exits 0, serial prints `SCENE`.
  - C920 `test-captures/m2c.jpg`: the buggy is a salmon shape in the right place on the ground, but the camera overexposed the panel (hill and ground read as white), so its shape cannot be judged from the frame. `imagesnap` only works from the user's Terminal panel; the agent shell is denied camera access.
  - Third pass: the attackers (16×8) and the rock (12×6) are also cut from the reference, in 2×1 blocks, at the reference positions. Flash 50840 bytes. C920 `test-captures/m2d.jpg` shows both attackers in the reference shape; the rock is lost in the overexposed ground.
  - [x] The user looked at the device and approved the buggy, attackers, and rock (2026-10-12).
- [x] Screen: yellow centered `900`, second line `10-12  TIME 12:47`, segmented course bar, all toward the power port.
- [x] Results recorded in this section, then commit

## M3 — Scroll, move, holes, and jump

**State:** done, physical switch check pending

The ground scrolls. With the power port at the bottom, 5-way right moves the buggy right and speeds the scroll up. 5-way left moves it left and slows the scroll down. 5-way up jumps. Holes open in the green, small and large. Autoplay jumps them while the switch is idle. Landing in a hole crashes, the scroll pauses, the buggy recovers, and the score is unchanged.

Built as an 8-bit indexed framebuffer for rows 0–176 with the reference palette, pushed row by row; only the driving band (rows 112–176) and the sky rows under moving things are pushed each frame. Serial reports `FPS 30`. Hole shape and crash debris are traced from the 1080p video; the jump is the video's 10 px, 1.4 s arc. Attackers started moving here at the user's request (M4 had them).

**Tests** (2026-10-12)

- [x] `pio run` exits 0. RAM 66 KB, flash 58–66 KB.
- [x] Right then left prints higher then lower `POS`/`SPEED`: `POS 109 → 168`, `SPEED 78 → 106`, then back to `POS 135`, `SPEED 90`. Injected input (`IR`, `IL`). Physical switch not yet confirmed; the C920 frames do not show the move.
- [x] Up prints `JUMP` (injected `IU`). C920 `test-captures/m3a.jpg` shows the buggy in the air over a hole.
- [x] Idle: 8 `JUMP`s and no `CRASH` in 25 s of autoplay; `HOLE 100` follows each.
- [x] Landing in a hole prints `CRASH` then `RECOVER` and the scene moves again (`test-captures/m3-switch.log`, `m4-serial-game.log`).
- [x] `SCORE` after `RECOVER` equals `SCORE` before `CRASH` (900/900 in autoplay, 300/300 and 700/700 in a game).
- [x] HUD text 2×, readable in `test-captures/m3a.jpg`.
- [ ] Physical 5-way right/left/up and the top-left button, by the user.
- [x] Results recorded in this section, then commit (with M4).

## M4 — Shooting and score

**State:** done, physical switch check pending

Ground shots remove rocks. Air shots remove attackers. The yellow score matches the serial score. Attackers bomb the buggy. Clearing a hole scores 100. Autoplay shoots rocks ahead and attackers overhead. Rocks: the reference rock is one shot; a 16×8 boulder (the same shape grown) takes two and prints `ROCK HIT` on the first. Destroyed rocks flash yellow; destroyed attackers break into a checkerboard.

The board hung once: the core's `noTone()` waits on timer TC0, whose clock is only enabled by the first `tone()`. Muting before any sound had played froze the loop. `stopSound()` now skips `noTone()` until a tone has started.

**Tests** (2026-10-12)

- [x] `pio run` exits 0. Flash 65784 bytes.
- [x] Press prints `SHOT`; six injected presses inside 0.5 s gave one `SHOT` (one of each in flight). Later raised to three of each: eight presses in 0.4 s gave three `SHOT`s.
- [x] One-shot rock: `ROCK 100`, `SCORE` +100.
- [x] Two-shot rock: `ROCK HIT` on the first shot, `ROCK 100` on the second.
- [x] Attacker: `ATTACKER 200`, `SCORE` +200 (autoplay and injected game).
- [x] HUD matches serial: C920 `test-captures/m4a.jpg` at 16:08:08 shows `1700`; serial printed `SCORE 1700` at 16:08:08.
- [x] A crash does not clear the score (bomb and hole crashes, 300/300 and 700/700).
- [x] Sound: autoplay boots `SOUND OFF`, `NEW` turns it on, `AUTO` turns it off, the sound button toggles (injected `IS`). Buzzer not yet confirmed by ear.
- [ ] Physical press and the top middle button, by the user.
- [x] Results recorded in this section, then commit (with M3).

## M4b — Lives, title screen, attacker types, autoplay pace

**State:** done (added by the user during M4, 2026-10-12)

**Tests** (2026-10-12)

- [x] `pio run` exits 0. Flash 70944 bytes.
- [x] Top-left button in autoplay prints `TITLE`; 5 s later `NEW`, `SOUND ON`, `LIVES 3`, `SCORE 0`. A second press during the title skips it.
- [x] Three crashes in an idle game: `LIVES 2`, `1`, `0`, then `GAME OVER`, `TITLE` 3 s later, `AUTO` 5 s after that (`test-captures/lives-title.log`).
- [x] C920 `test-captures/title.jpg`: logo, copyright line, and all three attacker types on the title. `test-captures/game-hud.jpg`: lives icon, `3`, `TIME 12:47`.
- [x] Autoplay pace: `SPEED` from 64 to 96 px/s across a minute (`test-captures/auto-pace.log`).
- [x] Autoplay over 90 s: 2 crashes (`BY BOMB`, `BY HOLE`), 21 holes, 12 rocks, 5 attackers, score 4600 (`test-captures/auto-pace2.log`). Before bomb dodging and the lower bomb rates it was 5 crashes in 60 s, 4 by bombs.
- [ ] Extra life at 10,000 not yet seen on the device; a hand-played game has not reached it.
- [ ] Physical buttons and buzzer, by the user.

## M5 — Time and date in the game HUD

**State:** approved

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

**State:** approved

Left alone, the buddy boots in autoplay: the buggy jumps holes and shoots rocks and attackers. The top-left button starts a new game. Right and left change speed and position, up jumps, and a press shoots. Another press on that button returns to autoplay. Score climbs. The clock stays on the HUD.

**Tests**

- [ ] `pio run` exits 0
- [ ] Hands off after boot: serial shows `AUTO`, at least one `JUMP`, and at least one of `ROCK` or `ATTACKER`, and `SCORE` is higher at the end of one course bar
- [ ] The top-left button prints `NEW` and the next `SCORE` line is 0
- [ ] During that game, right and left print `POS` and `SPEED`, up prints `JUMP`, and a press prints `SHOT`
- [ ] Leaving the 5-way idle during the game does not print `AUTO`
- [ ] Pressing the top-left button again prints `AUTO`, and the following `SCORE` is not reset to 0
- [ ] Screen still matches the reference layout, with a live score and a live `MM-DD  TIME HH:MM`
- [ ] No lockup over that run
- [ ] Results recorded in this section, then commit

## Next session

1. Read this file, then `git log` and `git status`.
2. Ask the user for the hands-on check of the 5-way, the top-left (mode) and top middle (sound) buttons, and the buzzer, while logging serial. Then start M5.
3. New art comes from the reference frame or the video, not drawn shapes. Crash debris is yellow and salmon checkers about the buggy's size (seen in the video).
4. M3–M6 are approved. Continue through them without asking permission. Record each test and commit before the next one.
5. Reconfirm the Wio port before flashing. Skip `/dev/cu.usbmodem21401`.
6. For every screen check, capture from the C920 only (`imagesnap -d "HD Pro Webcam C920"`). Leave the JPEG in `test-captures/`. Run `imagesnap` and `tools/flash_uf2.py` from the user's Terminal panel: the agent shell is denied the camera and cannot write to `/Volumes/Arduino`. The C920 overexposes the panel in this room, so for shape checks also ask the user to look.
