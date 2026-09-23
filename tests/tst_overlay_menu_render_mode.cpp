// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die 2D/3D-Umschaltung gehoert in das Blatt, das der Rechtsklick auf
// den Panadapter oeffnet.
//
// Betreiber am 2026-09-23, mit einem Bild von genau diesem Blatt:
// „hier sollte das 2d und 3d zum ändern sein!"
//
// Sie gab es zu dem Zeitpunkt schon — aber in SpectrumOverlayPanel, der
// Knopfleiste AUF dem Spektrum. Das hier ist SpectrumOverlayMenu, ein
// anderes Widget; es traegt Wasserfall-Farben, Fuellung, Anzeigebereich,
// CTUN und die Kerbe. Ich habe ihm zweimal gesagt, die Zeile sei hier —
// sie war es nie.
//
// Zwei Haelften, die beide sitzen muessen:
//   * die Auswahl schaltet den Panadapter wirklich um, und
//   * beim Oeffnen zeigt sie, was der Panadapter GERADE macht (sonst
//     stuende dort nach jedem Start „2D", auch wenn 3D laeuft — genau
//     dieser Fehler steckte im anderen Blatt schon einmal drin).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-23 — Created for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QComboBox>
#include <QDir>
#include <QSignalSpy>

#include "gui/SpectrumOverlayMenu.h"

using namespace Longpath;

class TstOverlayMenuRenderMode : public QObject {
    Q_OBJECT

private:
    static QComboBox* modeCombo(SpectrumOverlayMenu* menu)
    {
        return menu->findChild<QComboBox*>(
            QStringLiteral("overlayMenuRenderModeCombo"));
    }

private slots:

    void theSheetCarriesTheSwitch()
    {
        SpectrumOverlayMenu menu;
        QVERIFY2(modeCombo(&menu),
                 "das Blatt muss eine 2D/3D-Auswahl tragen");
        QCOMPARE(modeCombo(&menu)->count(), 2);
    }

    void switchingEmitsTheMode()
    {
        SpectrumOverlayMenu menu;
        QComboBox* cmb = modeCombo(&menu);
        QVERIFY(cmb);

        QSignalSpy spy(&menu, &SpectrumOverlayMenu::spectrumRenderModeChanged);
        cmb->setCurrentIndex(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);

        cmb->setCurrentIndex(0);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toInt(), 0);
    }

    void openingShowsWhatThePanadapterDoes()
    {
        SpectrumOverlayMenu menu;
        QComboBox* cmb = modeCombo(&menu);
        QVERIFY(cmb);

        // Nachziehen darf NICHT wie ein Klick aussehen, sonst stellt das
        // blosse Oeffnen des Blattes den Panadapter um.
        QSignalSpy spy(&menu, &SpectrumOverlayMenu::spectrumRenderModeChanged);
        menu.setSpectrumRenderModeIndex(1);
        QCOMPARE(cmb->currentIndex(), 1);
        QCOMPARE(spy.count(), 0);

        menu.setSpectrumRenderModeIndex(0);
        QCOMPARE(cmb->currentIndex(), 0);
        QCOMPARE(spy.count(), 0);

        // Unsinnige Werte landen im gueltigen Bereich.
        menu.setSpectrumRenderModeIndex(7);
        QCOMPARE(cmb->currentIndex(), 1);
        menu.setSpectrumRenderModeIndex(-3);
        QCOMPARE(cmb->currentIndex(), 0);
    }

    // Mit LONGPATH_GRAB_DIR faellt das Blatt als Bild heraus -- so laesst
    // sich zeigen, wie die neue Zeile zwischen den anderen aussieht, ohne
    // die ganze Anwendung dafuer zu starten. Ohne die Variable tut die
    // Stelle nichts.
    void theSheetAsAPicture()
    {
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (grabDir.isEmpty()) {
            QSKIP("LONGPATH_GRAB_DIR nicht gesetzt -- kein Bild gewuenscht.");
        }
        SpectrumOverlayMenu menu;
        menu.setSpectrumRenderModeIndex(1);
        menu.resize(menu.sizeHint());
        const QPixmap shot = menu.grab();
        QVERIFY2(!shot.isNull(), "das Blatt liess sich nicht abbilden");
        QDir().mkpath(grabDir);
        const QString path =
            QDir(grabDir).filePath(QStringLiteral("overlay-menu-2d-3d.png"));
        QVERIFY2(shot.save(path), qPrintable(QStringLiteral(
            "Bild liess sich nicht schreiben: %1").arg(path)));
        qInfo() << "Blatt abgelegt:" << path;
    }
};

QTEST_MAIN(TstOverlayMenuRenderMode)
#include "tst_overlay_menu_render_mode.moc"
