// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_native_overlay_audit.cpp  (Longpath)
// =================================================================
// Liegt irgendwo etwas Nicht-Natives IN einem nativen Fenster?
//
// Auf macOS zeichnet ein natives NSView ueber allen nicht-nativen
// Geschwistern; unser eigener Vermerk in SpectrumWidget.cpp:551 sagt
// es fuer QRhiWidget ausdruecklich. Ein Bedienelement, das so haengt,
// ist DA, reagiert auf Klicks an seiner Stelle — und ist unsichtbar.
//
// Das ist in diesem Vorhaben dreimal passiert: die Kacheln, der
// Ziehgriff im Panadapterfenster, und zuletzt Zoomstreifen und
// Bedienflaeche im Panadapter. Jedes Mal hat es Tage gekostet, weil
// jede Pruefung „sichtbar" meldete: Qt weiss von der Regel nichts.
//
// Diese Pruefung geht den ganzen Widgetbaum des echten Hauptfensters
// durch und meldet jedes sichtbare, nicht-native Widget, das unter
// einem nativen haengt.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include "gui/MainWindow.h"

using namespace Longpath;

class TestNativeOverlayAudit : public QObject
{
    Q_OBJECT
private slots:
    void nothingNonNativeHidesInsideANativeWidget()
    {
        auto* mwp = new MainWindow();      // bewusst nicht abgeraeumt
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(600);

        // ── Das richtige Kriterium ───────────────────────────────────
        //
        // Nicht „haengt unter einem nativen Vorfahren": Qt macht JEDEN
        // Vorfahren eines nativen Widgets ebenfalls nativ, und damit
        // meldet diese Regel 87 voellig harmlose Regler in der
        // Applet-Spalte (deren Container ein MeterWidget enthaelt).
        // Beim ersten Lauf am 2026-08-20 genau das passiert.
        //
        // Es geht um Ueberdeckung: ein nicht-natives Widget, dessen
        // Flaeche sich mit der eines QRhiWidget UEBERSCHNEIDET. Nur
        // dort zeichnet das native NSView darueber, und nur dort ist
        // etwas da und trotzdem nicht zu sehen.
        QList<QWidget*> rhi;
        for (QWidget* w : mwp->findChildren<QWidget*>()) {
            if (w && w->isVisible() && w->inherits("QRhiWidget")) {
                rhi << w;
            }
        }
        qDebug() << "QRhi-Flaechen im Fenster:" << rhi.size();

        QStringList offenders;
        for (QWidget* w : mwp->findChildren<QWidget*>()) {
            if (!w || !w->isVisible() || w->isWindow()) { continue; }
            if (w->testAttribute(Qt::WA_NativeWindow)) { continue; }
            if (w->testAttribute(Qt::WA_TransparentForMouseEvents)) { continue; }
            if (w->width() < 4 || w->height() < 4) { continue; }

            // ── Und noch eine Stufe genauer ──────────────────────────
            //
            // Kinder eines nativen Widgets sind sicher: es hat eine
            // eigene Flaeche und zeichnet sie hinein. Der Zoomstreifen
            // war nur deshalb unsichtbar, weil sein naechster nativer
            // Vorfahre der PANADAPTER war — und dessen Flaeche gehoert
            // dem QRhi-Rendern, nicht dem Widgetbaum.
            //
            // Beim zweiten Lauf am 2026-08-20 meldete die Pruefung
            // sonst 14 Elemente, davon dreizehn Kinder der (nativen)
            // Bedienflaeche, die sehr wohl zu sehen sind.
            QWidget* nearestNative = nullptr;
            for (QWidget* a = w->parentWidget(); a; a = a->parentWidget()) {
                if (a->testAttribute(Qt::WA_NativeWindow) || a->isWindow()) {
                    nearestNative = a; break;
                }
            }

            const QRect wr(w->mapToGlobal(QPoint(0, 0)), w->size());
            for (QWidget* r : rhi) {
                if (r == w || w->isAncestorOf(r)) { continue; }
                if (nearestNative != r) { continue; }
                const QRect rr(r->mapToGlobal(QPoint(0, 0)), r->size());
                if (!wr.intersects(rr)) { continue; }
                offenders << QStringLiteral("%1 (%2) ueber %3")
                                 .arg(QString::fromLatin1(
                                          w->metaObject()->className()),
                                      w->objectName().isEmpty()
                                          ? QStringLiteral("-")
                                          : w->objectName(),
                                      QString::fromLatin1(
                                          r->metaObject()->className()));
                break;
            }
        }
        offenders.removeDuplicates();
        for (const QString& o : offenders) { qDebug().noquote() << "  " << o; }

        QVERIFY2(offenders.isEmpty(),
                 qPrintable(QStringLiteral(
                     "%1 nicht-native Bedienelemente haengen in einem "
                     "nativen Widget und sind auf macOS unsichtbar "
                     "(SpectrumWidget.cpp:551)").arg(offenders.size())));
    }
};
QTEST_MAIN(TestNativeOverlayAudit)
#include "tst_native_overlay_audit.moc"
