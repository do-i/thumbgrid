# Video Controls UI Plan

## Requirements

- Show the current volume percentage inside the volume popup.
- Make the volume, speed, and playback progress slider bars twice as tall.
- Make the volume and speed popup sliders 1.5x wider.
- Add a reset button in the speed popup that sets playback speed to `1.00x`.
- Let the playback progress bar fill the remaining width in the main video controls.
- Commit in small checkpoints while implementing.

## Implementation Steps

- Commit this plan file first.
- Keep the existing slider visibility/color fix and include it in the first UI implementation commit.
- Add a volume percentage label to the volume popup and keep it synchronized with slider changes and external volume updates.
- Resize popup sliders from 120px wide to 180px wide and adjust popup button widths to match.
- Increase the styled horizontal slider track/fill height from 4px to 8px, and adjust the handle height/margins so the controls remain usable.
- Add a reset button to the speed popup that updates the slider to `100`, updates labels, and emits `playbackSpeedChanged(1.0)` through the existing slider path.
- Update the `.ui` metadata for the seek bar so it is not fixed to 200px and can expand with the runtime layout.
- Build the app and run stylesheet/diff hygiene checks.

## Verification

- `git diff --check`
- `./run.sh` then choose `b`
- Inspect the relevant code paths:
  - `VideoControlVolumeSlider`
  - `VideoControlSpeedSlider`
  - `VideoControlSeekSlider`
  - speed reset button signal path
  - volume label update path
