// src/gui/ColorSwatchButton.h
#pragma once

#include <QColor>
#include <QColorDialog>
#include <QPushButton>
#include <QString>

namespace Longpath {

/// Reusable color picker button: shows a color swatch, opens QColorDialog on click.
///
/// Used across Display Setup pages (Spectrum/Waterfall/Grid colors), meter
/// editors, and future skin editor work. Replaces the dead makeColorSwatch
/// helper in DisplaySetupPages.cpp. Emits colorChanged when the user picks a
/// new color via the dialog or programmatic setColor().
///
/// Persistence helpers colorToHex/colorFromHex round-trip through the
/// AppSettings "#RRGGBBAA" string format.
class ColorSwatchButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorSwatchButton(const QColor& initial, QWidget* parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

    /// Die Optionen, mit denen der Farbwaehler aufgeht.
    ///
    /// Als benannter Wert, damit ein Pruefstand ihn lesen kann:
    /// `QColorDialog::getColor()` ist ein blockierender statischer
    /// Aufruf, den kein Test ausloesen kann, ohne selbst haengen zu
    /// bleiben -- also wird wenigstens festgenagelt, WOMIT er
    /// aufgerufen wird.
    ///
    /// DontUseNativeDialog ist kein Geschmack, sondern eine Lehre vom
    /// 2026-09-23: auf macOS nimmt Qt sonst das native NSColorPanel,
    /// und genau das stand beim Betreiber als schwarzes Fenster ohne
    /// ein einziges bedienbares Element da, waehrend getColor() seine
    /// eigene Ereignisschleife fuhr. Das Programm nahm nichts mehr an,
    /// nicht einmal Cmd+Q, nicht einmal SIGTERM -- es musste
    /// abgeschossen werden.
    static QColorDialog::ColorDialogOptions pickerOptions();

    static QString colorToHex(const QColor& c);
    static QColor colorFromHex(const QString& hex);

signals:
    void colorChanged(const QColor& c);

private slots:
    void openPicker();

private:
    void updateSwatchStyle();

    QColor m_color;
};

} // namespace Longpath
