// no-port-check: Longpath-eigenes Zeichenwerkzeug, kein Prüfstand.
// =================================================================
// tests/tst_ft8_kopfzeile_blatt.cpp  (Longpath)
// =================================================================
//
// Kein Prüfstand — ein Zeichenwerkzeug. Es rendert die Kopfzeile des
// FT8-Dekoders samt dem ⚙ und das Blatt dahinter, damit der Betreiber
// am Bild entscheidet statt in der laufenden App zu suchen.
//
// Braucht LONGPATH_GRAB_DIR **und** LONGPATH_ENTWURF=1: die CI setzt
// den ersten global (ci.yml), er allein wäre also keine Weiche,
// sondern eine Falle.
//
//   LONGPATH_GRAB_DIR=/tmp/blaetter LONGPATH_ENTWURF=1 \
//     QT_QPA_PLATFORM=offscreen ./build/tests/tst_ft8_kopfzeile_blatt
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Neu, zum Optionsknopf im Dekoderkopf.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QPainter>
#include <QPushButton>
#include <QStyleFactory>

#include "gui/styles/AppTheme.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TestFt8KopfzeileBlatt : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Wie main.cpp, sonst zeichnet das Blatt im hellen Systemthema
        // und zeigt Farben, die es im Programm nicht gibt.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
    }

    // ── Die Knoepfe im Dekoder stellen auch die Skala ────────────────
    //
    // Betreiber 2026-10-21: „bei doppelklick oder klick auf ft8 und ft4
    // muss er automatisch auf die richtige frequenz springen". Bis dahin
    // schalteten FT8/FT4 IM Dekoder nur das Verfahren um -- die Knoepfe
    // in der Kommandoleiste stellten die Frequenz, die im Panel nicht.
    //
    // Geprueft wird das Signal, nicht die Frequenz: die Anrufstelle
    // setzt MainWindow::aufDigiStellen(), und die hat ihre eigene
    // Regel (Band behalten, auf einer Anrufstelle stehenbleiben).
    void knoepfe_im_dekoder_verlangen_die_frequenz()
    {
        RadioModel rm;
        Ft8Applet applet(&rm, nullptr);

        QSignalSpy wunsch(&applet, &Ft8Applet::digiFrequenzGewuenscht);

        auto* ft4 = applet.findChild<QPushButton*>();
        QVERIFY(ft4 != nullptr);

        // Die beiden Verfahrensknoepfe tragen ihren Namen.
        QPushButton* ft8Knopf = nullptr;
        QPushButton* ft4Knopf = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("FT8")) { ft8Knopf = b; }
            if (b->text() == QStringLiteral("FT4")) { ft4Knopf = b; }
        }
        QVERIFY(ft8Knopf != nullptr);
        QVERIFY(ft4Knopf != nullptr);

        ft4Knopf->click();
        QCOMPARE(wunsch.count(), 1);
        QCOMPARE(wunsch.at(0).at(0).value<DigiFrequenzen::Digimodus>(),
                 DigiFrequenzen::Digimodus::Ft4);

        ft8Knopf->click();
        QCOMPARE(wunsch.count(), 2);
        QCOMPARE(wunsch.at(1).at(0).value<DigiFrequenzen::Digimodus>(),
                 DigiFrequenzen::Digimodus::Ft8);
    }

    void kopfzeile_mit_zahnrad()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()
            || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1");
        }

        RadioModel rm;
        Ft8Applet applet(&rm, nullptr);
        applet.resize(620, 150);
        applet.show();
        QTest::qWait(400);

        // Erst nachsehen, DASS es ihn gibt -- ein Bild ohne Knopf und
        // ein Bild mit unsichtbarem Knopf sehen gleich aus.
        const auto knoepfe = applet.findChildren<QPushButton*>();
        int zahnraeder = 0;
        for (QPushButton* b : knoepfe) {
            if (b->text() == QStringLiteral("...")) {
                ++zahnraeder;
                qInfo() << "Optionsknopf:" << b->geometry() << "sichtbar"
                        << b->isVisible() << "Elternteil"
                        << (b->parentWidget() ? b->parentWidget()->metaObject()->className() : "-");
            }
        }
        QCOMPARE(zahnraeder, 1);

        const QImage bild = applet.grab().toImage();
        const QString pfad = dir + QStringLiteral("/ft8-kopfzeile.png");
        QVERIFY2(bild.save(pfad), qPrintable(pfad));
        qInfo() << "Blatt:" << pfad << bild.size();
    }
};

QTEST_MAIN(TestFt8KopfzeileBlatt)
#include "tst_ft8_kopfzeile_blatt.moc"
