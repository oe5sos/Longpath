// tests/tst_status_badge.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "gui/widgets/StatusBadge.h"

using namespace NereusSDR;

class TstStatusBadge : public QObject {
    Q_OBJECT

private slots:
    void defaultsToInfoVariant() {
        StatusBadge b;
        QCOMPARE(b.variant(), StatusBadge::Variant::Info);
        QVERIFY(b.icon().isEmpty());
        QVERIFY(b.label().isEmpty());
    }

    void setIconStoresAndRenders() {
        StatusBadge b;
        b.setIcon(QStringLiteral("~"));
        QCOMPARE(b.icon(), QStringLiteral("~"));
    }

    void setLabelStoresAndRenders() {
        StatusBadge b;
        b.setLabel(QStringLiteral("USB"));
        QCOMPARE(b.label(), QStringLiteral("USB"));
    }

    // Frueher standen hier zwei Hexwerte (#5fff8a, #ff6060) aus der
    // Zeit vor dem Hausstil. Ein Test, der eine Farbe festnagelt, faellt
    // bei jedem Feinschliff um und sagt nichts darueber, ob das Widget
    // stimmt.
    //
    // Geprueft wird jetzt die Zusicherung, die die Datei selbst
    // aufstellt: Beschriftung und SVG-Symbol tragen DIESELBE Farbe.
    // variantForegroundColor() ist die eine Quelle; steht ihr Wert
    // nicht im Stylesheet, sind die beiden auseinander -- und genau das
    // war zwischen dem Hausstil-Umbau und dem 2026-08-17 der Fall.
    void styleFollowsTheVariantForegroundColour() {
        StatusBadge b;
        for (auto v : {StatusBadge::Variant::Info, StatusBadge::Variant::On,
                       StatusBadge::Variant::Off, StatusBadge::Variant::Warn,
                       StatusBadge::Variant::Tx}) {
            b.setVariant(v);
            QCOMPARE(b.variant(), v);
            const QString want = b.variantForegroundColor().name();
            QVERIFY2(b.styleSheet().contains(want),
                     qPrintable(QStringLiteral(
                         "Variante %1: Symbolfarbe %2 steht nicht im "
                         "Stylesheet — die beiden Tabellen sind wieder "
                         "auseinander. Stylesheet: %3")
                             .arg(static_cast<int>(v)).arg(want,
                                  b.styleSheet())));
        }
    }

    // Der Grund kommt aus einem BENANNTEN Paar, nicht aus einer
    // Deckkraft. Geprueft wird das an der Eigenschaft, die eine
    // Deckkraft nicht haben kann: der Grund steht als fester Wert im
    // Stylesheet und nicht als rgba(...) mit Alphaanteil.
    void backgroundIsANamedColourNotAnAlphaWash()
    {
        StatusBadge b;
        for (auto v : {StatusBadge::Variant::Info, StatusBadge::Variant::On,
                       StatusBadge::Variant::Off, StatusBadge::Variant::Warn,
                       StatusBadge::Variant::Tx}) {
            b.setVariant(v);
            const QString qss = b.styleSheet();
            QVERIFY2(!qss.contains(QStringLiteral("rgba(")),
                     qPrintable(QStringLiteral(
                         "Variante %1 traegt wieder eine Deckkraft: %2")
                             .arg(static_cast<int>(v)).arg(qss)));
            QVERIFY2(qss.contains(QStringLiteral("background: #")),
                     qPrintable(QStringLiteral(
                         "Variante %1 hat keinen benannten Grund: %2")
                             .arg(static_cast<int>(v)).arg(qss)));
        }
    }

    // Und jede Variante bekommt ihren EIGENEN Grund. Ein Paar, das
    // sich den Grund mit einer anderen Variante teilt, unterscheidet
    // nur noch an der Schrift — und dafuer braucht es keinen Grund.
    void everyVariantHasItsOwnBackground()
    {
        StatusBadge b;
        QSet<QString> seen;
        for (auto v : {StatusBadge::Variant::Info, StatusBadge::Variant::On,
                       StatusBadge::Variant::Off, StatusBadge::Variant::Warn,
                       StatusBadge::Variant::Tx}) {
            b.setVariant(v);
            const QString qss = b.styleSheet();
            const int at = qss.indexOf(QStringLiteral("background: #"));
            QVERIFY(at >= 0);
            const QString bg = qss.mid(at + 12, 7);
            QVERIFY2(!seen.contains(bg),
                     qPrintable(QStringLiteral("Grund %1 wird von zwei "
                                               "Varianten benutzt").arg(bg)));
            seen.insert(bg);
        }
    }

    // Die Varianten muessen unterscheidbar bleiben. Das ist der Rest
    // dessen, was "txVariantUsesRed" eigentlich meinte: nicht "genau
    // dieses Rot", sondern "nicht dieselbe Farbe wie die anderen".
    void everyVariantHasItsOwnColour() {
        StatusBadge b;
        QSet<QString> seen;
        for (auto v : {StatusBadge::Variant::Info, StatusBadge::Variant::On,
                       StatusBadge::Variant::Off, StatusBadge::Variant::Warn,
                       StatusBadge::Variant::Tx}) {
            b.setVariant(v);
            const QString c = b.variantForegroundColor().name();
            QVERIFY2(!seen.contains(c),
                     qPrintable(QStringLiteral("Farbe %1 wird von zwei "
                                               "Varianten benutzt").arg(c)));
            seen.insert(c);
        }
    }

    void leftClickEmitsClicked() {
        StatusBadge b;
        b.resize(40, 18);
        QSignalSpy spy(&b, &StatusBadge::clicked);
        QTest::mouseClick(&b, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }

    void rightClickEmitsRightClicked() {
        StatusBadge b;
        b.resize(40, 18);
        QSignalSpy spy(&b, &StatusBadge::rightClicked);
        QTest::mouseClick(&b, Qt::RightButton);
        QCOMPARE(spy.count(), 1);
    }

    void identityRoundTrip() {
        StatusBadge b;
        b.setIcon(QStringLiteral("⌁"));
        b.setLabel(QStringLiteral("NR2"));
        b.setVariant(StatusBadge::Variant::On);
        QCOMPARE(b.icon(), QStringLiteral("⌁"));
        QCOMPARE(b.label(), QStringLiteral("NR2"));
        QCOMPARE(b.variant(), StatusBadge::Variant::On);
    }
};

QTEST_MAIN(TstStatusBadge)
#include "tst_status_badge.moc"
