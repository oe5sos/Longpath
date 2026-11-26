// =================================================================
// tests/tst_kombi_ausrichtung_rx_phone.cpp  (Longpath)
// =================================================================
//
// Der RX- und der Phone/CW-Reiter im Combined-Feld sitzen oben, wie der
// TX-Reiter -- kein Leerraum zwischen den Zeilen, kein gestreckter
// Anzeigekasten. Martins Befund (2026-11-24): „rx schlecht, phone auch
// schlecht". Gemessen an der Geometrie, nicht am Bild.
//
// Modification history (Longpath):
//   2026-11-24 — Martin Fischer (OE5SOS), AI-assisted via Anthropic
//                 Claude.
// =================================================================

#include <QtTest>

#include <QLabel>

#include "gui/applets/KombiApplet.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TestKombiAusrichtungRxPhone : public QObject
{
    Q_OBJECT

private slots:
    // Der Anzeigekasten „-50 dB" (DEXP) wird nicht auf die Hoehe der
    // Schieberspalte gestreckt: feste 20 px, mittig in der Zeile.
    void dexpAnzeigeIstNichtGestreckt()
    {
        // Feld zuerst: es wird zuletzt zerstoert, die Panels gehoeren ihm.
        KombiApplet feld(nullptr);
        RadioModel radio;
        PhoneCwApplet phone(&radio, nullptr);
        feld.resize(445, 885);
        feld.nimmAuf(&phone);
        feld.show();
        QTest::qWaitForWindowExposed(&feld);

        QLabel* dexp = phone.findChild<QLabel*>(QStringLiteral("PhoneCwDexpValue"));
        QVERIFY2(dexp, "DEXP-Anzeige nicht gefunden");
        QCOMPARE(dexp->height(), 20);
    }

    // Die Durchlassflaeche im RX-Reiter steht oben links unter dem
    // Schritt, nicht unten im Feld.
    void rxDurchlassSitztOben()
    {
        KombiApplet feld(nullptr);
        RadioModel radio;
        int id = radio.sliceById(0) ? 0 : radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);

        RxApplet rx(slice, &radio, nullptr);
        feld.resize(445, 885);
        feld.nimmAuf(&rx);
        feld.show();
        QTest::qWaitForWindowExposed(&feld);

        auto* pane = rx.findChild<BandwidthFilterPane*>();
        QVERIFY2(pane, "Durchlassflaeche nicht gefunden");
        const QPoint oben = pane->mapTo(&feld, QPoint(0, 0));
        QVERIFY2(oben.y() < feld.height() / 2,
                 qPrintable(QStringLiteral("Durchlassflaeche bei y=%1, Feldhoehe %2")
                                .arg(oben.y()).arg(feld.height())));
    }
};

QTEST_MAIN(TestKombiAusrichtungRxPhone)
#include "tst_kombi_ausrichtung_rx_phone.moc"
