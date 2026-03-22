# Signal Analysis Workflow

This document records the current end-to-end workflow for:

- reading raw `.map` or `.dat` capture files with `readDat`
- generating segmented `.mat` files in `data`
- visualizing the generated data with `nr_showTimeData`

## Directory roles

- `signalAnalysis/tools`: shared MATLAB tools such as `readDat.m`
- `signalAnalysis/base`: basic analysis and visualization scripts such as `nr_showTimeData.m`
- `signalAnalysis/data`: raw capture files, generated `.mat` files, and exported figures

## Workflow 1: read raw capture data with `readDat`

Place the source file in `signalAnalysis/data`, then run MATLAB from that directory or switch to that directory before calling `readDat`.

Example command:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');
readDat("20260213001_syncRx_xnr-sxr-01.map", 1, 4096*15*20*2, 0*4, "int16=>double", 10, 0, 1);
```

This command reads the raw file and generates:

```text
20260213001_syncRx_000_00000.mat
20260213001_syncRx_000_00001.mat
...
20260213001_syncRx_000_00009.mat
```

In the current workflow, one generated `.mat` file corresponds to one frame of data.

## `readDat` parameter notes

Function signature:

```matlab
readDat(filename, flag, length, offsetlen, precision, cnt, frameIdx, dataFormat)
```

Key parameters:

- `filename`: raw input filename. This name directly affects the output `.mat` filenames and variable name.
- `flag`: `0` means real data, `1` means IQ complex data.
- `length`: number of values read per block.
  For complex IQ data, the raw file is read as interleaved I/Q values, so this should usually be `2 * number_of_complex_samples`.
  If one output `.mat` is intended to represent one frame, then `length` should be chosen to match one frame of raw IQ data.
  In the current `122.88 MHz` workflow with complex IQ input, one 10 ms frame contains `4096 * 15 * 20 = 1228800` complex samples, so `length` should be `4096 * 15 * 20 * 2 = 2457600` when the raw file stores `I, Q, I, Q...` as `int16`.
- `offsetlen`: byte offset before reading starts.
- `precision`: raw sample type such as `"int16=>double"` or `"float=>double"`.
- `cnt`: number of output `.mat` files to generate. `cnt=10` generates 10 files.
- `frameIdx`: starting frame index for naming.
- `dataFormat`: output frame-numbering mode.
  `0` keeps the 5G NR 1024-frame cycle structure in the filename, so the middle 3-digit field is the superframe index and the last 5-digit field is the frame index within that 1024-frame cycle.
  `1` uses continuous frame numbering, so the middle 3-digit field is fixed to `000` and the last 5-digit field keeps increasing across 1024-frame boundaries.

Recommended default practice:

- raw `.dat` or `.map` files may be very large, so if there is no special requirement, do not read the whole file first
- when not otherwise specified, start with only about 10 to 20 frames
- a practical default choice is `cnt = 10` or `cnt = 16`
- use `cnt = 0` only when you really want to read the whole file

Example:

- With `dataFormat = 0`, frame 0 of the second 1024-frame cycle is named like `..._001_00000.mat`.
- With `dataFormat = 1`, the same position is named continuously after the first cycle, so it follows frame 1023 directly in numbering.

For large files, a safe first pass is:

```matlab
readDat(filename, 1, lengthPerFrame, 0, "int16=>double", 10, 0, 1);
```

After confirming the data format, signal quality, and naming are correct, increase `cnt` if you need more frames.

## Optional advanced workflow: align frame head before regenerating `.mat`

This is an additional workflow only for cases where you really need frame-head-aligned `.mat` files.

Use this workflow when:

- later decoding or measurement depends on frame origin
- the first-pass `.mat` files are known to start in the middle of a frame
- you want one generated `.mat` file to begin as close as possible to the actual radio frame head

If you only want quick waveform viewing or rough inspection, this extra alignment step is usually unnecessary.

Recommended idea:

1. Use `readDat` first to generate a small batch of preliminary `.mat` files.
2. Use `advance/decoder_0001_cellsearch.m` on a `.mat` that contains SSB.
3. Read the searched `frameOffset`.
4. Convert that sample offset into `readDat` byte offset.
5. Run `readDat` again with the adjusted `offsetlen` to regenerate aligned `.mat` files.

### Step 1: generate a preliminary batch first

Example:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');
readDat("20260305001_syncRx_zte-msg2-0305.dat", 1, 4096*15*20*2, 0, "int16=>double", 10, 0, 1);
```

### Step 2: search frame timing with `decoder_0001_cellsearch.m`

The current script can be used to estimate the frame timing from a generated `.mat` file that contains SSB.

Example result from this project:

```text
frameOffset = -9489
```

Interpretation:

- the current `.mat` file does not start exactly at frame head
- a negative value means the waveform start is already after the true frame origin
- in this case, the current waveform start is `9489` samples later than the target frame head

### Step 3: convert the searched frame offset into `offsetlen`

`readDat` uses `offsetlen` in bytes, not in samples.

In the current workflow:

- one frame has `4096*15*20 = 1228800` complex samples
- the raw file is read with `precision = "int16=>double"`
- for this project, one sample step in the searched offset is converted using `4` bytes in the raw-file offset calculation

When the searched frame offset is negative and `readDat` only accepts a non-negative `offsetlen`, a practical method is:

- move forward by one whole frame
- then move backward inside that frame by the searched amount

So for:

```text
frameOffset = -9489
```

use:

```matlab
offsetlen = (4096*15*20 - 9489) * 4;
```

### Step 4: regenerate aligned `.mat` files

Example:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');

offsetlen = (4096*15*20 - 9489) * 4;
readDat("20260305001_syncRx_zte-msg2-0305.dat", 1, 4096*15*20*2, offsetlen, "int16=>double", 10, 0, 1);
```

### Step 5: if SSB is sparse, regenerate more frames for verification

Sometimes one frame does not contain SSB. In that case, a frame-head search script may fail even when the offset correction direction is right.

For the dataset in this project, SSB appears once every 64 frames. A practical verification method was:

1. regenerate 70 frames using the adjusted `offsetlen`
2. choose a frame that is expected to contain SSB, such as `20260305001_syncRx_000_00063.mat`
3. run `decoder_0001_cellsearch.m` on that file
4. check whether `BCH CRC = 0` and whether `frameOffset` is close to zero

Example regeneration:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');

offsetlen = (4096*15*20 - 9489) * 4;
readDat("20260305001_syncRx_zte-msg2-0305.dat", 1, 4096*15*20*2, offsetlen, "int16=>double", 70, 0, 1);
```

Example verification target:

```text
20260305001_syncRx_000_00063.mat
```

Example verification result from this project:

```text
Cell identity: 314
BCH CRC: 0
frameOffset = -66
```

This means:

- the offset-adjusted regeneration direction is correct
- the waveform has been aligned very close to frame head
- a small residual offset may still remain and can be fine-tuned if needed

## Optional lightweight workflow: coarse screen which `.mat` files may contain SSB

This is another optional workflow. Use it when you want to quickly determine which generated `.mat` files are worth sending to `decoder_0001_cellsearch.m`.

This step is especially useful when:

- SSB does not appear in every frame
- `decoder_0001_cellsearch.m` may fail simply because the selected `.mat` does not contain SSB
- you want a fast pre-check before doing full cell search

The purpose of this workflow is not to fully decode SSB. It is only a lightweight coarse screening step.

### Why this screening is useful

In NR, one SSB occupies:

- 4 OFDM symbols in time
- 20 RB in frequency, which is 240 RE across one symbol

So in a time-frequency view, an SSB often appears as a bright local block with:

- short time duration
- limited frequency width
- noticeably stronger energy than the nearby background

This means we can use a lightweight MATLAB script to look for a `240 x 4` style energy block before running full cell search.

### Current lightweight script

Current script:

- `signalAnalysis/advance/check_ssb_presence.m`

This script performs a coarse screening by:

1. loading a generated `.mat` file
2. OFDM demodulating it into a grid using the configured `nrb`, `scs`, and `fftNum`
3. sliding a `240 x 4` window over the power grid
4. finding the brightest local block
5. comparing that block against the background energy
6. outputting a coarse metric and a preliminary `hasSSB` decision

This is only a coarse detector, but it is fast and works well as a pre-filter.

### Parameters that must still be correct

Even for this lightweight script, these parameters still need to match the actual signal:

- the selected `.mat` file or frame range
- `rxSampleRate`, which is determined by `fftNum * scs * 1e3`
- `nrb`
- `scs`
- `fftNum`

If these are wrong, the coarse screening result can also be misleading.

### Example batch screening

Example:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/advance');
addpath('D:/work/codex/github/codex/signalAnalysis/base');
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
addpath('D:/work/codex/github/codex/signalAnalysis/data');
cd('D:/work/codex/github/codex/signalAnalysis/data');

captureId = "20260305001";
variableName = "syncRx";
superFrameIdx = 0;
frameRange = 0:69;
nrb = 51;
scs = 30;
fftNum = 4096;

results = check_ssb_presence;
```

The script returns a table containing fields such as:

- `frameIdx`
- `fileName`
- `coarseMetricDb`
- `timeContrastDb`
- `freqContrastDb`
- `peakSubcarrier`
- `peakSymbol`
- `hasSSB`

### Example result from this project

For:

```text
20260305001_syncRx_000_00000.mat
...
20260305001_syncRx_000_00069.mat
```

the coarse screening marked these frames as likely containing SSB:

- `20260305001_syncRx_000_00000.mat`
- `20260305001_syncRx_000_00063.mat`
- `20260305001_syncRx_000_00064.mat`

In this project, those frames had coarse metrics around `33 dB`, while most non-SSB frames were only around `1 dB`.

### How to use the coarse screening result

Recommended use:

1. run `check_ssb_presence.m` on a batch of generated `.mat` files
2. sort or inspect the frames with the highest `coarseMetricDb`
3. pick those frames as candidates for `decoder_0001_cellsearch.m`
4. use the full cell search result to confirm whether the frame really contains SSB

Important caution:

- `hasSSB = true` only means the frame is a strong SSB candidate
- final confirmation should still rely on `decoder_0001_cellsearch.m`, such as checking `Cell identity`, `BCH CRC`, and `frameOffset`
- if two adjacent frames are both marked, it may mean the SSB is near a frame boundary and the current frame alignment still has a small residual error

### Practical method: estimate `FrequencyOffset` from `peakSubcarrier` before checking `frameOffset`

When `check_ssb_presence.m` has already identified likely SSB frames, a practical next step is to use the reported `peakSubcarrier` to set a better initial `FrequencyOffset` for `decoder_0001_cellsearch.m`.

This is useful because:

- `decoder_0001_cellsearch.m` currently applies a manual frequency shift before SSB search
- if that initial `FrequencyOffset` is too far from the real SSB location, later `BCH CRC` may fail even when the frame really contains SSB
- a better initial `FrequencyOffset` usually makes the later `frameOffset` result much more reliable

Recommended idea:

1. Run `check_ssb_presence.m` first.
2. Pick one or more strong SSB candidate frames.
3. Read `peakSubcarrier` from the coarse-screening result.
4. Convert that position into the SSB center frequency offset from the RF center.
5. Use that value as the initial `FrequencyOffset` in `decoder_0001_cellsearch.m`.
6. Then check `Cell identity`, `BCH CRC`, and `frameOffset`.

### How to convert `peakSubcarrier` into `FrequencyOffset`

The coarse detector uses a `240 x 4` window, so `peakSubcarrier` is the starting subcarrier index of the coarse SSB block.

For one candidate frame:

- let `peakSubcarrier` be the coarse SSB start position
- SSB width is `240` RE in frequency
- so the coarse SSB center is:

```text
ssbCenter = peakSubcarrier + 240/2
```

For the full analyzed carrier:

- the frequency-domain center of the demodulated grid is:

```text
gridCenter = nrb * 12 / 2
```

So the SSB center offset from RF center, measured in subcarriers, is:

```text
subcarrierOffset = gridCenter - ssbCenter
```

Then convert that to Hz:

```text
FrequencyOffset = subcarrierOffset * scs * 1e3
```

### Example from this project

Suppose `check_ssb_presence.m` reports:

```text
peakSubcarrier = 12
```

and the waveform parameters are:

```text
nrb = 51
scs = 30
```

Then:

```text
ssbCenter = 12 + 240/2 = 132
gridCenter = 51*12/2 = 306
subcarrierOffset = 306 - 132 = 174
FrequencyOffset = 174 * 30e3 = 5.22e6 Hz
```

So a practical choice in `decoder_0001_cellsearch.m` is:

```matlab
FrequencyOffset = 174*30e3;
```

In this project, using this value allowed `decoder_0001_cellsearch.m` to recover:

- `Cell identity = 1`
- `BCH CRC = 0`
- `frameOffset = 0` or very close to zero

This proved much more reliable than using a rough frequency shift that was farther away from the true SSB position.

### Practical note on `searchBW` and why `FrequencyOffset` does not need to be exact

In `decoder_0001_cellsearch.m`, `searchBW` is the SSB frequency-search range used by the later synchronization process.

That means:

- `FrequencyOffset` is only the initial shift that moves the SSB near the center
- `searchBW` still allows the script to search around that initial position

So `FrequencyOffset` does not need to be perfectly exact, as long as the remaining error is still inside the later search range.

Practical rule:

- when the SSB location is still uncertain, use a larger `searchBW`
- once one SSB has already been decoded successfully, update `FrequencyOffset` using the decoded or coarse-screened position
- after that, reduce `searchBW` to speed up later searches
- if the SSB position is already known accurately enough, `searchBW` can be set very small, or even `0`

This creates a practical two-stage method:

1. coarse search with a larger `searchBW`
2. refined search with improved `FrequencyOffset` and a smaller `searchBW`

This usually gives a better balance between:

- search robustness
- search time
- frame-head verification reliability

## Filename convention for raw files

`readDat.m` uses:

```matlab
strcell = strsplit(filename,'_');
varname = strcell{2};
dataname = strcell{1};
```

That means the raw filename should preferably follow this format:

```text
<timeSequenceId>_<variableName>_<tags>.map
```

Recommended example:

```text
20260213001_syncRx_xnr-sxr-01.map
```

Why this matters:

- `<timeSequenceId>` becomes the prefix of every generated `.mat` filename
- `<variableName>` becomes the MATLAB variable stored in each `.mat`
- `<tags>` can preserve source and issue context for later lookup
- later scripts may depend on this variable name

For the example above:

- `dataname = 20260213001`
- `varname = syncRx`
- generated files are named like `20260213001_syncRx_000_00001.mat`
- each `.mat` contains a variable named `syncRx`

## Naming recommendations

To make later processing easier, use these rules:

- Keep the first field as a time-based sequence ID.
- A time sequence ID makes large batches of files sort naturally by filename, which keeps the dataset visually tidy and makes batch review easier.
- A practical format is `YYYYMMDDNNN`.
- `YYYYMMDD` should follow local time, so the filename date stays aligned with the actual local analysis or capture date.
- The trailing `NNN` is the sequence number for that local date, such as the first, second, or third dataset handled that day.
- Keep the second field stable and meaningful, such as `syncRx`, `txWaveform`, `rxWaveform`, or `pdschRx`.
- Put source information, environment information, exception summaries, or issue tags in the third field and later fields.
- The tags at the end may look unimportant at first, but they are very useful later when you need to quickly identify where the file came from or what problem was seen during capture.
- Do not omit the second field, because `readDat` uses it as the saved MATLAB variable name.
- Avoid spaces and avoid special characters in the variable-name field.
- Keep the tags readable and concise so that filename sorting still stays clean.

Recommended examples:

```text
20260213001_syncRx_xnr-sxr-01.map
20260213002_rxWaveform_lab-test-01.dat
20260213003_pdschRx_gnb-A-ue-02.map
20260213004_syncRx_ru1-lowpower-ulweak.map
20260213005_syncRx_fieldA-clockdrift.map
```

Example interpretation:

```text
20260309001_syncRx_sinr19-0309.dat
```

- `20260309` means the local date is March 9, 2026
- `001` means this is the first dataset handled on that local date
- `syncRx` is the MATLAB variable name that will be saved into the generated `.mat`
- `sinr19-0309` is the tag suffix used for source or issue tracking

For packet captures that belong to the same acquisition batch, use the same time sequence ID and the same tag suffix so that the files line up naturally when sorted:

```text
20260309001_syncRx_sinr19-0309.dat
20260309001_pcap_sinr19-0309.pcap
```

Less suitable examples:

```text
test.map
20260213001_xnr-sxr-01.map
20260213001_sync-Rx_xnr-sxr-01.map
```

Problems with these names:

- too few underscore-separated fields
- unstable or unclear variable names
- variable names that are inconvenient to use in MATLAB

## How to handle a non-standard filename first

Sometimes incoming files do not follow the standard naming rule. In that case, rename the file before calling `readDat`.

Example input:

```text
sinr_19_0309.dat
```

This name is not suitable for the workflow because:

- the first field is not a sortable time sequence ID
- the second field would become `19`, which is not a meaningful MATLAB variable name for later analysis
- the filename does not clearly preserve source meaning in a reusable way

A better renamed version is:

```text
20260309001_syncRx_sinr19-0309.dat
```

Why this renamed version works better:

- `20260309001` is a time-sequence-style prefix, so files sort neatly in chronological order
- `syncRx` becomes the saved MATLAB variable name inside each generated `.mat`
- `sinr19-0309` is kept as a lightweight tag to preserve origin and context

Suggested processing steps:

```matlab
% Step 1: rename the raw file outside MATLAB or with MATLAB file operations
% sinr_19_0309.dat -> 20260309001_syncRx_sinr19-0309.dat

% Step 2: read the renamed file
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');
readDat("20260309001_syncRx_sinr19-0309.dat", 1, 4096*15*20*2, 0*4, "int16=>double", 40, 0, 1);
```

For this file, `cnt = 40` is appropriate because the file size matches 40 complete blocks under the current read settings.

## Why this naming style is useful

- The first field is a time sequence number, so when many capture files are placed in one directory they sort in chronological order automatically.
- This keeps the raw files, generated `.mat` files, and exported figures looking much cleaner during large-scale analysis.
- The last part of the filename can carry lightweight tags such as source module, test site, abnormal behavior, or problem keywords.
- Those tags make it much faster to trace a suspicious dataset back to its origin without opening the file first.
- A filename that looks verbose can still save a lot of time during debugging, regression comparison, and issue review.
- If the `.dat/.map` file and the `.pcap` file share the same time sequence ID and tag suffix, it becomes much easier to cross-check waveform data and signaling data side by side.

## How to pair raw data and `pcap` naming

For one acquisition batch, it is recommended that the raw IQ file and the packet capture file use the same:

- time sequence ID
- tag suffix

Only the middle field changes to describe the file role.

Recommended pattern:

```text
<timeSequenceId>_syncRx_<tags>.dat
<timeSequenceId>_pcap_<tags>.pcap
```

Example:

```text
20260309001_syncRx_sinr19-0309.dat
20260309001_pcap_sinr19-0309.pcap
```

This pairing is useful because:

- sorting by filename keeps the matching `.dat` and `.pcap` files adjacent or very close
- the shared prefix makes it obvious that both files come from the same acquisition event
- the shared tag makes it easier to match waveform anomalies with signaling context

Suggested handling for a new batch:

1. Rename the raw IQ file to the standard format.
2. Rename the corresponding `pcap` file to the same time sequence ID and tag suffix.
3. Run `readDat` on the raw IQ file.
4. Extract `scs` and `nrb` from the paired `pcap`.
5. Use those parameters in `nr_showTimeData`.

## Workflow 2: visualize generated data with `nr_showTimeData`

Current script:

- `signalAnalysis/base/nr_showTimeData.m`

Typical usage:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/base');
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
addpath('D:/work/codex/github/codex/signalAnalysis/data');
cd('D:/work/codex/github/codex/signalAnalysis/data');
nr_showTimeData
```

The script now supports configurable input naming through workspace variables. If you do not provide them, it falls back to the old default example.

Configurable usage:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/base');
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
addpath('D:/work/codex/github/codex/signalAnalysis/data');
cd('D:/work/codex/github/codex/signalAnalysis/data');

captureId = "20260309001";
variableName = "syncRx";
superFrameIdx = 0;
frameRange = 1;
nrb = 51;
scs = 30;
fftNum = 4096;
frameOffset = 0;
nr_showTimeData
```

The script loads data in this form:

```matlab
filename = sprintf("%s_%s_%03d_%05d.mat", captureId, variableName, superFrameIdx, frameIdx);
dataStruct = load(filename);
rxWaveform = dataStruct.(variableName);
```

So the current script assumes:

- the `.mat` file lives in `signalAnalysis/data`
- the file name matches `%captureId%_%variableName%_%03d_%05d.mat`
- the `.mat` file contains a variable whose name matches `variableName`

In practice, `nr_showTimeData.m` is not a locked script. You can either:

- set the required workspace variables before calling it
- or directly edit the default values inside `nr_showTimeData.m` for the current dataset

Directly editing `nr_showTimeData.m` is acceptable in the current workflow when you want the script itself to open a specific dataset by default.

## How to determine downlink `scs` and `nrb` from `pcap`

When both uplink and downlink configurations are present, we usually default to analyzing downlink data.

For NR SIB1, the downlink parameters should be read from:

```text
SIB1
  -> servingCellConfigCommon
  -> downlinkConfigCommon
  -> frequencyInfoDL
  -> scs-SpecificCarrierList
```

Key fields:

- `subcarrierSpacing`: downlink SCS
- `carrierBandwidth`: downlink NRB count

Do not use the uplink path when your target is downlink waveform analysis:

```text
uplinkConfigCommon
  -> frequencyInfoUL
```

That path is for uplink configuration.

Example from the current capture:

- downlink `subcarrierSpacing = kHz30`
- downlink `carrierBandwidth = 51`

So the recommended analysis parameters are:

- `scs = 30`
- `nrb = 51`

## Example `pcap` extraction flow

Associated capture file:

```text
20260309001_pcap_sinr19-0309.pcap
```

You can inspect it in Wireshark, or use `tshark` from the local Wireshark installation.

Manual inspection path in Wireshark:

```text
SIB1
  -> servingCellConfigCommon
  -> downlinkConfigCommon
  -> frequencyInfoDL
  -> scs-SpecificCarrierList
```

Useful `tshark` approach:

```powershell
& 'C:\Program Files\Wireshark\tshark.exe' `
  -r 'D:\work\codex\github\codex\signalAnalysis\data\20260309001_pcap_sinr19-0309.pcap' `
  -Y "nr-rrc.SIB1_element" `
  -V
```

Then look for:

- `subcarrierSpacing: kHz30 (1)`
- `carrierBandwidth: 51`

## Sample rate and `fftNum` guidance

The RF sampling rate is commonly `122.88 MHz` in the current workflow.

Under this sampling rate:

- if `scs = 30 kHz`, use `fftNum = 4096`
- if `scs = 120 kHz`, use `fftNum = 1024`

This follows:

```text
rxSampleRate = fftNum * scs * 1e3
```

Example checks:

- `4096 * 30e3 = 122.88e6`
- `1024 * 120e3 = 122.88e6`

## `nr_showTimeData` parameter notes

Important parameters in the current script:

- `nrb = 51`: number of resource blocks for OFDM demodulation
- `scs = 30`: subcarrier spacing in kHz
- `fftNum`: FFT size, chosen together with `scs` to match the RF sample rate
- `rxSampleRate = fftNum * scs * 1e3`: sampling rate
- `frameOffset = 0`: time-domain shift before OFDM demodulation
- `frameIdx`: selects which generated `.mat` file to load
- `captureId`: identifies which capture batch to visualize
- `variableName`: identifies which variable to read from the `.mat` file
- `superFrameIdx`: the middle three-digit field in the generated filename, representing which 1024-frame cycle the data belongs to
- `frameRange`: which frame index or indices to plot

Parameters that usually need to be checked or modified before running `nr_showTimeData.m`:

- `captureId`: must match the generated `.mat` filename prefix
- `variableName`: must match the variable saved inside the `.mat`
- `superFrameIdx`: usually `0` when `readDat(..., dataFormat=1)` is used
- `frameRange`: choose which frame or frames to inspect
- `nrb`: determine this from the paired `pcap`, usually from downlink SIB1
- `scs`: determine this from the paired `pcap`, usually from downlink SIB1
- `fftNum`: choose this to match the RF sample rate together with `scs`
- `frameOffset`: adjust this when the waveform needs a time-domain shift before OFDM demodulation

These parameters must match the actual captured signal. If they do not match:

- the time-domain plot may still look reasonable
- the time-frequency grid may be blurred, shifted, or incorrect

Recommended starting rule:

- first use `pcap` to determine downlink `scs` and `nrb`
- then choose `fftNum` from the RF sample rate relationship
- then make sure `captureId`, `variableName`, `superFrameIdx`, and `frameRange` match the generated `.mat` files

For the common `122.88 MHz` downlink case in this project:

- set `scs = 30`
- set `nrb = 51`
- set `fftNum = 4096`
- make sure each `.mat` frame was generated with `readDat(..., 4096*15*20*2, ...)`

## Practical cautions

- `nr_showTimeData.m` depends on MATLAB 5G Toolbox functions such as `nr_OFDMInfo` and `nr_OFDMDemodulate`.
- Run it from `signalAnalysis/data`, or make sure the `data` directory is on the MATLAB path.
- The script now supports configurable `captureId` and `variableName`, but the generated `.mat` naming still depends on the raw filename passed into `readDat`.
- If the second field in the raw filename is not chosen carefully, later MATLAB loading and plotting become less readable.
- `readDat` reads one block and saves one `.mat` file each cycle, so choose `length` and `cnt` carefully before processing very large files.
- If you want one `.mat` file to correspond to one full 10 ms frame at `122.88 MHz`, do not use `20*1024*15*2`; that only yields `307200` complex samples per file. Use `4096*15*20*2` so that each `.mat` contains `1228800` complex samples.
- For complex IQ data, confirm that the raw file really stores interleaved `I, Q, I, Q...` samples before using `flag = 1`.
- If you want to preserve the original NR 1024-frame cycle information, use `dataFormat = 0`.
- If you want frame numbering to stay continuous across cycle boundaries, use `dataFormat = 1`.

## MATLAB startup issue seen in this project

During actual processing on this machine, MATLAB did not start cleanly on the first try.

Observed symptoms:

- one run reported `Fatal Startup Error` with `failed to load settings errors_warnings plugin`
- later runs could print `Unable to load ApplicationService for command client-v1`

What worked:

- create a temporary clean MATLAB preferences directory
- set `MATLAB_PREFDIR` to that directory before launching MATLAB
- then run MATLAB in `-batch` mode

Example PowerShell command:

```powershell
$env:MATLAB_PREFDIR='D:\work\codex\github\.matlab_prefs'
New-Item -ItemType Directory -Force -Path $env:MATLAB_PREFDIR | Out-Null

& 'D:\Program\R2024a\bin\matlab.exe' -batch "disp('MATLAB_OK'); disp(version);"
```

Practical note:

- the `ApplicationService` warnings did not block the actual batch processing in this project
- setting a clean `MATLAB_PREFDIR` avoided the startup failure and allowed `readDat` and `nr_showTimeData` to run successfully

## Recommended end-to-end example

1. Put the raw file in `signalAnalysis/data`:

```text
20260213001_syncRx_xnr-sxr-01.map
```

2. Generate segmented `.mat` files:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');
readDat("20260213001_syncRx_xnr-sxr-01.map", 1, 4096*15*20*2, 0*4, "int16=>double", 10, 0, 1);
```

3. Visualize one frame:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/base');
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
addpath('D:/work/codex/github/codex/signalAnalysis/data');
cd('D:/work/codex/github/codex/signalAnalysis/data');

captureId = "20260213001";
variableName = "syncRx";
superFrameIdx = 0;
frameRange = 0;
nrb = 51;
scs = 30;
fftNum = 4096;
frameOffset = 0;
nr_showTimeData
```

4. Expected data linkage:

- raw file: `20260213001_syncRx_xnr-sxr-01.map`
- generated `.mat`: `20260213001_syncRx_000_00001.mat`
- variable inside `.mat`: `syncRx`
- visualization script loads `syncRx` and plots the waveform

## Example for a renamed non-standard file

1. Original incoming file:

```text
sinr_19_0309.dat
```

2. Rename it first:

```text
20260309001_syncRx_sinr19-0309.dat
```

3. Rename the paired packet capture in the same style:

```text
20260309001_pcap_sinr19-0309.pcap
```

4. Generate segmented `.mat` files:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
cd('D:/work/codex/github/codex/signalAnalysis/data');
readDat("20260309001_syncRx_sinr19-0309.dat", 1, 4096*15*20*2, 0*4, "int16=>double", 40, 0, 1);
```

5. Read downlink `scs` and `nrb` from the paired `pcap`:

```powershell
& 'C:\Program Files\Wireshark\tshark.exe' `
  -r 'D:\work\codex\github\codex\signalAnalysis\data\20260309001_pcap_sinr19-0309.pcap' `
  -Y "nr-rrc.SIB1_element" `
  -V
```

- downlink `subcarrierSpacing = kHz30`
- downlink `carrierBandwidth = 51`

6. Visualize the first frame:

```matlab
addpath('D:/work/codex/github/codex/signalAnalysis/base');
addpath('D:/work/codex/github/codex/signalAnalysis/tools');
addpath('D:/work/codex/github/codex/signalAnalysis/data');
cd('D:/work/codex/github/codex/signalAnalysis/data');

captureId = "20260309001";
variableName = "syncRx";
superFrameIdx = 0;
frameRange = 0;
nrb = 51;
scs = 30;
fftNum = 4096;
frameOffset = 0;
nr_showTimeData
```

7. Expected data linkage:

- raw file: `20260309001_syncRx_sinr19-0309.dat`
- paired `pcap`: `20260309001_pcap_sinr19-0309.pcap`
- generated `.mat`: `20260309001_syncRx_000_00001.mat`
- variable inside `.mat`: `syncRx`
- optional exported figure: `nr_showTimeData_20260309001_syncRx_000_00001.png`

## Suggested next improvement

The next useful step would be to refactor `nr_showTimeData.m` from a script into a callable function so that the filename, variable name, and radio parameters can be passed in directly as function inputs.
