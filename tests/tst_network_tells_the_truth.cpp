// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wenn das Netz schuld ist, muss die App das SAGEN.
//
// Vorgeschichte, 2026-08-22: der Betreiber meldete "anvelina über das
// netzwerk ist katastrophal (audio)" — und hatte drei Tage lang die App
// im Verdacht. Gemessen war es die Funkstrecke: sein Anvelina Pro 3
// haengt am WLAN (2,4 GHz), die ARP-Aufloesung stand zeitweise auf
// (incomplete), und Protokoll 2 wiederholt kein einziges verlorenes
// Paket. Die App wusste das die ganze Zeit — sie schrieb es in ein
// Protokoll, das niemand sieht, und warf beim Scheitern sogar den
// Begruendungstext weg (Q_UNUSED(detail)).
//
// Diese Pruefung haelt die zwei Lehren fest.

#include <QtTest>

#include "gui/TitleBar.h"
#include "gui/StyleConstants.h"
#include "core/RadioConnection.h"

using namespace Longpath;

class TstNetworkTellsTheTruth : public QObject
{
    Q_OBJECT

private slots:
    void theWorstMomentIsTheOneThatCounts()
    {
        ConnectionSegment seg;
        seg.setState(ConnectionState::Connected);

        // Ein Stoss Verlust, dann wieder Ruhe — genau das Muster einer
        // belegten Funkstrecke.
        seg.setPacketLoss(0.0);
        seg.setPacketLoss(1.4);
        seg.setPacketLoss(0.0);
        seg.setPacketLoss(0.0);

        seg.resize(600, 30);
        QImage img(seg.size(), QImage::Format_ARGB32);
        img.fill(Qt::black);
        seg.render(&img);

        // Gesucht wird der HAUSFARBTON fuer "unbrauchbar"
        // (Style::kRedText, #f0dcdc) — NICHT gesaettigtes Rot.
        //
        // Die erste Fassung dieser Pruefung suchte kraeftiges Rot und
        // schlug fehl, obwohl die Anzeige richtig arbeitete: gesaettigtes
        // Rot ist im Hausstil der Bandkante und dem SWR vorbehalten, und
        // die Kopfleiste haelt sich daran. Der Test hatte unrecht, nicht
        // der Code.
        const QColor want(Style::kRedText);
        int red = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red()   - want.red())   < 24
                    && qAbs(c.green() - want.green()) < 24
                    && qAbs(c.blue()  - want.blue())  < 24) {
                    ++red;
                }
            }
        }
        QVERIFY2(red > 0,
                 "Der Verluststoss ist aus der Anzeige verschwunden — "
                 "dann misst sie den bequemen Augenblick statt des "
                 "Problems");
    }

    void aNewConnectionStartsWithACleanSlate()
    {
        // Der Betreiber am 2026-08-22 sah am ANAN-10/100 den
        // Verlustwert des ANDEREN Geraets. Ein fremder Messwert ist
        // schlimmer als gar keiner: er behauptet etwas ueber eine
        // Strecke, die nie gemessen wurde.
        ConnectionSegment seg;
        seg.setState(ConnectionState::Connected);
        seg.setPacketLoss(2.4);

        seg.resize(600, 30);
        auto redPixels = [&]() {
            QImage img(seg.size(), QImage::Format_ARGB32);
            img.fill(Qt::black);
            seg.render(&img);
            const QColor want(Style::kRedText);
            int n = 0;
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (qAbs(c.red()   - want.red())   < 24
                        && qAbs(c.green() - want.green()) < 24
                        && qAbs(c.blue()  - want.blue())  < 24) { ++n; }
                }
            }
            return n;
        };
        QVERIFY2(redPixels() > 0, "Der Wert steht gar nicht erst da");

        // Geraetewechsel: trennen und neu verbinden.
        seg.setState(ConnectionState::Disconnected);
        seg.setState(ConnectionState::Connected);
        QVERIFY2(redPixels() == 0,
                 "Nach dem Wechsel steht der Verlustwert des vorigen "
                 "Geraets noch da");
    }

    void aFailedConnectSaysWhy()
    {
        // Der Waechter darf nicht mehr stumm sein, und er darf nicht
        // nach zwei Sekunden aufgeben: auf einer belegten Funkstrecke
        // brauchen ARP und der Anlauf des Geraets laenger.
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/core/P2RadioConnection.h"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString hdr = QString::fromUtf8(f.readAll());
        QVERIFY2(!hdr.contains(QStringLiteral("kConnectTimeoutMs = 2000")),
                 "Zwei Sekunden sind fuer WLAN zu knapp — gemessen beim "
                 "Betreiber am 2026-08-22");

        QFile c(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(c.open(QIODevice::ReadOnly), qPrintable(c.fileName()));
        const QString src = QString::fromUtf8(c.readAll());
        QVERIFY2(src.contains(QStringLiteral("emit connectAttemptFailed")),
                 "Die Begruendung wird wieder weggeworfen — dann sieht "
                 "der Bediener nur 'Disconnected' und verdaechtigt die "
                 "App");
    }
};

QTEST_MAIN(TstNetworkTellsTheTruth)
#include "tst_network_tells_the_truth.moc"
