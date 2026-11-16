// Global UI layout metrics for the AeroSimWorkbench main window and the
// feature pages it hosts.
//
// Purpose
// -------
// Every module used to carry its own hard-coded window sizes, margins, panel
// widths and screen-resolution lookups, so pages diverged across different
// resolutions and the main window could be overwritten by an unrelated page.
// This header plus its implementation (UiLayoutConfig.cpp) are the single
// source of truth for the shared layout numbers and the screen/DPI adapters.
// Versioned pages keep their own internal structure but read the same
// margin/spacing/min-size/panel-ratio values and the same screen helper, so the
// whole application scales and adapts consistently.
//
// The three layers
// ----------------
// The parameters are intentionally grouped into three layers:
//
//   1. Window layer    - geometry of the main window (or truly top-level
//                        windows). Owned by MainWindow; feature pages must NOT
//                        apply these to themselves.
//   2. Common layer    - the shared margin/spacing/height tokens that feature
//                        pages read to keep a uniform typography and rhythm.
//   3. Constraint layer- dock minimums, panel widths and default ratios that
//                        the MainWindow layout manager uses.
//
// Units & reference
// -----------------
// All pixel sizes are Qt logical pixels. 1920x1080 is only the *design*
// reference for reasoning about proportions; the running geometry is driven by
// the current screen's available rectangle (see availableRect/screenOf), so a
// smaller or larger display adapts rather than being forced to a fixed size.
//
// UiLayoutConfig is NOT a runtime state container. It never stores the current
// size of a page or reads back a user-dragged dock width; those are runtime
// state owned by MainWindow and its QSettings persistence. The functions here
// only return defaults, shared rules and safe-value helpers.
//
// This unit is intentionally dependency-light (it never pulls in Widgets UI
// code), so any UI module can include it freely.
#pragma once

#include <QMargins>
#include <QScreen>
#include <QSize>
#include <QtGlobal>

class QWidget;
class QVBoxLayout;

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

// =============================================================================
// Layer 1 - Window geometry
// Owned by MainWindow and other real top-level windows. Central feature pages
// MUST NOT call these to size themselves.
// -----------------------------------------------------------------------------
// Default window geometry in Qt logical px. Applied by MainWindow on creation.
inline constexpr QSize defaultWindowSize()
{
    return QSize(detail::kDefaultWindowWidth, detail::kDefaultWindowHeight);
}
// Lower bound for the main window, in Qt logical px. Applied via
// setMinimumSize; keeps the central workspace usable at small resolutions.
inline constexpr QSize minimumWindowSize()
{
    return QSize(detail::kMinimumWindowWidth, detail::kMinimumWindowHeight);
}
// Design reference used purely to reason about proportions; never applied as a
// fixed runtime geometry. Scope: MainWindow layout math.
inline constexpr QSize designReferenceSize()
{
    return QSize(detail::kDesignWidth, detail::kDesignHeight);
}

// =============================================================================
// Layer 2 - Common layout tokens
// Read by feature pages to keep a uniform visual rhythm. Safe for any page to
// call. All values are Qt logical px.
// -----------------------------------------------------------------------------
// Content margin applied to the root layout of central feature pages. The page
// root is responsible for applying this margin exactly once; the central
// container does not add it again (see UI-Development-Guidelines.md §3).
// Scope: default page root. Page-usable: yes.
inline constexpr QMargins pageMargins()
{
    return QMargins(24, 20, 24, 20);
}
// Spacing between major blocks / sections inside a page.
// Scope: section separators. Page-usable: yes.
inline constexpr int sectionSpacing() { return 16; }
// Spacing between control rows (forms, toolbars, button rows). The default
// rhythm for stacked controls. Scope: page internals. Page-usable: yes.
inline constexpr int panelSpacing() { return 12; }
// Compact spacing between tightly grouped controls. Use for closely related
// controls (e.g. label + field, paired buttons). Page-usable: yes.
inline constexpr int controlSpacing() { return 8; }
// Baseline height kept for inputs and list rows so pages look consistent.
// Use as a minimum/ideal height hint, not a hard fixed size. Page-usable: yes.
inline constexpr int controlHeight() { return 28; }
// Height of section/panel title bars. Scope: page section headers.
// Page-usable: yes.
inline constexpr int sectionTitleHeight() { return 28; }

// =============================================================================
// Layer 3 - Dock constraints and default ratios
// Read by the MainWindow layout manager. Feature pages must NOT apply these to
// the global docks. Values are Qt logical px (ratios are unitless fractions).
// -----------------------------------------------------------------------------
// Default and minimum width of the left workspace-tree dock. MainWindow-only.
inline constexpr int projectDockWidth() { return 260; }
inline constexpr int projectDockMinimumWidth() { return 220; }
// Property editor dock widths (left, split beside the workspace tree).
// MainWindow-only.
inline constexpr int propertyDockMinimumWidth() { return 180; }
inline constexpr int propertyDockDefaultWidth() { return 330; }
// Global output dock heights. MainWindow-only.
inline constexpr int outputExpandedHeight() { return 140; }
inline constexpr int outputCollapsedHeight() { return 40; }
// Compact width reserved for the workspace tree while an embedded page (e.g. a
// parameter editor) hides the property dock. MainWindow-only.
inline constexpr int embeddedPageDockWidth() { return 260; }

// Default panel ratios used when (re)building the default layout. They are
// fractions of the window width/height, so they scale with the screen.
inline constexpr double defaultProjectDockRatio() { return 0.18; }
inline constexpr double defaultPropertyDockRatio() { return 0.23; }
inline constexpr double defaultOutputHeightRatio() { return 0.15; }
inline constexpr double defaultExpandedOutputRatio() { return 0.15; }

// Safe ranges (min..max) for ratios that are read back from saved user layouts.
// A persisted ratio outside these is treated as corrupt and clamped.
constexpr double kProjectDockRatioMin = 0.12;
constexpr double kProjectDockRatioMax = 0.35;
constexpr double kPropertyDockRatioMin = 0.18;
constexpr double kPropertyDockRatioMax = 0.38;
constexpr double kOutputRatioMin = 0.10;
constexpr double kOutputRatioMax = 0.45;

// Clamp an arbitrary ratio into the shared safe range. Use for any
// user-persisted ratio before applying it, so a corrupt or hostile value can
// never shrink a panel to zero or blow it past the screen.
inline constexpr double clampRatio(double value, double lo = 0.10, double hi = 0.45)
{
    return qBound(lo, value, hi);
}
// Specialized clamps with the documented per-dock safe ranges.
inline constexpr double clampProjectDockRatio(double value)
{
    return clampRatio(value, kProjectDockRatioMin, kProjectDockRatioMax);
}
inline constexpr double clampPropertyDockRatio(double value)
{
    return clampRatio(value, kPropertyDockRatioMin, kPropertyDockRatioMax);
}
inline constexpr double clampOutputRatio(double value)
{
    return clampRatio(value, kOutputRatioMin, kOutputRatioMax);
}

// =============================================================================
// Screen / DPI adapters (implementation in UiLayoutConfig.cpp)
// -----------------------------------------------------------------------------
// The screen a widget currently lives on. For an embedded page this is its
// parent window's screen; for an independent dialog it prefers the screen of
// the parent window when available. Falls back to the primary screen, or
// nullptr when no windowing system is present.
QScreen *screenOf(const QWidget *widget);

// The screen's usable (avail-to-clock-area) rectangle in logical px. Dividers
// and window geometry should derive from this instead of a hard-coded size.
// Falls back to the 1920x1080 design reference when no screen is available.
QRect availableRect(const QWidget *widget);

// Logical DPI of the screen a widget is on (fallback 96). All values are in
// Qt logical px; this is informational and must not be combined with Qt's own
// automatic high-DPI scaling to double the scaling path.
qreal logicalDpi(const QWidget *widget);

// Reference scaling factor (logical DPI relative to a 96-DPI base). Prefer
// layout (Qt already scales fonts via its DPI system) over multiplying widget
// sizes; this is only for rare pixel-perfect targets.
qreal scaleFactor(const QWidget *widget);

// Create the standard root layout for a central feature page: a QVBoxLayout
// with pageMargins() applied. This is the single, framework-owned way pages set
// their root margin so the central container and the page never both add the
// same margin. The caller owns the returned layout and must assign it to page.
QVBoxLayout *createPageRootLayout(QWidget *parent);

} // namespace uimetrics
} // namespace aerosim
