// ══ NICHT IM GATTER — und das ist eine Aussage, keine Bequemlichkeit ══
//
// Dieser Test STUERZT AB (SIGSEGV), und zwar an einem echten Fehler:
// Panadapter abloesen, Fenster schliessen. Der Rueckwaertsstapel:
//
//   QRhiWidgetPrivate::ensureRhi()::$_0 — EXC_BAD_ACCESS
//
// Das ist Qts eigener Aufraeum-Rueckruf am Zeichenkontext. Er
// ueberlebt das Widget und greift ins Leere.
//
// Zwei Kuren versucht (2026-08-21):
//   1. PanadapterStack::dockAllFloating() beim Schliessen — der
//      Absturz WANDERTE vom Schliessen in den Destruktor. Also wirkt
//      sie, aber sie reicht nicht. Bleibt drin: ein schwebendes
//      Fenster, das das Schliessen ueberlebt, ist unabhaengig davon
//      falsch (siehe c8d8161a).
//   2. releaseResources() vor dem Abbau — WIRKUNGSLOS, derselbe
//      Rahmen, dieselbe Adresse. Wieder zurueckgenommen; eine
//      Aenderung, die nichts beweisbar bewirkt, gehoert nicht in den
//      Baum.
//
// Er ist nicht eingetragen, weil ein abstuerzender Test das Gatter
// unbrauchbar macht und dann GAR NICHTS mehr ausgeliefert werden kann.
// Er bleibt liegen, weil die Frage richtig gestellt ist und die
// naechste Sitzung hier weitermacht — mit einem Rueckwaertsstapel als
// Ausgangspunkt statt mit Vermutungen.
//
// So wird er gefahren:
//   cmake --build build --target tst_real_pan_float_state
//   lldb -b -o run -o "bt 14" ./build/tests/tst_real_pan_float_state

#include <QtTest>
#include <QSet>
#include <QWindow>

// ── Die Meldungen zaehlen statt sie zu lesen ─────────────────────────
//
// „QRhiWidget: No QRhi" ist die Meldung eines Bereichs, der KEINEN
// Zeichenkontext mehr hat. Ein solcher Bereich wird nicht neu gemalt —
// er zeigt weiter, was zuletzt darin stand. Das sieht aus wie eine
// zweite Kopie und ist keine.
//
// PanadapterStack.cpp haelt seit dem 2026-08-20 fest, dass zwei Kuren
// dagegen versucht und verworfen wurden — beide aus Vermutungen. Ohne
// Zahl kann man nicht erkennen, ob eine Kur wirkt. Also erst die Zahl.
static int g_noRhi = 0;
static QtMessageHandler g_prev = nullptr;
static void counting(QtMsgType t, const QMessageLogContext& c,
                     const QString& m)
{
    if (m.contains(QLatin1String("No QRhi"))) { ++g_noRhi; return; }
    if (g_prev) { g_prev(t, c, m); }
}
#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
using namespace Longpath;
// Was hier bewacht wird, ist NICHT der gemeldete Fehler — der liess
// sich nicht nachstellen. Bewacht werden die drei Eigenschaften, die
// beim Suchen nachgewiesen wurden und die stimmen MUESSEN, damit das
// Abloesen ueberhaupt eine Chance hat, richtig zu sein:
//
//   1. Danach gibt es genau EINEN Panadapter, nicht zwei.
//   2. Er haengt im schwebenden Fenster, nicht mehr im Hauptfenster.
//   3. Seine native Zeichenflaeche deckt sich mit dem Widget. Taeten
//      sie das nicht, wuerde an zwei Stellen gemalt — das waere das
//      doppelte Bild aus dem Bildschirmfoto.
//
// Alle drei sind heute erfuellt, auch mit der echten Anordnung des
// Betreibers (seine 1,2-MB-Einstellungsdatei in den Testsandkasten
// kopiert und dieselbe Probe gefahren). Der Fehler sitzt also
// woanders.
class TstRealPanFloat : public QObject { Q_OBJECT
private slots:
    void floatingLeavesExactlyOnePanadapterInTheRightPlace() {
        qInfo() << "Einstellungsdatei der Probe:"
                << AppSettings::instance().filePath();

        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i=0;i<10;++i) QCoreApplication::processEvents();
        QTest::qWait(300);

        auto* stack = mw->findChild<PanadapterStack*>();
        qInfo() << "Stapel vorhanden:" << (stack != nullptr);
        if (stack) qInfo() << "  kennt:" << stack->panIdsForTesting()
                           << " schwebend:" << stack->floatingCountForTesting();

        const auto applets = mw->findChildren<PanadapterApplet*>();
        qInfo() << "PanadapterApplets:" << applets.size();
        for (auto* a : applets) qInfo() << "  Applet panId:" << a->panId()
                                        << "sichtbar" << a->isVisible();

        QSet<SpectrumWidget*> seen;
        for (QWidget* t : QApplication::topLevelWidgets())
            for (SpectrumWidget* s : t->findChildren<SpectrumWidget*>())
                seen.insert(s);
        qInfo() << "SpectrumWidgets insgesamt:" << seen.size();
        for (SpectrumWidget* s : seen) {
            QStringList chain;
            for (QWidget* w = s; w; w = w->parentWidget())
                chain << QString::fromLatin1(w->metaObject()->className());
            qInfo() << "  sichtbar" << s->isVisible() << chain.join(" < ");
        }

        // ── Jetzt abloesen und nachsehen ────────────────────────────
        qInfo() << "--- floatPanadapter(pan-0) ---";
        g_prev = qInstallMessageHandler(counting);
        g_noRhi = 0;
        stack->floatPanadapter(QStringLiteral("pan-0"));
        for (int i=0;i<10;++i) QCoreApplication::processEvents();
        QTest::qWait(300);

        // Ein paar Bilder lang laufen lassen: die Meldung kommt EINMAL
        // JE BILD, nicht einmal je Ereignis.
        for (int i = 0; i < 40; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(16);
        }
        qInstallMessageHandler(g_prev);
        qInfo() << "  OHNE Zeichenkontext, Meldungen:" << g_noRhi;
        QCOMPARE(stack->floatingCountForTesting(), 1);
        QSet<SpectrumWidget*> after;
        for (QWidget* t : QApplication::topLevelWidgets())
            for (SpectrumWidget* s : t->findChildren<SpectrumWidget*>())
                after.insert(s);
        QVERIFY2(after.size() == 1,
                 qPrintable(QStringLiteral(
                     "%1 Panadapter nach dem Abloesen statt einem — dann "
                     "waeren es wirklich zwei Widgets").arg(after.size())));
        for (SpectrumWidget* s : after) {
            QStringList chain;
            for (QWidget* w = s; w; w = w->parentWidget())
                chain << QString::fromLatin1(w->metaObject()->className());
            QVERIFY2(s->window() != mw,
                     qPrintable(QStringLiteral(
                         "Der Panadapter haengt noch im Hauptfenster: %1")
                         .arg(chain.join(QStringLiteral(" < ")))));
        }

        // ── Deckt sich die native Flaeche mit dem Widget? ───────────
        //
        // Der Panadapter traegt WA_NativeWindow: er hat ein EIGENES
        // Fenster im Betriebssystem. Nach dem Umhaengen muss dessen
        // Lage der des Widgets folgen. Tut sie es nicht, wird an zwei
        // Stellen gezeichnet — genau das doppelte Bild aus dem
        // Bildschirmfoto des Betreibers.
        for (SpectrumWidget* s : after) {
            QWindow* h = s->windowHandle();
            if (!h) { continue; }   // ohne eigenes Fenster kein Versatz
            QVERIFY2(h->geometry() == s->geometry(),
                     qPrintable(QStringLiteral(
                         "Native Flaeche und Widget stehen auseinander: "
                         "%1,%2 %3x%4 gegen %5,%6 %7x%8 — dann wird an "
                         "zwei Stellen gemalt")
                         .arg(h->geometry().x()).arg(h->geometry().y())
                         .arg(h->geometry().width()).arg(h->geometry().height())
                         .arg(s->geometry().x()).arg(s->geometry().y())
                         .arg(s->geometry().width()).arg(s->geometry().height())));
        }

        // ── Bleibt im Hauptfenster ein LOCH? ────────────────────────
        //
        // Der Betreiber: „bitte kontrolliere selbst." Die Frage war,
        // ob das doppelte Bild beim Groessenaendern verschwindet.
        //
        // Mit den Augen kann ich das nicht — aber die Bedingung dafuer
        // ist messbar: stehengebliebene Pixel brauchen eine Flaeche,
        // die Platz belegt und NICHTS Sichtbares enthaelt. Dort malt
        // niemand, also bleibt stehen, was zuletzt darin stand.
        auto describe = [&](const char* when) {
            int visibleKids = 0;
            for (QWidget* k : stack->findChildren<QWidget*>()) {
                if (k->isVisible() && k->parentWidget() == stack) {
                    ++visibleKids;
                }
            }
            int anyVisible = 0;
            for (QWidget* k : stack->findChildren<QWidget*>()) {
                if (k->isVisible()) { ++anyVisible; }
            }
            for (QWidget* k : stack->findChildren<QWidget*>()) {
                if (!k->isVisible()) { continue; }
                qInfo() << "   " << when << "sichtbar im Stapel:"
                        << k->metaObject()->className()
                        << k->geometry()
                        << "Elter"
                        << (k->parentWidget()
                                ? k->parentWidget()->metaObject()->className()
                                : "-");
            }
            qInfo() << when
                    << "Stapel" << stack->size()
                    << "sichtbar" << stack->isVisible()
                    << "direkte sichtbare Kinder" << visibleKids
                    << "sichtbare Nachfahren" << anyVisible;
        };
        describe("nach dem Abloesen:");

        // ── Was NICHT geprueft werden kann ──────────────────────────
        //
        // Ob die zurueckbleibende Flaeche auf dem Schirm uebermalt
        // wird, laesst sich hier nicht messen. QWidget::render() malt
        // den Fensterhintergrund entweder immer mit (dann ist die
        // Pruefung blind fuer den Fehler) oder gar nicht (dann ist sie
        // blind fuer die Behebung). Beides ausprobiert, beides gruen
        // beziehungsweise rot in BEIDEN Faellen. Ein Messgeraet, das
        // den Unterschied nicht sehen kann, beweist nichts — und eine
        // Pruefung, die immer dasselbe sagt, ist keine.
        //
        // Was geprueft WIRD, steht unten: dass das Zurueckholen den
        // Panadapter wieder ins Hauptfenster bringt. Das ist die
        // Eigenschaft, die durch das Verstecken leerer Splitter kaputt
        // gehen KOENNTE — und darum die, die bewacht gehoert.

        // Und jetzt das, was der Betreiber tun wuerde: Fenster ziehen.
        mw->resize(1500, 900);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(200);
        describe("nach Groessenaenderung:");

        // ── Zurueckholen muss den Panadapter zurueckbringen ─────────
        //
        // hideEmptySplitters() versteckt beim Abloesen die leer
        // gewordene Flaeche. Wuerde sie beim Zurueckdocken nicht
        // wieder hervorgeholt, bliebe das Hauptfenster leer — eine
        // schlimmere Krankheit als die behandelte.
        stack->dockPanadapter(QStringLiteral("pan-0"));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(250);

        QCOMPARE(stack->floatingCountForTesting(), 0);

        int backHome = 0;
        for (SpectrumWidget* w : mw->findChildren<SpectrumWidget*>()) {
            if (w->isVisible() && w->window() == mw) { ++backHome; }
        }
        QVERIFY2(backHome == 1,
                 qPrintable(QStringLiteral(
                     "Nach dem Zurueckdocken stehen %1 sichtbare "
                     "Panadapter im Hauptfenster statt einem")
                     .arg(backHome)));

        mw->close();
        for (int i=0;i<6;++i) QCoreApplication::processEvents();
        // Seit dem Rueckport der AetherSDR-Schutzmethoden (2026-08-22)
        // ist der Abbau der ABSTURZ-REPRO: vorher starb genau dieses
        // delete mit SIGSEGV in QRhiWidgetPrivate::ensureRhi (#2495).
        // Ueberlebt es, ist der Rueckport wirksam.
        delete mw;
    }
};
QTEST_MAIN(TstRealPanFloat)
#include "tst_real_pan_float_state.moc"
