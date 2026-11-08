// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Rundum-Lauf (2026-11-08) ────────────────────────────────────────
//
//   1. RundumKarte: Sektoren, staerkste Belegung, lautester Boden.
//   2. RundumLauf an einem nachgebildeten Rotor: zwoelf 30-Grad-Schritte,
//      Rueckkehr zur Ausgangsrichtung, Messen nur bei frischer Position.
//   3. Abbruch: ohne Rotor, bei Stillstand, auf Knopfdruck.
//   4. Das Fenster ohne Rotor sagt es, statt still nichts zu tun.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QPushButton>

#include <cmath>

#include "core/AppSettings.h"
#include "core/RotorController.h"
#include "core/RundumLauf.h"
#include "gui/RundumDialog.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {
class NachgebildeterRotor : public RotorController {
public:
    QString description() const override { return QStringLiteral("Pruefstand"); }
    State state() const override { return m_verbunden ? State::Idle : State::Disconnected; }
    double azimuth() const override { return m_az; }
    bool hasFreshPosition() const override { return m_frisch; }
    void connectToRotor() override { m_verbunden = true; }
    void disconnectFromRotor() override { m_verbunden = false; }
    void moveTo(double az) override { ziele.push_back(az); }
    void stop() override { ++stopps; }

    // Faehrt in 5-Grad-Schritten zum letzten Ziel, meldet jede Stellung
    // und laesst danach den Panadapter eine Bandlage liefern (gezaehlt
    // wird bei der zuletzt GEMELDETEN Richtung, siehe RundumLauf.h).
    template <typename F>
    void fahre(F messe)
    {
        const double ziel = ziele.back();
        for (int i = 0; i < 20 && std::abs(std::remainder(ziel - m_az, 360.0)) > 0.1; ++i) {
            const double d = std::remainder(ziel - m_az, 360.0);
            m_az = std::fmod(m_az + std::clamp(d, -5.0, 5.0) + 360.0, 360.0);
            emit positionChanged(m_az);
            messe(m_az);
        }
    }

    bool m_verbunden{true};
    bool m_frisch{true};
    double m_az{100.0};
    QVector<double> ziele;
    int stopps{0};
};

// Belebt um 210 Grad, Stoerer aus 30 Grad.
float belegungBei(double az) { return 10.0f * std::exp(-std::pow(std::remainder(az - 210.0, 360.0) / 25.0, 2)); }
float bodenBei(double az)    { return -120.0f + 8.0f * std::exp(-std::pow(std::remainder(az - 30.0, 360.0) / 20.0, 2)); }
} // namespace

class TstRundumLauf : public QObject {
    Q_OBJECT
private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void dieKarteSammeltJeSektor()
    {
        RundumKarte k;
        QCOMPARE(k.abgedeckt(), 0);
        QCOMPARE(k.meisteBelegung(), -1);
        k.nimm(212.0, -120.0f, 9.0f, 4);
        k.nimm(358.0, -112.0f, 1.0f, 1);      // 358 gehoert zu 0 Grad
        k.nimm(1.0, -114.0f, 0.0f, 0);
        QCOMPARE(k.abgedeckt(), 2);
        QCOMPARE(k.meisteBelegung(), 210);
        float laut = 0.0f;
        QCOMPARE(k.lautesterBoden(&laut), 0);
        QVERIFY(std::abs(laut - 7.0f) < 0.01f);  // (-112-114)/2 = -113 gegen -120
    }

    void einmalHerumUndZurueck()
    {
        NachgebildeterRotor rotor;
        RundumLauf lauf;
        lauf.setRotor(&rotor);
        QSignalSpy fertig(&lauf, &RundumLauf::fertig);
        QVERIFY(lauf.start());
        QCOMPARE(lauf.phase(), RundumLauf::Phase::Lauf);
        // Der Panadapter liefert alle 100 ms, also auch schon, bevor sich
        // der Rotor das erste Mal bewegt: das ist der Startsektor.
        lauf.nimmBandLage(bodenBei(100.0), belegungBei(100.0), 0);
        int sicherung = 0;
        while (fertig.isEmpty() && ++sicherung < 40) {
            rotor.fahre([&](double az) {
                lauf.nimmBandLage(bodenBei(az), belegungBei(az), az > 180 && az < 240 ? 5 : 0);
            });
        }
        QCOMPARE(fertig.size(), 1);
        QCOMPARE(fertig.first().first().toBool(), true);
        QCOMPARE(rotor.ziele.size(), 12);
        for (int i = 0; i < 12; ++i) {
            QCOMPARE(rotor.ziele[i], std::fmod(100.0 + (i + 1) * 30.0, 360.0));
        }
        QCOMPARE(rotor.ziele.last(), 100.0);        // zurueck auf den Anfang
        QCOMPARE(lauf.karte().abgedeckt(), RundumKarte::kSektoren);
        QCOMPARE(lauf.karte().meisteBelegung(), 210);
        float laut = 0.0f;
        QCOMPARE(lauf.karte().lautesterBoden(&laut), 30);
        QVERIFY(laut > 7.0f);
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) {
            RundumBild bild;
            bild.setKarte(&lauf.karte());
            bild.setRichtung(100.0, true);
            bild.resize(380, 380);
            bild.grab().save(dir + QStringLiteral("/rundum-lauf.png"));
        }
    }

    void ohneFrischePositionKeineWerte()
    {
        NachgebildeterRotor rotor;
        RundumLauf lauf;
        lauf.setRotor(&rotor);
        QVERIFY(lauf.start());
        rotor.m_frisch = false;
        lauf.nimmBandLage(-120.0f, 5.0f, 2);
        QCOMPARE(lauf.karte().abgedeckt(), 0);
        lauf.abbrechen();
    }

    void abbruchFaelleSagenEs()
    {
        // Kein Rotor verbunden.
        NachgebildeterRotor aus;
        aus.m_verbunden = false;
        RundumLauf a;
        a.setRotor(&aus);
        QSignalSpy f(&a, &RundumLauf::fehler);
        QVERIFY(!a.start());
        QCOMPARE(f.size(), 1);

        // Stillstand: der Rotor meldet nichts mehr.
        NachgebildeterRotor rotor;
        RundumLauf lauf;
        lauf.setRotor(&rotor);
        lauf.setStillstandMs(50);
        QSignalSpy fehler(&lauf, &RundumLauf::fehler);
        QSignalSpy fertig(&lauf, &RundumLauf::fertig);
        QVERIFY(lauf.start());
        QTRY_COMPARE_WITH_TIMEOUT(fertig.size(), 1, 2000);
        QCOMPARE(fertig.first().first().toBool(), false);
        QCOMPARE(fehler.size(), 1);
        QCOMPARE(rotor.stopps, 1);
        QCOMPARE(rotor.ziele.last(), 100.0);       // zurueckgeschickt

        // Knopfdruck.
        NachgebildeterRotor r2;
        RundumLauf l2;
        l2.setRotor(&r2);
        QVERIFY(l2.start());
        l2.abbrechen();
        QCOMPARE(l2.phase(), RundumLauf::Phase::Ruhe);
        QCOMPARE(r2.ziele.last(), 100.0);
    }

    void dasFensterOhneRotor()
    {
        RadioModel radio;
        SpectrumWidget sw;
        RundumDialog d(&radio, &sw);
        // Start ueber die Knoepfe: den Startknopf suchen und druecken.
        QPushButton* start = nullptr;
        for (QPushButton* b : d.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("Start")) { start = b; }
        }
        QVERIFY(start);
        start->click();
        QVERIFY2(d.statusTextForTest().contains(QStringLiteral("No rotator")),
                 qPrintable(d.statusTextForTest()));
    }
};

QTEST_MAIN(TstRundumLauf)
#include "tst_rundum_lauf.moc"
