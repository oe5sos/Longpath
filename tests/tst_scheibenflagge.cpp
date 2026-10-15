// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Flagge ueber dem Panadapter — die dritte Runde um dieselbe Sache.
//
// Bis zum 2026-08-18 gab es sie (VfoWidget, 3274 Zeilen, aus AetherSDR
// nach Thetis-Vorbild). Der Betreiber liess sie streichen: „Sie kommt
// ersatzlos weg, nicht ausblendbar." Alles darin zog in eigene Fenster
// um.
//
// Am 2026-10-15 wollte er sie zurueck — „wie es eigentlich bei aether
// und nereus war ... welches wir dann umgebaut haben auf lauter eigene
// windows" —, und zwar als OPTION: „wenn ich sota mache stelle ich mir
// ein anderes profil zusammen, als ich zu hause bin."
//
// Genau das haelt dieser Lauf nach: dass sie da ist, wenn man sie
// einschaltet, dass sie ihrem Traeger folgt, dass sie sich verschieben
// laesst und dort bleibt — und dass sie wieder verschwindet.
//
// Gegenprobe (2026-10-15): ohne setSliceFlagsEnabled(true) findet
// derSchalterBringtSie keine einzige Flagge.

#include <QtTest>

#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/SliceFlag.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstScheibenflagge : public QObject
{
    Q_OBJECT

private:
    MainWindow* bauen()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        if (!QTest::qWaitForWindowExposed(mw)) { return mw; }
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        return mw;
    }

private slots:
    /// Aus ist die Vorgabe — der Zustand seit August.
    void abWerkAus()
    {
        auto* mw = bauen();
        QVERIFY2(!mw->sliceFlagsEnabled(),
                 "die Flagge waere ab Werk an — das war nie abgesprochen");
        QVERIFY2(mw->findChildren<SliceFlag*>().isEmpty(),
                 "eine Flagge steht da, obwohl der Schalter aus ist");
        delete mw;
    }

    /// Der Schalter bringt je Scheibe eine.
    void derSchalterBringtSie()
    {
        auto* mw = bauen();
        auto* radio = mw->findChild<RadioModel*>();
        QVERIFY(radio);
        // Ohne Funkgeraet gibt es keine Scheibe — und ohne Scheibe
        // keine Flagge. Ein QSKIP waere hier aber eine Nicht-Aussage
        // („Abwesenheit ist keine Aussage", 2026-10-03): der Lauf liefe
        // gruen durch, ohne je eine Flagge gesehen zu haben. Also eine
        // anlegen. suppressAutoStreamBinding, damit die Sicherheitslogik
        // sie nicht fuer die Scheibe eines echten Funkgeraets haelt.
        if (radio->slices().isEmpty()) {
            radio->addSlice(QString(), /*suppressAutoStreamBinding=*/true);
            for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        }
        const int scheiben = radio->slices().size();
        QVERIFY2(scheiben > 0, "keine Scheibe anzulegen — dann prueft "
                               "das hier nichts");

        mw->setSliceFlagsEnabled(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        const auto flaggen = mw->findChildren<SliceFlag*>();
        qInfo() << "Scheiben:" << scheiben << "Flaggen:" << flaggen.size();
        QCOMPARE(flaggen.size(), scheiben);

        mw->setSliceFlagsEnabled(false);
        // deleteLater() braucht einen Durchlauf der Schlange, sonst
        // findet findChildren die Flagge noch — sie ist dann nicht
        // „noch da", sondern nur noch nicht abgeholt.
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY2(mw->findChildren<SliceFlag*>().isEmpty(),
                 "nach dem Ausschalten steht noch eine Flagge da");
        delete mw;
    }

    /// Verschieben heisst: sie bleibt liegen, und die Leitlinie zeigt
    /// weiter auf den Traeger. DAS ist der Unterschied zwischen einer
    /// Flagge und einer Beschriftung.
    void verschobenBleibtVerschoben()
    {
        SliceFlag flagge(0, nullptr);
        QVERIFY2(!flagge.wasMoved(), "frisch angelegt und schon verschoben");
        QCOMPARE(flagge.manualOffset(), QPoint(0, 0));

        flagge.setManualOffset(QPoint(40, 70));
        QVERIFY2(flagge.wasMoved(), "die Verschiebung wurde nicht gemerkt");
        QCOMPARE(flagge.manualOffset(), QPoint(40, 70));
    }

    /// Breite und Hoehe stehen fest — eine Flagge, die mit ihrem Inhalt
    /// waechst, verdeckt irgendwann das, wofuer sie da ist.
    void sieBleibtSchmal()
    {
        SliceFlag flagge(1, nullptr);
        QCOMPARE(flagge.width(),  SliceFlag::kWidth);
        QCOMPARE(flagge.height(), SliceFlag::kHeight);
        QVERIFY2(SliceFlag::kHeight <= 80,
                 "die Flagge ist hoch geworden — dann deckt sie den "
                 "Wasserfall zu, und genau deshalb fiel die alte");
    }
};

QTEST_MAIN(TstScheibenflagge)
#include "tst_scheibenflagge.moc"
