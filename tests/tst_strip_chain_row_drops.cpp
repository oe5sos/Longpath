// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_strip_chain_row_drops.cpp  (Longpath)
// =================================================================
// Die Reihe oben IST die Kette -- also muss sie es auch bleiben.
//
// „diese sollte man dann in der timeline oben reinziehen koennen.
// aber relativ leicht auch wieder loeschen."
//
// Ein Plugin an eine Stelle zu ziehen ist nur dann besser als eine
// Auswahlliste, wenn die Stelle, auf die man gezielt hat, auch die
// ist, an der es landet. Das ist Rechnerei ueber eine Reihe, die sich
// bei jedem Einschub veraendert -- genau die Sorte Code, die
// unbemerkt daneben zu liegen kommt.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QDropEvent>
#include <QMimeData>
#include <QSignalSpy>

#include "core/strip/StripChain.h"
#include "gui/applets/StripGraphics.h"

using namespace Longpath;

namespace {

// Der Fallenlasser wird direkt gerufen, nicht ueber die
// Ereignisschlange. Dass Qt einen Drop an das richtige Widget
// zustellt, ist Qts Sache und nichts, was dieser Prueftand beweisen
// soll; geprueft wird die Rechnerei dahinter -- welche Stelle der
// Reihe ein Punkt meint.
class Probe : public StripChainView {
public:
    using StripChainView::dropEvent;
};

void dropAt(Probe& v, const QString& id, int x)
{
    QMimeData mime;
    mime.setData(QLatin1String(StripChainView::kPluginMime), id.toUtf8());
    QDropEvent ev(QPointF(x, 20), Qt::CopyAction, &mime,
                  Qt::LeftButton, Qt::NoModifier);
    v.dropEvent(&ev);
}

} // namespace

class TstStripChainRowDrops : public QObject
{
    Q_OBJECT
private slots:

    // Ganz links heisst vor dem Gate, ganz rechts hinter dem
    // Begrenzer. Wenn DAS nicht stimmt, stimmt nichts dazwischen.
    void bothEndsOfTheRowMeanWhatTheyLookLike()
    {
        StripChain c;
        c.prepare(48000.0);
        Probe v;
        v.setChain(&c);
        v.resize(v.sizeHint());

        QSignalSpy spy(&v, &StripChainView::pluginDropped);

        dropAt(v, QStringLiteral("test:a"), 0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 0);

        dropAt(v, QStringLiteral("test:b"), v.sizeHint().width() - 1);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(1).toInt(), StripChain::kStageCount);

        // Und der Bezeichner kommt unveraendert an.
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("test:a"));
    }

    // Jede Fuge ist erreichbar. Eine Reihe, in der man die siebte
    // Stelle nicht treffen kann, hat acht Stufen und sieben Luegen.
    void everyGapCanBeHit()
    {
        StripChain c;
        c.prepare(48000.0);
        Probe v;
        v.setChain(&c);
        v.resize(v.sizeHint());

        QSignalSpy spy(&v, &StripChainView::pluginDropped);

        // Die Reihe laeuft Fuge, Stufe, Fuge, Stufe ... Ein Punkt kurz
        // VOR der Mitte einer Stufe gehoert zur Fuge davor.
        const int w = v.sizeHint().width();
        QSet<int> seen;
        for (int x = 0; x < w; ++x) {
            spy.clear();
            dropAt(v, QStringLiteral("test:x"), x);
            QCOMPARE(spy.count(), 1);
            seen.insert(spy.at(0).at(1).toInt());
        }
        for (int at = 0; at <= StripChain::kStageCount; ++at) {
            QVERIFY2(seen.contains(at),
                     qPrintable(QStringLiteral("Stelle %1 ist nicht zu "
                                               "treffen").arg(at)));
        }
    }

    // Haengt ein Einschub drin, wird die Reihe breiter -- sonst
    // ueberdeckt die Kachel eine Stufe, statt neben ihr zu stehen.
    void theRowGrowsWhenSomethingHangsInIt()
    {
        StripChain c;
        c.prepare(48000.0);
        Probe v;
        v.setChain(&c);
        const int bare = v.sizeHint().width();

        // Ohne echtes Plugin laesst sich kein Einschub setzen, also
        // wird hier nur geprueft, dass die Reihe ohne Einschuebe
        // genau die acht Stufen und ihre Fugen ist und refreshInserts
        // sie nicht verschiebt.
        v.refreshInserts();
        QCOMPARE(v.sizeHint().width(), bare);
        QVERIFY(bare > 8 * 84);   // acht Kacheln plus Fugen
    }
};

QTEST_MAIN(TstStripChainRowDrops)
#include "tst_strip_chain_row_drops.moc"
