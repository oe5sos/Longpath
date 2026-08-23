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
#include <QChildEvent>
#include <QMouseEvent>
#include <QMenu>

#include "gui/SpectrumWidget.h"
#include "gui/widgets/NotchEditPopup.h"

#include <QCheckBox>
#include <QSpinBox>

using namespace Longpath;


namespace {

// ── Warum hier nicht QTest::mouseClick steht ────────────────────────
//
// Gemessen am 2026-08-23, nach zwei falschen Reparaturen. Eine Sonde
// im Fehlerfall zeigte:
//
//   Klick bei x = 581 · Notch dort: 4 · Fenster aktiv: FALSE
//                                     · Menues jetzt: 0
//
// und im Gutfall dasselbe mit "Fenster aktiv: true · Menues jetzt: 2".
// Der Notch wird also gefunden, die Geometrie stimmt — der Klick
// erreicht den Handler nur dann nicht, wenn das Fenster nicht aktiv
// ist. Unter "ctest -j8" streiten sich acht Oberflaechentests um die
// Aktivierung, und einmal ist sogar qWaitForWindowActive selbst
// gescheitert.
//
// Ob ein synthetischer Klick ein INAKTIVES Fenster erreicht, ist Qts
// Sache und nicht unsere. Diese Pruefung will wissen, ob UNSER
// mousePressEvent bei einem Rechtsklick auf einen Notch das Menue
// oeffnet — also wird genau das Ereignis zugestellt, das ein
// Rechtsklick erzeugt, ohne Umweg ueber den Fenstermanager.
//
// Meine beiden vorigen Reparaturen (Wartezeit auf 15 s, dann
// QTRY_VERIFY aufs Menue) haben die Haeufigkeit gesenkt und die
// Ursache verfehlt. Beide sind zurueckgenommen, wo sie nur Symptome
// verdeckten.
void sendPress(QWidget* w, const QPoint& pos, Qt::MouseButton button,
               QEvent::Type type = QEvent::MouseButtonPress)
{
    const QPointF local(pos);
    const QPointF global = w->mapToGlobal(pos);
    QMouseEvent ev(type, local, local, global, button, button,
                   Qt::NoModifier, Qt::MouseEventSynthesizedByApplication);
    QCoreApplication::sendEvent(w, &ev);
}

} // namespace


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

        QSignalSpy gone(&w, &SpectrumWidget::notchRemoveRequested);
        sendPress(&w, QPoint(x, y), Qt::RightButton);
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
        QMenu* menu = w.findChild<QMenu*>();
        QVERIFY2(menu != nullptr,
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

        // Ein Doppelklick ist Druck, Loslassen, Doppelklick — und aus
        // demselben Grund wie oben direkt zugestellt.
        sendPress(&w, QPoint(x, 80), Qt::LeftButton);
        sendPress(&w, QPoint(x, 80), Qt::LeftButton, QEvent::MouseButtonRelease);
        sendPress(&w, QPoint(x, 80), Qt::LeftButton, QEvent::MouseButtonDblClick);
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
        qInfo() << "Editor beim Oeffnen:" << wid->value() << "(erwartet 400)";
        QCOMPARE(wid->value(), 400);          // zeigt den Ist-Zustand
        wid->setValue(50);                    // zu schmal, absichtlich
        QCoreApplication::processEvents();
        qInfo() << "nach setValue(50):" << width.count() << "Wuensche";
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

        qInfo() << "Editorwerte:" << wid->value()
                << "· Breitenwuensche:" << width.count()
                << "· Abschaltwuensche:" << off.count();
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
