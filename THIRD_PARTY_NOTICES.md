# Third-party notices

Mastering Suite is GPL-3.0-or-later.

## hearth-dsp

Filters, ADAA shaping, oversampling, spectrum analysis, true-peak detection and
other shared DSP are consumed from `hearth-dsp` (GPL-3.0-or-later), extracted
from Comp Suite. See `third_party/hearth-dsp/THIRD_PARTY_NOTICES.md`.

## libebur128

Loudness, loudness range and true-peak metering use `libebur128` 1.2.6 by
Jiří Žůrek, MIT licensed. It is linked dynamically and must be installed on the
target system (`libebur128.so.1`).

Copyright (c) 2011-2020 Jiří Žůrek and contributors.
MIT License: https://github.com/jiixyj/libebur128/blob/master/COPYING

## CLAP

The CLAP headers in `third_party/clap` are MIT licensed.

Copyright (c) 2021 Alexandre BIQUE.
See `third_party/clap/LICENSE.md`.

## Jost typeface

The panel uses Jost ExtraBold, licensed under the SIL Open Font License 1.1.
See `assets/fonts/OFL.txt`.

Copyright 2020 The Jost Project Authors
(https://github.com/indestructible-type).

## Model references

The five colour models are original transfer shapes written for this plugin.
They are informed by the general behaviour of console, bus, tape and
transformer saturation, not by any proprietary schematic, curve or product
name. If MIT-licensed JClones saturator code is ported in a later version, its
notice and attribution will be added here with the implementation.

The shared dynamics and ratio ladder come from hearth-dsp and Comp Suite. No
JSFX source is copied.

Algorithm provenance: RBJ biquad cookbook, first-order ADAA (Parker,
Zavalishin, Le Bivic), Kaiser-windowed half-band filtering, Catmull-Rom
interpolation.
