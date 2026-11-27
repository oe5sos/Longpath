// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Zoom geht an die Flex (2026-11-27).
//
// Anlass: Martins Protokoll longpath-20261127-102325.log. Kachel
// 20.9586–21.1892 MHz ueber 2460 Bins (~94 Hz je Bin), Fenster 7,9 kHz
// auf 1298 Punkten -- rund 84 Bins, jeder 15 Punkte breit. Betreiber:
// „warum ist das blau so pixelig?". Longpath hatte der Maschine nie eine
// Panadapterbreite geschickt; gezoomt wurde nur lokal in den 200 kHz.
//
// Dieser Stand haelt fest:
//   1. Die Entscheidung (core/FlexZoomZug.h): welcher Ausschnitt fuer
//      welches Fenster, wann ein Zug noetig ist, wann das Fenster bleibt.
//   2. Den Draht: ein Zoom ergibt GENAU EIN
//        display pan set <id> center=<MHz> bandwidth=<MHz>
//      mit der Mitte und Breite des Fensters, Mitte und Breite in einem
//      Befehl (AetherSDR RadioModel.cpp:5513 [@d58e2b8a]).
//   3. Das Echo: meldet die Maschine danach denselben Ausschnitt, kommt
//      KEIN zweiter Befehl und keine zweite Ausschnittsmeldung.
//   4. Den Kreis, wie MainWindow_Flex ihn faehrt: nach der Rueckmeldung
//      steht das Fenster still, und die naechste Entscheidung schweigt.

#include <QtTest>

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "core/FlexRadioConnection.h"
#include "core/FlexRadioDiscovery.h"
#include "core/FlexZoomZug.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Eine falsche Maschine auf 127.0.0.1, wie in tst_flex_stufe1: der
// Rundlauf durch den echten Socket ist der Punkt -- ohne Verbindung
// sendet `sendCommand` nichts, und kein Rueckruf wird faellig.
class FalscheMaschine : public QObject
{
    Q_OBJECT
public:
    explicit FalscheMaschine(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            m_client = m_server.nextPendingConnection();
            connect(m_client, &QTcpSocket::readyRead, this, &FalscheMaschine::lesen);
            schreiben(QStringLiteral("V1.4.0.0"));
            schreiben(QStringLiteral("H5081495E"));
        });
    }

    bool hoeren() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    QMap<QString, QString> antworten;
    QMap<QString, QStringList> nachschub;
    QStringList empfangeneBefehle;

    void schreiben(const QString& zeile)
    {
        if (m_client) {
            m_client->write((zeile + QLatin1Char('\n')).toUtf8());
            m_client->flush();
        }
    }

    QStringList befehleMitBreite() const
    {
        QStringList out;
        for (const QString& b : empfangeneBefehle) {
            if (b.startsWith(QLatin1String("display pan set"))
                && b.contains(QLatin1String("bandwidth="))) {
                out << b;
            }
        }
        return out;
    }

signals:
    void befehlGesehen(const QString& command);

private slots:
    void lesen()
    {
        m_puffer.append(m_client->readAll());
        int nl;
        while ((nl = m_puffer.indexOf('\n')) >= 0) {
            const QString zeile = QString::fromUtf8(m_puffer.left(nl)).trimmed();
            m_puffer.remove(0, nl + 1);
            if (zeile.isEmpty() || !zeile.startsWith(QLatin1Char('C'))) { continue; }
            const int pipe = zeile.indexOf(QLatin1Char('|'));
            if (pipe < 0) { continue; }
            const quint32 seq = zeile.mid(1, pipe - 1).toUInt();
            const QString befehl = zeile.mid(pipe + 1);
            empfangeneBefehle << befehl;
            emit befehlGesehen(befehl);

            QString antwort = QStringLiteral("0|");
            QString treffer;
            for (auto it = antworten.cbegin(); it != antworten.cend(); ++it) {
                if (befehl.startsWith(it.key())) {
                    antwort = it.value();
                    treffer = it.key();
                    break;
                }
            }
            schreiben(QStringLiteral("R%1|%2").arg(seq).arg(antwort));
            if (!treffer.isEmpty()) {
                for (const QString& extra : nachschub.value(treffer)) {
                    schreiben(extra);
                }
            }
        }
    }

private:
    QTcpServer  m_server;
    QTcpSocket* m_client{nullptr};
    QByteArray  m_puffer;
};

// Die Anmeldefolge, wie in tst_flex_stufe1::radioModelOeffnetDenFlexKanal.
void richteAnmeldungEin(FalscheMaschine& radio)
{
    radio.antworten.insert(QStringLiteral("client program"),
                           QStringLiteral("10000002|unknown client program Longpath"));
    radio.antworten.insert(QStringLiteral("client gui"),
                           QStringLiteral("0|11112222-3333-4444-5555-666677778888"));
    radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));
    radio.antworten.insert(QStringLiteral("display panafall create"),
                           QStringLiteral("0|0x40000000,0x42000000"));
    radio.antworten.insert(QStringLiteral("slice create"), QStringLiteral("0|0"));
    radio.nachschub.insert(QStringLiteral("slice create"),
        {QStringLiteral("S5081495E|slice 0 in_use=1 RF_frequency=21.074000 mode=DIGU")});
}

RadioInfo flexInfo(quint16 port)
{
    RadioInfo info;
    info.protocol   = ProtocolVersion::Flex;
    info.boardType  = HPSDRHW::Flex8400;
    info.name       = QStringLiteral("FLEX-8400 (Pruefstand)");
    info.macAddress = QStringLiteral("00:1C:2D:05:34:28");
    info.address    = QHostAddress(QHostAddress::LocalHost);
    info.port       = port;
    return info;
}

const QString kPan = QStringLiteral("0x40000000");

} // namespace

class TestFlexZoomZug : public QObject
{
    Q_OBJECT

private slots:
    // ─── 1. Die Entscheidung ───────────────────────────────────────

    // Martins Fenster vom 27.11.: 7,9 kHz. Die Maschine bekommt ihre
    // Mindestbreite, die Mitte bleibt die des Fensters.
    void schmalesFensterBekommtDieMindestbreite()
    {
        const auto a = FlexZoomZug::wunschFuerFenster(21'075'550.0, 7'900.0);
        QVERIFY(a.gueltig());
        QCOMPARE(a.breiteHz, FlexZoomZug::kMindestBreiteHz);
        QCOMPARE(a.mitteHz, 21'075'550.0);
    }

    void breitesFensterBehaeltSeineBreite()
    {
        const auto a = FlexZoomZug::wunschFuerFenster(14'200'000.0, 48'000.0);
        QCOMPARE(a.breiteHz, 48'000.0);
        QCOMPARE(a.mitteHz, 14'200'000.0);
        // Ueber die Hoechstbreite hinaus nicht.
        const auto b = FlexZoomZug::wunschFuerFenster(14'200'000.0, 9.0e6);
        QCOMPARE(b.breiteHz, FlexZoomZug::kHoechstBreiteHz);
    }

    // Was die Maschine meldet, gilt vor der Vorgabe.
    void gemeldeteGrenzenGelten()
    {
        const auto a = FlexZoomZug::wunschFuerFenster(7'074'000.0, 3'500.0,
                                                      1'230.0, 14'745'600.0);
        QCOMPARE(a.breiteHz, 3'500.0);
    }

    // Der untere Rand bleibt ueber 0 Hz.
    void untererRandBleibtUeberNull()
    {
        const auto a = FlexZoomZug::wunschFuerFenster(100'000.0, 400'000.0);
        QCOMPARE(a.breiteHz, 400'000.0);
        QCOMPARE(a.mitteHz, 200'000.0);
    }

    void zugNurBeiAndererBreite()
    {
        QVERIFY(FlexZoomZug::zugNoetig(10'000.0, 200'000.0));     // hinein
        QVERIFY(FlexZoomZug::zugNoetig(200'000.0, 10'000.0));     // heraus
        QVERIFY(!FlexZoomZug::zugNoetig(10'000.0, 10'000.0));     // gleich
        QVERIFY(!FlexZoomZug::zugNoetig(200'500.0, 200'000.0));   // unter 1 %
        QVERIFY(!FlexZoomZug::zugNoetig(10'000.0, 0.0));          // noch nichts gemeldet
    }

    void fensterBleibtNurBeiUnseremZoom()
    {
        QVERIFY(FlexZoomZug::fensterBleibt(10'000.0, 10'000.0, true));
        QVERIFY(!FlexZoomZug::fensterBleibt(10'000.0, 10'000.0, false)); // fremd
        QVERIFY(!FlexZoomZug::fensterBleibt(10'000.0, 0.0, true));       // kein Zoomzug
        QVERIFY(!FlexZoomZug::fensterBleibt(200'000.0, 10'000.0, true)); // nur Mitte gezogen
    }

    void zoomDeckeIstWasDieMaschineKann()
    {
        QCOMPARE(FlexZoomZug::zoomDecke(0.0), FlexZoomZug::kHoechstBreiteHz);
        QCOMPARE(FlexZoomZug::zoomDecke(14'745'600.0), 14'745'600.0);
    }

    // ─── 4. Der Kreis, wie MainWindow_Flex ihn faehrt ──────────────
    //
    // Fenster gezoomt -> Zug -> Rueckmeldung als eigener Zug -> Fenster
    // bleibt -> naechste Entscheidung. Die muss schweigen, sonst ist es
    // ein Echo-Kreislauf.
    void nachDerRueckmeldungSchweigtDieEntscheidung_data()
    {
        QTest::addColumn<double>("fensterBreite");
        QTest::addColumn<double>("panBreite");
        QTest::newRow("hinein unter die Mindestbreite (27.11.)") << 7'900.0 << 200'000.0;
        QTest::newRow("hinein ueber der Mindestbreite") << 48'000.0 << 200'000.0;
        QTest::newRow("heraus") << 400'000.0 << 10'000.0;
    }

    void nachDerRueckmeldungSchweigtDieEntscheidung()
    {
        QFETCH(double, fensterBreite);
        QFETCH(double, panBreite);
        const double fensterMitte = 21'075'550.0;

        const auto wunsch = FlexZoomZug::wunschFuerFenster(fensterMitte, fensterBreite);
        QVERIFY(FlexZoomZug::zugNoetig(wunsch.breiteHz, panBreite));
        const double zugBreite = wunsch.breiteHz;

        // Die Rueckmeldung (RadioModel::uebernimmFlexPanAusschnitt) traegt
        // genau die geschickte Breite und ist ein eigener Zug.
        const double gemeldet = wunsch.breiteHz;
        QVERIFY(FlexZoomZug::fensterBleibt(gemeldet, zugBreite, true));

        // Das Fenster blieb -- also dieselbe Eingabe, aber der neue Pan.
        const auto nochmal = FlexZoomZug::wunschFuerFenster(fensterMitte, fensterBreite);
        QVERIFY(!FlexZoomZug::zugNoetig(nochmal.breiteHz, gemeldet));
    }

    // ─── 2. + 3. Der Draht und das Echo ────────────────────────────

    void einZoomEinBefehlUndKeinEcho()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        richteAnmeldungEin(radio);

        RadioModel model;
        QSignalSpy frequenz(&model, &RadioModel::flexSliceFrequencyChanged);
        model.connectToRadio(flexInfo(radio.port()));
        QVERIFY(model.flexConnection() != nullptr);
        QVERIFY(frequenz.wait(8000));

        // Die Maschine meldet ihren Panadapter: 200 kHz um 21,074 MHz,
        // mit ihren Breitengrenzen.
        QSignalSpy ausschnitt(&model, &RadioModel::flexAusschnittGeaendert);
        radio.schreiben(QStringLiteral(
            "S5081495E|display pan %1 center=21.074000 bandwidth=0.200000 "
            "min_bw=0.001230 max_bw=14.745600 y_pixels=700").arg(kPan));
        QTRY_COMPARE(ausschnitt.count(), 1);
        QCOMPARE(ausschnitt.at(0).at(2).toBool(), false);     // die Maschine
        QCOMPARE(model.flexPanMindestBreiteHz(), 1'230.0);
        QCOMPARE(model.flexPanHoechstBreiteHz(), 14'745'600.0);
        QTRY_VERIFY(model.flexConnection()->isConnected());

        // Der Zoom: 7,9 kHz um 21,07555 MHz. Mit den gemeldeten Grenzen
        // geht genau diese Breite hinaus.
        const auto wunsch = FlexZoomZug::wunschFuerFenster(
            21'075'550.0, 7'900.0, model.flexPanMindestBreiteHz(),
            model.flexPanHoechstBreiteHz());
        radio.empfangeneBefehle.clear();
        ausschnitt.clear();
        model.zieheFlexPanadapterAuf(wunsch.mitteHz, wunsch.breiteHz);

        QTRY_COMPARE(ausschnitt.count(), 1);
        QCOMPARE(radio.befehleMitBreite(),
                 QStringList{QStringLiteral(
                     "display pan set %1 center=21.075550 bandwidth=0.007900").arg(kPan)});
        QCOMPARE(ausschnitt.at(0).at(0).toDouble(), 21'071'600.0);
        QCOMPARE(ausschnitt.at(0).at(1).toDouble(), 21'079'500.0);
        QCOMPARE(ausschnitt.at(0).at(2).toBool(), true);      // unser Zug

        // Das Echo: die Maschine meldet denselben Ausschnitt.
        radio.schreiben(QStringLiteral(
            "S5081495E|display pan %1 center=21.075550 bandwidth=0.007900").arg(kPan));
        QTest::qWait(300);
        QCOMPARE(ausschnitt.count(), 1);                       // keine zweite Meldung
        QCOMPARE(radio.befehleMitBreite().size(), 1);          // kein zweiter Befehl

        model.disconnectFromRadio();
    }

    // Lehnt die Maschine ab, bleibt der Ausschnitt, wie er war --
    // sonst behauptete Longpath einen Zoom, den sie nie genommen hat.
    void abgelehnterZoomAendertNichts()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        richteAnmeldungEin(radio);
        radio.antworten.insert(QStringLiteral("display pan set %1 center=").arg(kPan),
                               QStringLiteral("50000016|"));

        RadioModel model;
        QSignalSpy frequenz(&model, &RadioModel::flexSliceFrequencyChanged);
        model.connectToRadio(flexInfo(radio.port()));
        QVERIFY(frequenz.wait(8000));

        QSignalSpy ausschnitt(&model, &RadioModel::flexAusschnittGeaendert);
        radio.schreiben(QStringLiteral(
            "S5081495E|display pan %1 center=21.074000 bandwidth=0.200000").arg(kPan));
        QTRY_COMPARE(ausschnitt.count(), 1);
        // Ohne gemeldete Grenzen gelten die von AetherSDR.
        QCOMPARE(model.flexPanMindestBreiteHz(), FlexZoomZug::kMindestBreiteHz);

        ausschnitt.clear();
        radio.empfangeneBefehle.clear();
        model.zieheFlexPanadapterAuf(21'075'550.0, 10'000.0);
        QTRY_COMPARE(radio.befehleMitBreite().size(), 1);
        QTest::qWait(300);
        QCOMPARE(ausschnitt.count(), 0);

        model.disconnectFromRadio();
    }
};

QTEST_MAIN(TestFlexZoomZug)
#include "tst_flex_zoomzug.moc"
