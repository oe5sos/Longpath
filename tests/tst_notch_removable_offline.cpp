// SPDX-License-Identifier: GPL-3.0-or-later
//
// Notch-Filter muessen sich auch OHNE Funkgeraet wieder loeschen
// lassen.
//
// Der Betreiber, 2026-08-21, mit einem Bildschirmfoto von sechs gelben
// Balken auf 40 m: „wie kann ich diese filter? loeschen? — app ist
// geschlossen." Die Balken sind Notch-Filter (TNF): gelb aktiv, gruen
// der gerade ausgewaehlte.
//
// Der Weg dahin ist ein Rechtsklick auf den Balken. Ob der im
// getrennten Zustand ankommt, ist nicht selbstverstaendlich: der
// Panadapter schluckt dort Klicks und oeffnet stattdessen das
// Verbindungsfeld (Phase 3Q-8). Er schluckt sie NUR links — aber das
// ist eine Zeile, die jemand aendern kann, ohne diesen Fall zu
// bedenken. Also festgenagelt.

#include <QtTest>
#include <QMenu>

#include "gui/SpectrumWidget.h"
#include "gui/widgets/NotchEditPopup.h"

#include <QCheckBox>
#include <QSpinBox>

using namespace Longpath;

class TstNotchRemovableOffline : public QObject
{
    Q_OBJECT

private slots:
    void aNotchCanBeRemovedWhileDisconnected()
    {
        SpectrumWidget w;
        w.resize(1200, 700);
        w.setConnectionState(ConnectionState::Disconnected);
        w.setFrequencyRange(7'131'200.0, 200'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVector<SpectrumWidget::NotchMarker> notches;
        SpectrumWidget::NotchMarker n;
        n.id = 4;
        n.freqMhz = 7.1312;
        n.widthHz = 400.0;
        n.active = true;
        notches.append(n);
        w.setNotchMarkers(notches);
        QCoreApplication::processEvents();

        // Die Stelle NICHT rechnen, sondern suchen. Die Mitte des
        // Fensters ist nicht die Mitte des Spektrums — rechts sitzt
        // der dBm-Streifen. Genau diese Verwechslung hat in dieser
        // Sitzung schon einmal einen Test fuer nichts gruen gemacht.
        int x = -1;
        for (int probe = 0; probe < w.width(); ++probe) {
            if (w.notchAtPixelForTest(probe) == 4) { x = probe; break; }
        }
        QVERIFY2(x >= 0, "Der Notch liegt nirgends auf dem Bild — dann "
                         "prueft der Rest dieses Tests nichts");
        const int y = 80;

        // Das Fenster muss AKTIV sein, nicht bloss sichtbar. Ein
        // popup() auf einem inaktiven Fenster kommt unter Last nicht
        // zuverlaessig hoch; nach dem QTRY_VERIFY unten blieb ein Rest
        // von rund einem Ausfall auf sechs Durchlaeufe uebrig.
        QVERIFY(QTest::qWaitForWindowActive(&w));

        QSignalSpy gone(&w, &SpectrumWidget::notchRemoveRequested);
        QTest::mouseClick(&w, Qt::RightButton, Qt::NoModifier, QPoint(x, y));
        QCoreApplication::processEvents();

        // Das Menue oeffnet mit popup(), nicht exec() — es haengt also
        // als Kind am Widget und laesst sich hier befragen.
        //
        // ABER: nicht sofort. Unter "ctest -j8" fiel dieser Test am
        // 2026-08-23 rund jedes dritte Mal aus, weil EIN
        // processEvents() nach dem Rechtsklick nicht reicht — das
        // Menue entsteht erst eine Runde spaeter.
        //
        // Ich habe das zuerst der Fensterwartezeit angelastet und sie
        // von 5 auf 15 Sekunden gehoben. Das war falsch: die
        // Fehlermeldung sagt "oeffnet kein Menue", nicht "Fenster nicht
        // sichtbar". Die drei gruenen Durchlaeufe danach waren Zufall,
        // kein Beleg — genau die Art Bestaetigung, auf die man in
        // dieser Sitzung schon mehrfach hereingefallen ist. Die
        // Wartezeit steht wieder auf ihrem Vorgabewert.
        QMenu* menu = nullptr;
        QTRY_VERIFY2((menu = w.findChild<QMenu*>()) != nullptr,
                     "Rechtsklick auf einen Notch oeffnet kein Menue — "
                     "im getrennten Zustand kaeme man dann nicht mehr an "
                     "seine Filter heran");

        QAction* remove = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text().contains(QLatin1String("Remove"))) {
                remove = a;
                break;
            }
        }
        QVERIFY2(remove, "Kein 'Remove Notch' im Menue");
        remove->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(gone.count(), 1);
        QCOMPARE(gone.at(0).at(0).toInt(), 4);
    }

    /// Doppelklick auf den Balken macht den Editor auf — auch getrennt.
    ///
    /// „es waere auch gut, wenn man am notchfilter klickt und dann
    /// diese bearbeite kann und auch loeschen" (2026-08-21).
    void aDoubleClickOpensTheEditor()
    {
        SpectrumWidget w;
        w.resize(1200, 700);
        w.setConnectionState(ConnectionState::Disconnected);
        w.setFrequencyRange(7'131'200.0, 200'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVector<SpectrumWidget::NotchMarker> notches;
        SpectrumWidget::NotchMarker n;
        n.id = 7; n.freqMhz = 7.1312; n.widthHz = 400.0; n.active = true;
        notches.append(n);
        w.setNotchMarkers(notches);
        QCoreApplication::processEvents();

        int x = -1;
        for (int probe = 0; probe < w.width(); ++probe) {
            if (w.notchAtPixelForTest(probe) == 7) { x = probe; break; }
        }
        QVERIFY(x >= 0);

        QTest::mouseDClick(&w, Qt::LeftButton, Qt::NoModifier, QPoint(x, 80));
        QCoreApplication::processEvents();

        auto* ed = w.findChild<NotchEditPopup*>();
        QVERIFY2(ed && ed->isVisible(),
                 "Doppelklick auf einen Notch macht keinen Editor auf");
        QCOMPARE(ed->notchId(), 7);
    }

    /// Was im Editor steht, geht als Wunsch hinaus — und was
    /// zurueckkommt, steht danach drin.
    ///
    /// Der zweite Teil ist der wichtige: WDSP setzt zu schmale Breiten
    /// selbst herauf (nbp.c:122-125). Ohne Nachfuehrung zeigte das
    /// Fenster weiter den Wunsch und der Balken das Ergebnis — zwei
    /// Zahlen fuer dieselbe Sache.
    void theEditorSendsWishesAndShowsWhatCameBack()
    {
        SpectrumWidget w;
        w.resize(1200, 700);
        w.setFrequencyRange(7'131'200.0, 200'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVector<SpectrumWidget::NotchMarker> notches;
        SpectrumWidget::NotchMarker n;
        n.id = 9; n.freqMhz = 7.1312; n.widthHz = 400.0; n.active = true;
        notches.append(n);
        w.setNotchMarkers(notches);

        QSignalSpy width(&w, &SpectrumWidget::notchWidthRequested);
        QSignalSpy off(&w, &SpectrumWidget::notchActiveRequested);

        w.openNotchEditor(9, QPoint(100, 100));
        auto* ed = w.findChild<NotchEditPopup*>();
        QVERIFY(ed);

        auto* wid = ed->findChild<QSpinBox*>();
        QVERIFY(wid);
        QCOMPARE(wid->value(), 400);          // zeigt den Ist-Zustand
        wid->setValue(50);                    // zu schmal, absichtlich
        QCoreApplication::processEvents();
        QCOMPARE(width.count(), 1);
        QCOMPARE(width.at(0).at(1).toDouble(), 50.0);

        auto* act = ed->findChild<QCheckBox*>();
        QVERIFY(act);
        act->setChecked(false);
        QCoreApplication::processEvents();
        QCOMPARE(off.count(), 1);
        QCOMPARE(off.at(0).at(1).toBool(), false);

        // Das Modell hat 100 daraus gemacht. Der Editor muss folgen —
        // und darf dabei keinen neuen Wunsch ausloesen.
        notches[0].widthHz = 100.0;
        notches[0].active = false;
        w.setNotchMarkers(notches);
        QCoreApplication::processEvents();

        QCOMPARE(wid->value(), 100);
        QCOMPARE(width.count(), 1);           // kein Echo
        QCOMPARE(off.count(), 1);
    }

    /// Verschwindet der Notch, geht der Editor zu.
    void theEditorClosesWhenTheNotchIsGone()
    {
        SpectrumWidget w;
        w.resize(1200, 700);
        w.setFrequencyRange(7'131'200.0, 200'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVector<SpectrumWidget::NotchMarker> notches;
        SpectrumWidget::NotchMarker n;
        n.id = 3; n.freqMhz = 7.1312; n.widthHz = 400.0; n.active = true;
        notches.append(n);
        w.setNotchMarkers(notches);
        w.openNotchEditor(3, QPoint(100, 100));

        auto* ed = w.findChild<NotchEditPopup*>();
        QVERIFY(ed && ed->isVisible());

        w.setNotchMarkers({});                 // geloescht
        QCoreApplication::processEvents();
        QVERIFY2(!ed->isVisible(),
                 "Der Editor steht auf einem Notch, den es nicht mehr "
                 "gibt — seine Knoepfe gehen ins Leere");
    }

    /// „Alle entfernen" steht erst ab zwei Filtern im Menue.
    ///
    /// Bei einem einzigen waere der Eintrag dasselbe wie „Remove
    /// Notch", nur gefaehrlicher formuliert.
    void removeAllOnlyAppearsWhenThereAreSeveral()
    {
        SpectrumWidget w;
        w.resize(1200, 700);
        w.setFrequencyRange(7'131'200.0, 200'000.0);

        auto entries = [&w](int id) {
            QMenu m;
            w.buildNotchContextMenuForTest(id, m);
            QStringList out;
            for (QAction* a : m.actions()) { out << a->text(); }
            return out;
        };

        QVector<SpectrumWidget::NotchMarker> one;
        SpectrumWidget::NotchMarker a;
        a.id = 1; a.freqMhz = 7.1312; a.widthHz = 400.0; a.active = true;
        one.append(a);
        w.setNotchMarkers(one);
        QVERIFY2(!entries(1).join(QLatin1Char('|')).contains(
                     QLatin1String("Alle ")),
                 "Bei einem einzigen Filter darf kein 'Alle entfernen' "
                 "im Menue stehen");

        SpectrumWidget::NotchMarker b = a;
        b.id = 2; b.freqMhz = 7.1350;
        one.append(b);
        w.setNotchMarkers(one);
        const QString all = entries(1).join(QLatin1Char('|'));
        QVERIFY2(all.contains(QLatin1String("Alle 2")),
                 qPrintable(QStringLiteral("Kein Sammel-Eintrag: %1").arg(all)));
        QVERIFY2(all.contains(QLatin1String("bearbeiten")),
                 qPrintable(QStringLiteral("Kein 'bearbeiten': %1").arg(all)));
    }
};

QTEST_MAIN(TstNotchRemovableOffline)
#include "tst_notch_removable_offline.moc"
