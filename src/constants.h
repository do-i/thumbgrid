// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Compile-time constants shared by 2+ files or bounding a user setting.
// Single-file constants stay in an anonymous namespace in that .cpp.

#pragma once

#include <QLatin1StringView>

// Durations, delays, debounces and timeouts (…Ms).
namespace tg::timing {

// Fade / slide durations per role (values differ today; unified later).
inline constexpr int kPanelSlideMs = 230;      // slide panels, controls overlay fade
inline constexpr int kConfirmFadeMs = 200;     // save-confirm dim, move-progress fade
inline constexpr int kMessageFadeMs = 300;     // floating message, zoom indicator fade

// On-screen message lengths; call sites pick a tier instead of a number.
inline constexpr int kMessageShortMs = 600;    // transient state echo
inline constexpr int kMessageNormalMs = 1500;  // ordinary notice
inline constexpr int kMessageLongMs = 4000;    // errors, startup notices

} // namespace tg::timing

// Pixel sizes, margins and scale factors (…Px).
namespace tg::size {

// Image hash decode target: larger files are decoded reduced to this edge.
inline constexpr int kHashDecodeSizePx = 256;

} // namespace tg::size

// Counts, thread pools and other non-setting caps.
namespace tg::limit {

// Entries examined when checking a folder for convertible images.
inline constexpr int kConvertibleScanMaxEntries = 1000;

} // namespace tg::limit

// Colors as constexpr QRgb (…Rgb); build a QColor at the use site.
namespace tg::color {}

// Format strings and fixed literals.
namespace tg::fmt {

// Qt reads PDF as an image format; it is never listed or scanned as an image.
inline constexpr const char *kPdfFormat = "pdf";

// Scratch dir prefixes under the cache root and the lock file inside them; the
// startup sweep in Settings::setupCache() matches what Core and the thumbnailer create.
inline constexpr QLatin1StringView kVideoThumbDirPrefix{"videothumb-"};
inline constexpr QLatin1StringView kExportDirPrefix{"export-"};
inline constexpr QLatin1StringView kScratchLockName{".tg-lock"};

// Theme palette file in the config dir, and the always-embedded default shortcut preset.
inline constexpr QLatin1StringView kThemeFileName{"theme.ini"};
inline constexpr QLatin1StringView kDefaultPresetId{"qimgv"};

// One HEIF-family container, several suffixes (the plugin wants the file's own key).
inline constexpr const char *kHeifSuffixes[] = {"heic", "heif", "hif", "avci", "hej2"};

} // namespace tg::fmt

// Min / max / default (and slider scale or step) of user settings. Settings
// getters and setters clamp with these; dialogs set widget ranges from them.
namespace tg::bounds {

// Filmstrip thumbnail size; the settings slider shows px / scale.
inline constexpr int kPanelThumbSizeMinPx = 100;
inline constexpr int kPanelThumbSizeMaxPx = 250;
inline constexpr int kPanelThumbSizeDefaultPx = 140;
inline constexpr int kPanelThumbSizeSliderScale = 10;
static_assert(kPanelThumbSizeMinPx % kPanelThumbSizeSliderScale == 0 &&
              kPanelThumbSizeMaxPx % kPanelThumbSizeSliderScale == 0 &&
              kPanelThumbSizeDefaultPx % kPanelThumbSizeSliderScale == 0);

// Folder grid thumbnail size; zoom moves it in steps.
inline constexpr int kGridThumbSizeMinPx = 80;
inline constexpr int kGridThumbSizeMaxPx = 400;
inline constexpr int kGridThumbSizeDefaultPx = 120;
inline constexpr int kGridThumbSizeStepPx = 20;
static_assert(kGridThumbSizeMinPx % kGridThumbSizeStepPx == 0 &&
              kGridThumbSizeMaxPx % kGridThumbSizeStepPx == 0);

// --gen-thumbs-size: pre-generating cache files, wider than the grid shows.
inline constexpr int kGenThumbsSizeMinPx = 50;
inline constexpr int kGenThumbsSizeMaxPx = 400;

// Viewer zoom step, stored as a factor; the settings slider shows percent.
inline constexpr int kZoomStepMinPct = 1;
inline constexpr int kZoomStepMaxPct = 50;
inline constexpr int kZoomStepDefaultPct = 26;
inline constexpr float kZoomStepMin = kZoomStepMinPct / 100.0f;
inline constexpr float kZoomStepMax = kZoomStepMaxPct / 100.0f;
inline constexpr float kZoomStepDefault = kZoomStepDefaultPct / 100.0f;

// Duplicate finder similarity; below ~70% a 64-bit pHash distance is noise.
inline constexpr int kDuplicateSimilarityMinPct = 70;
inline constexpr int kDuplicateSimilarityMaxPct = 100;
inline constexpr int kDuplicateSimilarityDefaultPct = 87;

} // namespace tg::bounds
