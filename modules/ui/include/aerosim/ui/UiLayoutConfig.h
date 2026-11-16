// Global UI layout metrics for the AeroSimWorkbench main window and the
// feature pages it hosts.
//
// Purpose
// -------
// Every module used to carry its own hard-coded window sizes, margins, panel
// widths and screen-resolution lookups, so pages diverged across different
// resolutions and the main window could be overwritten by an unrelated page.
// This header is the single source of truth for the shared layout numbers and
// the screen/DPI adapters. Feature pages keep their own internal structure but
// all read the same margin/spacing/min-size/panel-ratio values and the same
// screen helper, so the whole application scales and adapts consistently.
//
// Units & reference
// -----------------
// All pixel sizes are Qt logical pixels. 1920x1080 is only the *design*
// reference; the running geometry is driven by the current screen's available
// rectangle (see availableRect/screenOf), so a smaller or larger display adapts
// rather than being forced to a fixed resolution.
//
// The class is intentionally header-only and dependency-light so any UI module
// can include it without dragging in Widgets-dependent code.
#pragma once

#include <QMargins>
#include <QScreen>
#include <QSize>
#include <QtGlobal>

class QWidget;

namespace aerosim {
namespace uimetrics {

namespace detail {
// Default window geometry is only valid while a screen is available; these are
// the fallbacks used before/without a windowing system.
constexpr int kDefaultWindowWidth = 1440;
constexpr int kDefaultWindowHeight = 960;
constexpr int kMinimumWindowWidth = 1000;
constexpr int kMinimumWindowHeight = 720;
constexpr int kDesignWidth = 1920;  // design reference, not a forced runtime size
constexpr int kDesignHeight = 1080;
} // namespace detail

// ------- Window -----------------------------------------------------------------
inline constexpr QSize defaultWindowSize()
{
    return QSize(detail::kDefaultWindowWidth, detail::kDefaultWindowHeight);
}
inline constexpr QSize minimumWindowSize()
{
    return QSize(detail::kMinimumWindowWidth, detail::kMinimumWindowHeight);
}
// Design reference used purely to reason about proportions; never applied as a
// fixed runtime geometry.
inline constexpr QSize designReferenceSize()
{
    return QSize(detail::kDesignWidth, detail::kDesignHeight);
}

// ------- Shared layout numbers (logical px) -----------------------------------
// Page content margin applied to the root of central feature pages.
inline constexpr QMargins pageMargins()
{
    return QMargins(24, 20, 24, 20);
}
// Spacing between major blocks / sections inside a page.
inline constexpr int sectionSpacing() { return 16; }
// Spacing between control rows (forms, toolbars, button rows).
inline constexpr int panelSpacing() { return 12; }
// Compact spacing between tightly grouped controls.
inline constexpr int controlSpacing() { return 8; }
// Baseline height used to keep inputs and list rows visually consistent.
inline constexpr int controlHeight() { return 28; }
// Height of section/panel title bars.
inline constexpr int sectionTitleHeight() { return 28; }

// ------- Dock / splitter minimums and defaults --------------------------------
// Default and minimum width of the left workspace-tree dock.
inline constexpr int projectDockWidth() { return 260; }
inline constexpr int projectDockMinimumWidth() { return 220; }
// Property editor dock widths (left, split beside the workspace tree).
inline constexpr int propertyDockMinimumWidth() { return 180; }
inline constexpr int propertyDockDefaultWidth() { return 330; }
// Global output dock heights.
inline constexpr int outputExpandedHeight() { return 140; }
inline constexpr int outputCollapsedHeight() { return 40; }
// Compact width reserved for the workspace tree while an embedded page (e.g. a
// parameter editor) hides the property dock.
inline constexpr int embeddedPageDockWidth() { return 260; }

// Default panel ratios used when (re)building the default layout. They are
// fractions of the window width/height, so they scale with the screen.
inline constexpr double defaultProjectDockRatio() { return 0.18; }
inline constexpr double defaultPropertyDockRatio() { return 0.23; }
inline constexpr double defaultOutputHeightRatio() { return 0.15; }
inline constexpr double defaultExpandedOutputRatio() { return 0.15; }
// Bound a saved ratio into a safe range before applying it.
inline constexpr double clampRatio(double value, double lo = 0.10, double hi = 0.45)
{
    return qBound(lo, value, hi);
}

// ------- Screen / DPI adapters -------------------------------------------------
// The screen a widget currently lives on (falls back to the primary screen).
QScreen *screenOf(const QWidget *widget);

// The screen's usable (avail-to-clock-area) rectangle in logical px. Dividers
// and window geometry should derive from this instead of a hard-coded size.
QRect availableRect(const QWidget *widget);

// Logical DPI of the screen a widget is on (fallback 96).
qreal logicalDpi(const QWidget *widget);

// Reference scaling factor (logical DPI relative to a 96-DPI base). Use this to
// keep explicit pixel targets legible on high-DPI displays without forcing
// widget fonts to scale manually (Qt already scales fonts via its DPI system).
qreal scaleFactor(const QWidget *widget);

} // namespace uimetrics
} // namespace aerosim
