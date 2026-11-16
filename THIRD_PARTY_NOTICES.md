# Third-party notices

Mastering Suite is GPL-3.0-or-later.

## hearth-dsp

Filters, ADAA shaping, oversampling, spectrum analysis, true-peak detection and
other shared DSP are consumed from `hearth-dsp` (GPL-3.0-or-later), extracted
from Comp Suite. See `third_party/hearth-dsp/THIRD_PARTY_NOTICES.md`.

## libebur128

Loudness, loudness range and true-peak metering use the vendored
`libebur128` 1.2.6 source in `third_party/libebur128`, built into the plugin.
It is MIT licensed and adds no runtime library dependency.

Copyright (c) 2011 Jan Kokemüller and contributors.
See `third_party/libebur128/COPYING`.

The internal `sys/queue.h` copy is BSD-2-Clause, copyright (c) 1991, 1993 The
Regents of the University of California. See the header for the full notice.

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
