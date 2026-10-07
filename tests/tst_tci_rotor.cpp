// Prüfstand: den Rotor vom Telefon drehen — und vor allem, wann NICHT.
//
// Am anderen Ende der Leitung steht ein Mast mit einer Antenne darauf. Er
// dreht minutenlang ohne weiteres Zutun, und niemand steht daneben. Darum
// gilt hier dieselbe Strenge wie beim Senden und nicht die beim Loggen:
//
//     rotor:;           -> rotor_ist:<grad>,<zustand>,<frisch 0|1>;
//
// `<zustand>` ist seit dem 2026-10-07 ein NAME (getrennt, verbindet, bereit,
// dreht, fehler). Vorher war es die Nummer aus RotorController::State, und
// damit kannten zwei Stellen dieselbe Zaehlung -- diese Aufzaehlung und
// `handfunke/rotor.js`. Geprueft wurde das Feld ueberhaupt nicht.
//     rotor_to:<grad>;  -> rotor_ok:<grad>;   oder rotor_err:<grund>;
//     rotor_stop:;      -> rotor_ok:stop;     oder rotor_err:<grund>;
//
// Die Zusagen, die hier festgehalten sind:
//
//   * ABFRAGEN darf jeder angemeldete Client. Hinsehen bewegt nichts.
//   * DREHEN nur mit TciAllowRemoteRotor=True. Ab Werk steht das auf False,
//     und genau das ist der Punkt: wer die Vorgabe versehentlich umdreht,
//     fällt hier auf.
//   * Eine Ablehnung kommt MIT Grund. Ein stummes Nein ist auf einer
//     Fernbedienung nicht von einem Defekt zu unterscheiden — und bei einem
//     Rotor sieht man zehn Sekunden lang ohnehin nichts.
//   * "Es gibt hier keinen Rotor" ist eine andere Auskunft als "er steht auf
//     0 Grad". Nur eine davon heißt: such nicht weiter.
//   * Eine unbrauchbare Peilung dreht NICHT nach Norden (siehe
//     tst_rotor_peilung — `toDouble()` macht aus "" eine 0, und 0 ist Nord).

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>

#include "core/AppSettings.h"
#include "core/RotorController.h"
#include "core/TciServer.h"
#include "models/RadioModel.h"
#include "TciBurstHelfer.h"

using namespace Longpath;

namespace {
constexpr char kToken[] = "PRUEFTOKEN1234";

/// Ein Rotor aus Pappe: merkt sich, was ihm gesagt wurde, und dreht nichts.
class RotorAttrappe : public RotorController {
    Q_OBJECT
public:
    using RotorController::RotorController;
    QString description() const override { return QStringLiteral("Attrappe"); }
    State state() const override { return m_state; }
    double azimuth() const override { return m_az; }
    bool hasFreshPosition() const override { return m_frisch; }
    void connectToRotor() override {}
    void disconnectFromRotor() override {}
    void moveTo(double az) override { m_ziele.append(az); }
    void stop() override { m_haltRufe++; }

    State  m_state{State::Idle};
    double m_az{143.0};
    bool   m_frisch{true};
    QVector<double> m_ziele;
    int    m_haltRufe{0};
};
}  // namespace

class TestTciRotor : public QObject {
    Q_OBJECT

private:
    QStringList m_antworten;

    bool aufbauen(TciServer& server, QWebSocket& client, bool drehenFrei,
                  bool anmelden = true)
    {
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteRotor"),
            drehenFrei ? QStringLiteral("True") : QStringLiteral("False"));
        if (!TciServer::setRemoteToken(QString::fromLatin1(kToken))) { return false; }
        if (!server.start(0)) { return false; }
        server.setTreatAllClientsAsRemoteForTest(true);

        connect(&client, &QWebSocket::textMessageReceived, this,
                [this](const QString& m) {
                    if (m.startsWith(QStringLiteral("rotor"))) { m_antworten << m; }
                });

        QSignalSpy verbunden(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        if (!verbunden.wait(2000)) { return false; }
        if (!anmelden) { return true; }
        client.sendTextMessage(QStringLiteral("auth:%1;").arg(QLatin1String(kToken)));
        return TciTest::warteAufReady(client);
    }

    bool warteAuf(const QString& anfang, int grenzeMs = 4000)
    {
        QElapsedTimer uhr; uhr.start();
        while (uhr.elapsed() < grenzeMs) {
            for (const QString& a : m_antworten) {
                if (a.startsWith(anfang)) { return true; }
            }
            QTest::qWait(25);
        }
        return false;
    }

    QString ersteMit(const QString& anfang) const
    {
        for (const QString& a : m_antworten) {
            if (a.startsWith(anfang)) { return a; }
        }
        return {};
    }

private slots:
    void init() { m_antworten.clear(); }
    void cleanup() { TciServer::setRemoteToken(QString()); }

    /// Hinsehen bewegt nichts — und darf darum immer.
    void abfragenGehtAuchOhneFreigabe()
    {
        RadioModel model;
        RotorAttrappe rot;
        rot.m_az = 143.5; rot.m_frisch = true;
        model.setRotor(&rot);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY2(aufbauen(server, client, /*drehenFrei=*/false), "Aufbau gescheitert");

        client.sendTextMessage(QStringLiteral("rotor:;"));
        QVERIFY2(warteAuf(QStringLiteral("rotor_ist:")), "Keine Antwort auf rotor:");
        const QStringList f = ersteMit(QStringLiteral("rotor_ist:"))
                                  .mid(QStringLiteral("rotor_ist:").size())
                                  .chopped(1).split(QLatin1Char(','));
        QCOMPARE(f.size(), 3);
        QCOMPARE(f.at(0), QStringLiteral("143.5"));
        QCOMPARE(f.at(1), QStringLiteral("bereit"));     // ein NAME, keine Zahl
        QCOMPARE(f.at(2), QStringLiteral("1"));          // frisch
        QVERIFY(rot.m_ziele.isEmpty());                  // nichts gedreht
    }

    /// Eine alte Stellung wird als alt gemeldet. Eine Nadel, die eine
    /// veraltete Richtung zeigt, ohne das zu sagen, ist schlimmer als eine,
    /// die nichts zeigt (so steht es im Kopf von RotorController.h).
    void alteStellungWirdAlsAltGemeldet()
    {
        RadioModel model;
        RotorAttrappe rot;
        rot.m_frisch = false;
        model.setRotor(&rot);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, false));
        client.sendTextMessage(QStringLiteral("rotor:;"));
        QVERIFY(warteAuf(QStringLiteral("rotor_ist:")));
        QVERIFY2(ersteMit(QStringLiteral("rotor_ist:")).endsWith(QStringLiteral(",0;")),
                 qPrintable(ersteMit(QStringLiteral("rotor_ist:"))));
    }


    /// Jeder Zustand hat seinen eigenen Namen -- und zwar den, den die
    /// Gegenseite erwartet.
    ///
    /// Vorher stand hier eine Nummer, dieses Feld wurde von KEINEM
    /// Pruefpunkt angesehen, und `handfunke/rotor.js` fuehrte dieselbe
    /// Zaehlung ein zweites Mal. Wer einen Zustand in die Mitte der
    /// Aufzaehlung einfuegt, verschiebt alles darueber: unter der Scheibe
    /// am Telefon stand dann "DREHT", wenn der Rotor einen Fehler meldet.
    /// Am anderen Ende haengt ein Mast.
    void jederZustandHatSeinenNamen()
    {
        const QVector<QPair<RotorController::State, QString>> paare = {
            { RotorController::State::Disconnected, QStringLiteral("getrennt") },
            { RotorController::State::Connecting,   QStringLiteral("verbindet") },
            { RotorController::State::Idle,         QStringLiteral("bereit") },
            { RotorController::State::Moving,       QStringLiteral("dreht") },
            { RotorController::State::Error,        QStringLiteral("fehler") },
        };
        for (const auto& [zustand, name] : paare) {
            m_antworten.clear();
            RadioModel model;
            RotorAttrappe rot;
            rot.m_state = zustand;
            model.setRotor(&rot);

            TciServer server(&model);
            QWebSocket client;
            QVERIFY2(aufbauen(server, client, false), qPrintable(name));
            client.sendTextMessage(QStringLiteral("rotor:;"));
            QVERIFY2(warteAuf(QStringLiteral("rotor_ist:")), qPrintable(name));
            const QStringList f = ersteMit(QStringLiteral("rotor_ist:"))
                                      .mid(QStringLiteral("rotor_ist:").size())
                                      .chopped(1).split(QLatin1Char(','));
            QCOMPARE(f.size(), 3);
            QCOMPARE(f.at(1), name);
        }
    }

    /// DER Punkt: ab Werk wird aus dem Netz NICHT gedreht.
    void drehenIstAbWerkGesperrtUndSagtEs()
    {
        RadioModel model;
        RotorAttrappe rot;
        model.setRotor(&rot);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*drehenFrei=*/false));

        client.sendTextMessage(QStringLiteral("rotor_to:270;"));
        QVERIFY2(warteAuf(QStringLiteral("rotor_err:")),
                 "Ein abgelehnter Rotorbefehl blieb stumm");
        QVERIFY2(rot.m_ziele.isEmpty(), "Trotz Ablehnung wurde gedreht");

        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("rotor_stop:;"));
        QVERIFY(warteAuf(QStringLiteral("rotor_err:")));
        QCOMPARE(rot.m_haltRufe, 0);
    }

    /// Freigegeben dreht er — und zwar dorthin, wo er hinsoll.
    void freigegebenDrehtErWirklich()
    {
        RadioModel model;
        RotorAttrappe rot;
        model.setRotor(&rot);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*drehenFrei=*/true));

        client.sendTextMessage(QStringLiteral("rotor_to:270;"));
        QVERIFY(warteAuf(QStringLiteral("rotor_ok:")));
        QCOMPARE(rot.m_ziele.size(), 1);
        QCOMPARE(rot.m_ziele.first(), 270.0);

        // 360 ist Nord, nicht ein Fehler.
        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("rotor_to:360;"));
        QVERIFY(warteAuf(QStringLiteral("rotor_ok:")));
        QCOMPARE(rot.m_ziele.size(), 2);
        QCOMPARE(rot.m_ziele.last(), 0.0);

        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("rotor_stop:;"));
        QVERIFY(warteAuf(QStringLiteral("rotor_ok:")));
        QCOMPARE(rot.m_haltRufe, 1);
    }

    /// Eine unbrauchbare Peilung dreht NICHT nach Norden.
    void unbrauchbarePeilungDrehtNichtNachNorden()
    {
        RadioModel model;
        RotorAttrappe rot;
        model.setRotor(&rot);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*drehenFrei=*/true));

        for (const QString& unfug : {QStringLiteral(""), QStringLiteral("abc"),
                                     QStringLiteral("nan"), QStringLiteral("-5"),
                                     QStringLiteral("361")}) {
            m_antworten.clear();
            client.sendTextMessage(QStringLiteral("rotor_to:%1;").arg(unfug));
            QVERIFY2(warteAuf(QStringLiteral("rotor_err:")), qPrintable(unfug));
        }
        QVERIFY2(rot.m_ziele.isEmpty(),
                 "Eine unbrauchbare Peilung hat den Rotor bewegt");
    }

    /// Ohne Anmeldung passiert NICHTS — auch keine Antwort.
    void ohneAnmeldungKeinRotor()
    {
        RadioModel model;
        RotorAttrappe rot;
        model.setRotor(&rot);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, true, /*anmelden=*/false));

        client.sendTextMessage(QStringLiteral("rotor:;"));
        client.sendTextMessage(QStringLiteral("rotor_to:90;"));
        QTest::qWait(600);
        QVERIFY2(m_antworten.isEmpty(), qPrintable(m_antworten.join(QLatin1Char(' '))));
        QVERIFY(rot.m_ziele.isEmpty());
    }

    /// "Kein Rotor da" ist eine andere Auskunft als "er steht auf 0 Grad".
    void ohneRotorKommtEinFehlerKeineNull()
    {
        RadioModel model;                 // kein Rotor angemeldet
        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, true));

        client.sendTextMessage(QStringLiteral("rotor:;"));
        QVERIFY(warteAuf(QStringLiteral("rotor_err:")));
        QVERIFY(ersteMit(QStringLiteral("rotor_ist:")).isEmpty());
    }
};

QTEST_MAIN(TestTciRotor)
#include "tst_tci_rotor.moc"

#else   // HAVE_WEBSOCKETS

#include <QtTest>
class TestTciRotor : public QObject { Q_OBJECT };
QTEST_MAIN(TestTciRotor)
#include "tst_tci_rotor.moc"

#endif
