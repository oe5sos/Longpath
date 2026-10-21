// tests/tst_scan_fragt_gespeicherte_geraete.cpp
//
// Der ↻-Scan im Verbindungsfenster muss die GESPEICHERTEN Geräte auch
// gezielt fragen, nicht nur rundrufen.
//
// Anlass (2026-10-21): Die ANVELINA PRO 3 des Betreibers stand auf
// 172.30.30.26, sein Rechner auf 192.168.78.x. Im Verbindungsfenster
// blieb die Zeile grau -- "saved", "never seen" -- obwohl das Gerät
// lief: Rundrufe bleiben im eigenen Subnetz. Gemessen wurde am selben
// Tag, dass das Gerät auf eine GEZIELTE Abfrage an 172.30.30.26:1024
// sofort antwortet (60 Bytes, MAC 68:27:19:98:cb:71, Board 0x05) und
// auf Ping (3/3, 40 ms, kein ARP-Eintrag -- also über den Router).
//
// `RadioDiscovery::probeAddress()` konnte das längst, wurde aber nur
// vom Menüpunkt Connect und vom Dialog "Add Manually" benutzt. Dieser
// Prüfstand hält fest, dass auch der Scan sie benutzt -- sonst fällt
// die Verdrahtung beim nächsten Umbau still wieder heraus.

#include <QPushButton>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "gui/ConnectionPanel.h"
#include "models/RadioModel.h"

using namespace Longpath;

// Ein Horcher, der nur zählt, was bei ihm ankommt -- antworten muss er
// für diese Frage nicht.
class StillerHorcher : public QObject {
    Q_OBJECT
public:
    explicit StillerHorcher(QObject* parent = nullptr) : QObject(parent) {
        m_socket = new QUdpSocket(this);
        m_socket->bind(QHostAddress::LocalHost, 0);
        connect(m_socket, &QUdpSocket::readyRead, this, [this] {
            while (m_socket->hasPendingDatagrams()) {
                QByteArray buf;
                buf.resize(int(m_socket->pendingDatagramSize()));
                m_socket->readDatagram(buf.data(), buf.size());
                m_pakete.append(buf);
            }
        });
    }
    quint16 port() const { return m_socket->localPort(); }
    int anzahl() const { return m_pakete.size(); }

private:
    QUdpSocket*       m_socket;
    QList<QByteArray> m_pakete;
};

class TestScanFragtGespeicherteGeraete : public QObject {
    Q_OBJECT

private slots:
    void init() { RadioDiscovery::clearHoldOffForTest(); }

    void scanFragtDieGespeicherteAdresseGezielt()
    {
        StillerHorcher horcher;
        QVERIFY(horcher.port() != 0);

        // Ein gespeichertes Gerät, das genau dort steht, wo der Horcher
        // sitzt -- als stünde es in einem anderen Subnetz: per Rundruf
        // unerreichbar, per gezielter Frage sehr wohl.
        RadioInfo info;
        info.name       = QStringLiteral("ANVELINA PRO 3");
        info.macAddress = QStringLiteral("68:27:19:98:cb:71");
        info.address    = QHostAddress(QHostAddress::LocalHost);
        info.port       = horcher.port();
        info.boardType  = HPSDRHW::Orion;
        info.protocol   = ProtocolVersion::Protocol2;
        AppSettings::instance().saveRadio(info, /*pinToMac=*/false);

        RadioModel model;
        ConnectionPanel panel(&model);

        QPushButton* scan = nullptr;
        for (QPushButton* b : panel.findChildren<QPushButton*>()) {
            if (b->text().contains(QStringLiteral("Scan"))) {
                scan = b;
                break;
            }
        }
        QVERIFY2(scan, "Der Scan-Knopf wurde nicht gefunden");

        // Das Panel startet beim Oeffnen selbst einen Rundruf und sperrt
        // den Knopf, bis der durch ist. Wer sofort klickt, klickt ins
        // Leere -- genau daran ist dieser Pruefstand beim ersten Bau
        // haengengeblieben und meldete einen Fehler, den es nicht gab.
        QTRY_VERIFY_WITH_TIMEOUT(scan->isEnabled(), 20000);

        // Ab hier zaehlen: was der Erstscan selbst geschickt hat, geht
        // an die Rundruf-Adresse, nicht hierher -- aber die Zaehlung
        // soll von der Frage unabhaengig sein, nicht von ihrer Antwort.
        const int vorher = horcher.anzahl();

        scan->click();

        // Beim Horcher kann nur ankommen, was GEZIELT an seine Adresse
        // ging: ein Rundruf landet bei der Broadcast-Adresse.
        QTRY_VERIFY_WITH_TIMEOUT(horcher.anzahl() > vorher, 10000);

        AppSettings::instance().forgetRadio(info.macAddress);
    }
};

QTEST_MAIN(TestScanFragtGespeicherteGeraete)
#include "tst_scan_fragt_gespeicherte_geraete.moc"
