// SPDX-License-Identifier: GPL-3.0-or-later
//
// Eine selbst gewaehlte Grundfarbe muss sich zuruecknehmen lassen.
//
// Der Betreiber, 2026-08-21: „der raster inkl. blau-graue hintergrund
// gefaellt mir beim test sehr gut, bitte setze ihn als standard."
//
// Der Standard WAR schon das Blaugrau (Style::kPanadapterBg, #141e27).
// Was fehlte, war der Weg zurueck: „Grundfarbe…" schreibt die Wahl in
// die Einstellungen, und von da an gewinnt sie — gegen den eingebauten
// Wert und gegen jedes Thema. In seiner Datei stand deckendes Schwarz
// (#ff000000), also blieb der Panadapter schwarz, egal was der
// Standard sagte.

#include <QtTest>
#include <QMenu>

#include "gui/SpectrumWidget.h"
#include "gui/PanadapterApplet.h"
#include "gui/StyleConstants.h"
#include "core/AppSettings.h"

using namespace Longpath;

class TstPanadapterBaseColourReset : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().remove(
            QStringLiteral("PanadapterBackgroundFill"));
    }

    /// Waehlen, zuruecknehmen, wieder beim Standard sein.
    void aChosenColourCanBeTakenBack()
    {
        SpectrumWidget w;
        w.resize(800, 500);

        const QColor standard(Style::kPanadapterBg);
        QCOMPARE(w.backgroundFillColor(), standard);

        w.setBackgroundFillColor(QColor(QStringLiteral("#ff000000")));
        QVERIFY2(w.backgroundFillColor() != standard,
                 "Die Wahl ist gar nicht angekommen");
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("PanadapterBackgroundFill"),
                            QString{}).toString(),
                 QStringLiteral("#ff000000"));

        w.resetBackgroundFillColor();
        QCOMPARE(w.backgroundFillColor(), standard);
    }

    /// Der Schluessel wird GELOESCHT, nicht auf den heutigen Wert
    /// gesetzt.
    ///
    /// Der Unterschied ist nicht theoretisch: stuende dort wieder eine
    /// Zahl, waere sie eine neue eigene Wahl — und wer spaeter ein
    /// anderes Thema nimmt, bekaeme dessen Panadapter-Farbe nicht mehr,
    /// sondern das eingefrorene Blaugrau von heute.
    void theKeyIsRemovedRatherThanOverwritten()
    {
        SpectrumWidget w;
        w.setBackgroundFillColor(QColor(QStringLiteral("#ff112233")));
        w.resetBackgroundFillColor();

        const QString left = AppSettings::instance()
            .value(QStringLiteral("PanadapterBackgroundFill"),
                   QStringLiteral("<weg>")).toString();
        QCOMPARE(left, QStringLiteral("<weg>"));
    }

    /// Und der Eintrag steht im Zahnrad, wo die Farbwahl steht.
    void theEntrySitsNextToTheColourPicker()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);

        QSignalSpy spy(&applet,
                       &PanadapterApplet::backgroundColourResetRequested);
        // Der Eintrag wird NICHT ueber seinen Wortlaut gesucht
        // (2026-10-14). Vorher stand hier „zurücksetzen", und mit der
        // Umstellung auf Englisch als Quellsprache fiel dieser Lauf in
        // der CI — zu Recht: ein Text ist ab jetzt uebersetzbar, also
        // kein Bezeichner. Dieselbe tr()-Rolle wie im Programm liefert
        // genau den Text, der gerade gilt: englisch ohne Uebersetzer,
        // deutsch mit. Ein Prueflauf, der auf einen Wortlaut zeigt,
        // waere mit der deutschen Fassung ein zweites Mal gefallen.
        const QString gesucht = PanadapterApplet::tr("Reset base colour");
        bool fired = false;
        for (QAction* a : m->actions()) {
            if (a->text().contains(gesucht)) {
                a->trigger();
                fired = true;
                break;
            }
        }
        QVERIFY2(fired, qPrintable(QStringLiteral(
                     "Kein '%1' im Zahnrad").arg(gesucht)));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TstPanadapterBaseColourReset)
#include "tst_panadapter_base_colour_reset.moc"
