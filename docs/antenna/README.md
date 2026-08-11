# The antenna window

Tools → Antenna. Open a sweep from your analyser and it tells you where
the antenna is resonant and how much wire to add or remove.

It reads a file. It does not touch the radio and it cannot make your
analyser do anything — there is no driver behind it yet.

---

## The short version

Drag a `.s1p` onto the window. That is all it needs.

The big number is **where your antenna is resonant**, with the feed
resistance and the SWR there. Under it, the SWR at the start, middle
and end of the band, and how wide the antenna is at your SWR limit.

Everything else is optional:

* Give it a **target frequency** and it tells you the percentage.
* Add the **wire length** and the percentage becomes centimetres, and
  the big number becomes the instruction instead.
* The **coax** controls are folded away behind a button. If you
  calibrated at the antenna end — the right way — you never need them.

You can stop reading here. The rest is for when a number surprises you.

---

## First run: a test with a known answer

Two synthetic sweeps ship in `docs/antenna/samples/`. They are not real
measurements; they were generated from an impedance model so that the
right answer is known in advance. Use them to check the window works
before trusting it with your own antenna.

**Drag `dipole-40m-before.s1p` onto the window.** Set:

| control | value |
|---|---|
| Antenna | Dipole |
| Length | 20.00 m |
| Target | 7.030 MHz |
| Coax | none |
| SWR max | 2.0 |
| Region | Region 1 |

It must show:

| reading | value |
|---|---|
| headline | **+ 21.8 cm** per leg |
| headline with no target set | **7.183 MHz**, "resonant · 55 Ω · SWR 1.10" |
| resonant | 7.183 MHz, 55 Ω, SWR 1.10 |
| band start 7.000 | SWR 2.58 (red) |
| band middle 7.100 | SWR 1.51 |
| band end 7.200 | SWR 1.15 |
| usable | 7.047–7.333, 286 kHz |
| best SWR | 7.179 MHz at 1.10 |

Note that the best SWR (7.179) and the resonance (7.183) are close here
but not the same, and the title line says so.

**Now drag `dipole-40m-after.s1p` on** — this is the same antenna after
adding 11 cm per leg — and change Length to **20.22 m**.

| reading | value |
|---|---|
| resonant | 7.144 MHz |
| learned | "22.0 cm moved the resonance 39.1 kHz where the textbook rule expected 78.2" |
| exponent | L^-0.50 |
| headline | **+ 33.1 cm** per leg |

That last number is the point of the whole thing. The textbook rule
would have said +16.4 cm. This antenna responds half as much as the
maths assumes, and the window worked that out from your own two
measurements.

The previous sweep stays on screen as a faint dashed curve.

If any of these differ by more than a kilohertz or a hundredth of an
SWR point, something is wrong — tell me which one.

---

## Getting a sweep off a NanoVNA

1. Calibrate. **Where you calibrate is where the measurement starts** —
   see below.
2. Set the sweep range. Wider than the band: 6.9 to 7.5 for 40 m. For
   an end-fed, 3 to 30 MHz in one go.
3. Save to the SD card. On the NanoVNA-H that is
   `SAVE → S1P` in the menu; the file lands as `NanoVNA_xxx.s1p`.
4. Copy it to the computer and drag it onto the window.

Any analyser that writes Touchstone works — RigExpert, LiteVNA, a
proper VNA. The file format is what matters, not the instrument.

---

## The three things people get wrong

### Resonance is not the SWR minimum

**Resonant** means the reactance is zero — a fact about the LENGTH of
the wire. **Matched** means the impedance is near 50 Ω — a fact about
the FEED POINT. They are different conditions and they usually happen
at different frequencies.

On an antenna whose feed resistance rises across the band, the SWR
minimum sits where the resistance passes 50, which can be tens of
kilohertz from where the reactance vanishes. Trim to the dip and you
have trimmed to the wrong number — and it does not announce itself,
because the SWR looks excellent.

The window marks the resonance in amber and the SWR minimum separately,
and says "not the same as resonant" when they part company.

### The coax is part of what you measured

An analyser measures at its own port. A length of coax between there
and the antenna does two things:

**Loss flatters the SWR.** A *lossless* line changes it not at all,
which surprises people, but a lossy one absorbs the reflected wave
twice. Fifteen ohms at the feed point is an SWR of 3.33; through forty
metres of RG-58 on 40 m the meter reads 2.20, and through forty of
RG-174 on 10 m it reads 1.27. That last one looks like a good antenna.

**Phase rotates the impedance**, and this is the one that ruins a trim.
Two metres of RG-58 moved a 7.183 resonance to 7.247 in testing — 64
kHz, about 20 cm of wire cut off the wrong end. At five metres the
rising crossing vanished entirely and the only one left in the band was
a falling one at a perfectly plausible 45 Ω.

Two ways out, and the first is better:

* **Calibrate at the antenna end of the feedline.** Then there is
  nothing to remove and the Coax control stays at "none".
* Enter the cable type and length. The window removes it and the line
  under the curve says "at the antenna, 5.0 m of RG-58 removed".

The catalogue figures are nominal. Real cable varies by make, by age
and by how much water has got in. Choose "Custom" to enter your own
velocity factor and loss.

### Height above ground moves the resonance

Sometimes by more than the trim you are attempting. An antenna adjusted
at waist height in a garden is a different antenna at ten metres on a
summit. Adjust it at the height it will actually work at.

---

## Trimming

The rule is that resonant length scales inversely with frequency:

    L_new = L_old × f_measured / f_target

Measured high means the wire is too short. Measured low means too long.
Getting the sign wrong is the expensive mistake.

**Every recommendation to shorten is halved before it is shown**, and
the window says so. The first measurement usually carries a systematic
offset, and wire does not grow back. Two passes cost five minutes.
Lengthening is not halved — adding wire is reversible.

**Past ten percent** the simple rule stops being a measurement and
becomes a direction. The window says that too.

**After your second measurement** it stops using the textbook rule and
uses your antenna's actual behaviour. See the worked example above.

### By antenna type

**Dipole** — half the change on each leg. Doing it to one side only
skews the pattern and unbalances the feed.

**End-fed half-wave** — all the change at the far end. The harmonic
bands do NOT all move by the same percentage: the 49:1 transformer and
the counterpoise have their own frequency behaviour on top of the
wire's. Re-measure each band rather than assuming.

**Beam, driven element** — arithmetically identical to a dipole, half
per side. But two warnings that are not:

* This changes the MATCH only. The gain and the front-to-back live in
  the reflector and the directors. **Never trim a parasitic element to
  fix SWR** — the meter will reward you while you throw away what the
  beam was for.
* If the beam is fed through a gamma, hairpin or beta match, the
  adjustment is in the match, not in the element. Cutting the driven
  element there makes it worse.

**Vertical** — the radiator sets the resonance; the radials set the
feed resistance. A high SWR *at* resonance is a radial problem and no
amount of trimming the radiator will touch it.

---

## Reading the picture

* **Blue curve** — SWR.
* **Shaded blocks** — amateur bands, from the IARU plan for the chosen
  region. This is a band PLAN, not a licence: national allocations
  differ and yours may be narrower. The line under the curve says which
  region drew them.
* **Three dashed verticals** — the start, middle and end of the band
  being read off, with the SWR printed where each meets the curve, red
  above your limit.
* **Amber vertical** — the resonance.
* **Faint dotted verticals** — the other resonances, on a multiband
  sweep.
* **Faint dashed curve** — the previous sweep.

The vertical scale fits what is inside the bands, not the whole sweep.
Skirts outside a band routinely reach SWR 20 and scaling to them would
squash everything that matters into two pixels.

### Reading off your own range

A band is a default, not always the question. **READ FROM** and **TO**
take a span of your own — 7.020 to 7.040 for the CW window, say — and
the three verticals and the three tiles move to match. Both boxes must
be filled; one alone falls back to the band.

### USABLE

How wide the antenna actually is at your SWR limit, measured outwards
from the target. Green only when it covers the whole span being read
off — "286 kHz" means nothing without knowing whether that is enough.

---

## When it refuses to answer

The window would rather say nothing than say something wrong.

**"No resonance in this sweep"** — the reactance never reaches zero.
Sweep wider before trimming anything.

**"only downwards — is there coax between the analyser and the
antenna?"** — there are zero crossings but none of them a series
resonance. Either you are looking at an anti-resonance, or a feedline
is rotating the impedance. This is the message that catches the
five-metres-of-RG-58 case.

**"not swept"** — a band edge falls outside the measured range. Saying
nothing there would read as fine.

**"The wire got longer but the resonance went up, which is
backwards"** — something other than the length changed between two
measurements. Height, the counterpoise, a connector that was not tight.
Nothing is learned from that pair.

**"Removing 5 m of RG-58 pushed the reflection above 1"** — physically
impossible, so the cable loss figure is too high or the cable is
shorter than entered.

---

## What is not here yet

* **No analyser driver.** Sweeps arrive as files. When a driver does
  arrive it will need an interlock, because a VNA port is destroyed by
  a transmitter and the two share a coax.
* **The trim history is not saved between runs.** It could be, but
  without a name for *which* antenna the measurements belong to, a
  second antenna's sweeps would silently mix into the first one's and
  produce a confident exponent from two unrelated wires. That wants an
  antenna-project concept first.
* **Nothing measures the pattern.** This is about impedance. Where the
  signal goes is a different instrument.
