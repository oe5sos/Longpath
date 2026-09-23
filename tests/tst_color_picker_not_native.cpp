// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Farbwaehler darf nicht der native sein.
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Betreiber am 2026-09-23: „kann longpath nicht schliessen", und dann,
// voellig zu Recht: „trotzdem muesste der mit command Q geschlossen
// werden".
//
// Was war: ein Farbwaehler stand offen. Auf macOS nimmt Qt dafuer
// standardmaessig das NATIVE NSColorPanel — und das stand da als
// schwarzes Fenster OHNE ein einziges bedienbares Element (im
// Bedienungshilfen-Baum leer), waehrend QColorDialog::getColor() seine
// eigene Ereignisschleife fuhr. Das Programm nahm nichts mehr an,
// nicht einmal Cmd+Q, nicht einmal SIGTERM; es musste abgeschossen
// werden.
//
// Warum dieser Pruefstand so klein ist: `getColor()` ist ein
// blockierender statischer Aufruf. Kein Test kann ihn ausloesen, ohne
// selbst haengen zu bleiben. Also wird festgenagelt, WOMIT er
// aufgerufen wird. Das ist wenig — aber es ist wahr, und es faellt
// durch, sobald jemand das Flag wieder herausnimmt.
//
// Ausdruecklich NICHT behauptet: dass Beenden damit unter allen
// Umstaenden geht. Ein Wachhund dafuer war gebaut und ist wieder
// verworfen worden, weil seine Gegenprobe gruen blieb — er haette
// etwas repariert, das in Qt ohnehin funktioniert, und die echte
// Ursache (der native Dialog) verdeckt.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-23 — Created for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QColorDialog>

#include "gui/ColorSwatchButton.h"

using namespace Longpath;

class TstColorPickerNotNative : public QObject {
    Q_OBJECT

private slots:

    void thePickerIsQtsOwnDialog()
    {
        const QColorDialog::ColorDialogOptions opts =
            ColorSwatchButton::pickerOptions();
        QVERIFY2(opts.testFlag(QColorDialog::DontUseNativeDialog),
                 "ohne DontUseNativeDialog nimmt Qt auf macOS das native "
                 "NSColorPanel -- das stand am 2026-09-23 als schwarzes, "
                 "leeres Fenster da und machte das Programm unbeendbar");
    }

    void theAlphaChannelIsStillThere()
    {
        // Gegenprobe: das neue Flag darf das alte nicht verdraengt
        // haben. Die Paletten des Hauses arbeiten mit Deckkraft.
        const QColorDialog::ColorDialogOptions opts =
            ColorSwatchButton::pickerOptions();
        QVERIFY2(opts.testFlag(QColorDialog::ShowAlphaChannel),
                 "der Farbwaehler muss die Deckkraft anbieten");
    }
};

QTEST_MAIN(TstColorPickerNotNative)
#include "tst_color_picker_not_native.moc"
