# Embedded Notes — Day 2

ESP32-S3 (LILYGO T-Display-S3) · ESP-IDF v6.0.2 · Windows

**Topic:** moving work off the CPU and onto dedicated hardware.

---

## 1. The idea of the day

On Day 1 the LED blinked inside a `while` loop with `vTaskDelay`. The CPU was doing
the waiting — busy, purely to keep an LED flashing.

Day 2 was three steps of handing that work to hardware:

| Stage | Who toggles the LED | What the CPU does |
|---|---|---|
| Day 1 | my `while` loop + `vTaskDelay` | waits, just for the LED |
| Goal 3 | `esp_timer` calls my callback | free, doing other work |
| Goal 4 | the LEDC peripheral, 5000×/second | only sets a ratio, then forgets about it |
| Goal 5 | *(inverted)* hardware tells the CPU when something happened | reacts instead of polling |

A microcontroller contains dozens of these units — timers, PWM, ADC, DMA, UART, SPI —
and they all exist for the same reason: **take repetitive work away from the CPU.**
Knowing which job to delegate to which unit is most of the craft.

---

## 2. Clock

CPU raised from 160 MHz to 240 MHz in menuconfig
(`Component config → ESP System Settings → CPU frequency`). Confirmed in the boot log:
`cpu freq: 240000000 Hz`.

The board has a **40 MHz crystal**. That frequency does not reach the CPU directly:

```
crystal (40 MHz) → PLL (multiplies) → divider → CPU clock
                                    → divider → peripheral clocks
                                    → divider → flash clock
```

This structure is called the **clock tree** and every MCU has one. Choosing 80/160/240
in the menu is really choosing a divider. ESP-IDF hides this; on STM32 you configure it
by hand.

---

## 3. Hardware timer (`esp_timer`)

```c
const esp_timer_create_args_t timer_args = {
    .callback = &led_timer_callback,
    .name     = "led"
};
esp_timer_handle_t timer;
ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 500000));   // microseconds
```

- The period is in **microseconds**: `500000` µs = 500 ms.
- `ESP_ERROR_CHECK` aborts and prints the reason if the call fails. Swallowing return
  values is a bad habit in embedded code; this macro makes checking mandatory.

**Proof it works:** the LED blinked every 500 ms while the main loop printed every
2000 ms. Log timestamps were `262 → 2262 → 4262`, exactly 2 s apart. Two different
rates, neither waiting on the other.

`esp_timer` vs `gptimer`: `esp_timer` is a software layer running many virtual timers
on top of one hardware timer — convenient. `gptimer` hands you a real hardware timer
where you set the resolution and alarm value yourself — precise and low-level.
*(gptimer not tried yet.)*

---

## 4. PWM (LEDC)

The LED is not actually dimmed — it is switched **on and off 5000 times per second**.
The eye cannot follow that, so it perceives the average. The fraction of each period
spent ON is the **duty cycle**.

```
duty 25%:   ▁▁▁█▁▁▁█▁▁▁█     dim
duty 75%:   ███▁███▁███▁     bright
```

The same technique drives motor speed, heater power, servo position and audio.

### Setup is two objects

1. `ledc_timer_config` — a PWM timer: frequency and duty resolution
2. `ledc_channel_config` — binds that timer to a GPIO

The split matters: **one timer can feed several channels.** Three LEDs at the same
frequency but different brightness = one timer, three channels.

### `set_duty` + `update_duty`

```c
ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
```

Two calls because the hardware does not change duty mid-period — it applies the new
value at the start of the next period, so the output never glitches.

### The frequency / resolution trade-off

The LEDC timer counts from a fixed source clock (≈80 MHz here). One PWM period requires
counting `2^resolution` steps, so:

```
max frequency = source clock / 2^resolution
```

| Resolution | Steps | Max frequency (80 MHz source) |
|---|---|---|
| 8 bit | 256 | ≈312 kHz |
| 13 bit | 8192 | ≈9.7 kHz |

**You cannot raise both.** More brightness steps means a longer period means a lower
frequency. 5 kHz at 13 bit is a good LED setting: no visible flicker, 8192 smooth steps.

Worth trying: drop the frequency to 50 Hz and the flicker becomes visible — that is why
PWM frequencies are chosen high.

### ESP32-S3 note

Only `LEDC_LOW_SPEED_MODE` exists. `HIGH_SPEED` is original-ESP32 only and errors out here.

---

## 5. GPIO interrupts

**The problem:** how do you know a button was pressed?

*Polling* means calling `gpio_get_level()` in a loop, asking over and over. Two costs:
the CPU is permanently busy, and a press-and-release between two polls is missed entirely.

An **interrupt** inverts it. You tell the hardware "notify me when this pin falls". The
moment it falls, the CPU drops whatever it was doing, runs your function, then resumes.
That function is an **ISR** (Interrupt Service Routine).

### The two ISR rules

**1. Keep it short.** Normal code is frozen while the ISR runs. The rule: the ISR sets a
flag or bumps a counter, and the **main code does the real work**.

Never in an ISR: logging, `malloc`, waiting, long loops.

**2. Mark it `IRAM_ATTR`.** Code normally lives in flash and executes through a cache.
If an interrupt arrives while the cache is being filled and the ISR itself lives in
flash, the chip locks up. `IRAM_ATTR` places the function in internal RAM so it is always
reachable.

### `volatile`, second encounter

`s_isr_count` is written by the ISR and read by the main loop. The compiler, looking at
the main loop alone, cannot see the ISR and concludes nothing modifies the variable —
so it reads it once and caches it. `volatile` forbids that.

Day 1: "hardware changes this address." Day 2: "an interrupt changes this variable."
Same need, different cause.

### Configuration

```c
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << BUTTON_GPIO),
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_NEGEDGE
};
gpio_config(&io_conf);
gpio_install_isr_service(0);
gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
```

The internal pull-up holds the pin at 3.3 V; pressing the button shorts it to GND.
`GPIO_INTR_NEGEDGE` catches that falling edge. No external resistor needed.

Used the board's **BOOT** button (GPIO0). For an external button on a breadboard: one
leg to the GPIO, the other to GND, nothing else — the internal pull-up does the rest.
(6×6 tact switches have 4 legs but only 2 circuits; opposite legs are already connected,
so use diagonal corners or the switch is permanently closed.)

---

## 6. Switch bounce

Counter output showed `+1` most presses but sometimes `+2` — one physical press
producing two interrupts. A mechanical contact does not close cleanly; the metal leaf
bounces for a few milliseconds and the pin goes high/low repeatedly.

Fix: ignore edges that arrive too soon after the previous one.

```c
static void IRAM_ATTR button_isr_handler(void *arg)
{
    static int64_t last_us = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_us < 50000) {   // 50 ms
        return;
    }
    last_us = now;

    s_isr_count++;
}
```

- `esp_timer_get_time()` returns microseconds since boot and is ISR-safe (already in IRAM).
- `static` keeps `last_us` alive between calls.
- 50 ms is a judgement call: too small and bounce returns, too large and fast presses get
  swallowed. There is no magic number.

The board's BOOT button has some hardware filtering, so bounce here was mild. Bare tact
switches on a breadboard bounce far worse.

**Lesson: hardware is not ideal.** The code has to know that.

---

## 7. Questions I asked, and the answers

**What are OpenOCD and GDB?**
Two programs working together.
- **GDB** is the debugger itself — breakpoints, variable inspection, stepping. It is
  generic (x86, ARM, Xtensa, RISC-V) and knows nothing about any specific hardware.
- **OpenOCD** is the translator: it speaks to the chip's JTAG debug unit on one side and
  GDB's network protocol on the other.

```
GDB ←→ OpenOCD ←→ USB ←→ JTAG unit inside ESP32-S3 ←→ CPU
```

The split means GDB stays the same on every architecture; the hardware-specific part is
isolated in OpenOCD. This board has the JTAG unit *inside the chip* and wired to USB —
most boards need a separate probe (ST-Link, J-Link).

**Where do I put the breakpoint?**
In a function that is called repeatedly and has a variable worth watching — `blink_led()`
was the obvious choice.

**Note:** GDB accepted `break blink_led` and even printed `s_led_state = 0` while
OpenOCD was failing. That value came from the **ELF file's compile-time initialiser**,
not from the live chip. Without OpenOCD connected, nothing real is being read.

**The board keeps blinking even when `idf.py flash` fails — because the firmware is
already in the chip?**
Yes. The firmware lives in flash, which is non-volatile. The board is independent of the
PC: plug it into a phone charger and it keeps running. The PC is only needed to *upload*
new code and to *read* serial output.

**What are these `+1` / `+2` numbers?**
`total ISR` is the cumulative interrupt count since boot. `+N` is how much it grew during
the last main-loop iteration (500 ms). So `+2` means the interrupt fired twice in that
window — either two real presses, or bounce.

**Where do I put the button?**
Nowhere — the code uses the board's own BOOT button (GPIO0).

**What did we actually do today?**
See section 1.

---

## 8. Debugging log

**1. LED permanently on again**
`blink_led()` still had only the `W1TS` write, with no `W1TC` and no branch on
`s_led_state`. Same fault as Day 1. *Always-on* and *always-off* are different symptoms:
always-on means the set path runs and the clear path does not.

**2. `idf.py menuconfig` → "CMakeLists.txt not found in project directory C:\Users\yavuz\Desktop"**
`idf.py` operates on the project **in the current directory**. The Desktop is not a
project. Also: bare `cd` does nothing in PowerShell, it needs a path.

**3. OpenOCD: `LIBUSB_ERROR_NOT_FOUND` → `could not find or open device`**
Windows driver problem, not code. The board exposes **two interfaces** over one USB
connection: Windows auto-bound a driver to the serial one (the COM port) but left the
JTAG interface without one. OpenOCD needs exactly that second interface.

**4. Zadig bound WinUSB to the wrong interface → lost the COM port**
After running Zadig, Device Manager showed **two** `USB JTAG/serial debug unit` entries
under *Universal Serial Bus devices* and **no ESP32 COM port** at all. WinUSB had been
applied to the serial interface too, so Windows stopped exposing it as a COM port —
and without a COM port nothing can be flashed.

Recovery: Device Manager → uninstall both entries (delete driver if offered) → unplug,
wait, replug → Windows reinstalls its defaults → COM port returns (as COM6, not COM5 —
the number can change).

Lesson: verify the interface number against Espressif's own JTAG documentation before
running Zadig. Getting it wrong costs the ability to flash, which blocks everything else.

**Time-boxing lesson:** this ate most of the session. A driver problem is not an embedded
systems problem — it should have been abandoned after 30 minutes and picked up later.
Goal 1 was deferred to Day 3.

**5. `'s_led_state' undeclared` / `implicit declaration of function 'blink_led'`**
The timer callback was written at the top of the file, above the declarations of the
variable and function it used. **C compiles top to bottom: a symbol must be declared
before it is used.**

Two fixes: move the definition up, or write a forward declaration
(`static void blink_led(void);`) near the top. Header files exist for exactly this reason.

**6. `includes driver/gpio.h, provided by esp_driver_gpio component(s). However,
esp_driver_gpio is not in the requirements list of "main"`**
ESP-IDF is built from ~200 **components**, and each component must declare the ones it
uses. Since IDF v5 the drivers are separate components so only the code you actually use
gets compiled.

```cmake
idf_component_register(SRCS "blink_example_main.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_timer
                       INCLUDE_DIRS "")
```

`REQUIRES` = the component appears in *my* public headers. `PRIV_REQUIRES` = used only
in my `.c` files. Application code almost always wants `PRIV_REQUIRES`.

Naming is regular: `driver/xxx.h` → `esp_driver_xxx`.
This will recur throughout the protocol work (uart, i2c, spi).
**The error message names the exact component and file to edit — read it, don't guess.**

**7. `implicit declaration of function 'esp_timer_get_time'`**
Missing `#include "esp_timer.h"`. Reflex for this error message: *I forgot the header.*
Search the function name in the ESP-IDF docs; the header is named at the top of its page.

**8. Flash size reverted to 2 MB**
`set-target` wipes `sdkconfig`, so the 16 MB setting from Day 1 was lost and the warning
came back. Anything configured in menuconfig must be reapplied after `set-target`.

---

## 9. Still open

- [ ] JTAG debugging (Goal 1) — Zadig interface number needs verifying first
- [ ] `gptimer` — the raw hardware timer, vs `esp_timer`'s software layer
- [ ] Input capture / `PCNT` to measure a signal's frequency
- [ ] Two interrupt sources at once, to observe priority behaviour
- [ ] Flash size back to 16 MB in menuconfig
- [ ] PSRAM still not enabled
- [ ] Buy: logic analyzer, multimeter, microSD module

## 10. Next — Block 4: protocols

UART, I2C, SPI. The 10-DOF sensor board (MPU6050 + HMC5883L/QMC5883 + BMP180) gives
three devices on one I2C bus — a bus scanner should find all three.

This is the block interviews ask about most.
