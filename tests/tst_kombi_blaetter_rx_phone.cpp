// =================================================================
// tests/tst_kombi_blaetter_rx_phone.cpp  (Longpath)
// =================================================================
//
// Werkzeug, kein Pruefer: schreibt die Reiter RX, Phone/CW und TX des
// Combined-Feldes als PNG-Blaetter, damit der Vorher-Nachher-Vergleich
// am Bild gemacht werden kann. Laeuft nur, wenn LONGPATH_BLATT_DIR
// gesetzt ist; sonst wird er uebersprungen. Offscreen, eigene
// Sandkiste -- nie Martins Einstellungen, nie ein laufendes Programm.
//
// Modification history (Longpath):
//   2026-11-24 — Martin Fischer (OE5SOS), AI-assisted via Anthropic
//                 Claude.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QVBoxLayout>

#include "gui/applets/KombiApplet.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TestKombiBlaetterRxPhone : public QObject
{
    Q_OBJECT

private slots:
    void schreibeBlaetter()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_BLATT_DIR");
        if (ziel.isEmpty()) {
            QSKIP("Werkzeug: LONGPATH_BLATT_DIR nicht gesetzt");
        }
        QDir().mkpath(ziel);

        const QString praefix = qEnvironmentVariable("LONGPATH_BLATT_PRAEFIX");
        const int breite = qEnvironmentVariable("LONGPATH_BLATT_BREITE", "445").toInt();
        const int hoehe = qEnvironmentVariable("LONGPATH_BLATT_HOEHE", "885").toInt();

        // Feld zuerst deklariert, also zuletzt zerstoert: die Panels
        // gehoeren nach dem Umhaengen dem Feld.
        KombiApplet feld(nullptr);
        RadioModel radio;
        int id = radio.sliceById(0) ? 0 : radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);

        RxApplet rx(slice, &radio, nullptr);
        PhoneCwApplet phone(&radio, nullptr);
        TxApplet tx(&radio, nullptr);

        // Blatt B: die Durchlassflaeche aus dem RX-Reiter genommen (nur
        // fuer das Bild; das Programm bleibt unveraendert).
        if (qEnvironmentVariableIntValue("LONGPATH_BLATT_OHNE_PANE") != 0) {
            if (auto* pane = rx.findChild<BandwidthFilterPane*>()) {
                pane->setVisible(false);
            }
        }

        feld.resize(breite, hoehe);
        feld.nimmAuf(&rx);
        feld.nimmAuf(&phone);
        feld.nimmAuf(&tx);
        feld.show();
        QTest::qWaitForWindowExposed(&feld);

        const struct { int seite; const char* name; } blaetter[] = {
            {0, "rx"}, {1, "phone"}, {2, "tx"}};
        for (const auto& b : blaetter) {
            feld.zeigeSeite(b.seite);
            QApplication::processEvents();
            QTest::qWait(60);
            const QPixmap bild = feld.grab();
            const QString pfad = ziel + QStringLiteral("/") + praefix
                                 + QLatin1String(b.name) + QStringLiteral(".png");
            QVERIFY(bild.save(pfad, "PNG"));
            qInfo() << "geschrieben:" << pfad;
        }
    }
};

QTEST_MAIN(TestKombiBlaetterRxPhone)
#include "tst_kombi_blaetter_rx_phone.moc"
