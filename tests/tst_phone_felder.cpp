// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die AM-Traeger-Kennlinie
// stammt aus Thetis und ist in TransmitModel.h zitiert.
//
// Vier Attrappen im Phone/CW-Feld, verdrahtet (2026-10-12).
//
// Mithoeren (Knopf + Regler), Mikrofonquelle, Mikrofonprofil und
// AM-Traegerpegel trugen NyiOverlay: man druckte, und nichts geschah.
// Fuer drei davon war der Unterbau schon da, der AM-Traeger ist neu.
//
// NUR EIN MainWindow je Prozess -- siehe den Kopf von
// tests/tst_mehrere_fenster.cpp.

#include <QtTest/QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "core/TxChannel.h"
#include "gui/MainWindow.h"
#include "gui/applets/PhoneCwApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TstPhoneFelder : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Pflicht: ohne Testmodus schreibt AppSettings in die echte
        // Einstellungsdatei des Betreibers.
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── Die AM-Traeger-Kennlinie ────────────────────────────────────────────

    void dieAmTraegerKennlinieIstEineWurzel()
    {
        // From Thetis Console/setup.cs:9709 [v2.10.3.15]:
        //   TXAMCarrierLevel = Math.Sqrt(0.01 * udTXAMCarrierLevel.Value) * 0.5
        //
        // Festgenagelt, weil der naheliegende Fehler hier teuer ist: wer
        // das fuer linear haelt (Wert/200, oder 0.01*Wert), liegt bei
        // Regler 25 um den Faktor ZWEI daneben -- und das hoert man am
        // Traeger, waehrend die Anzeige richtig aussieht.
        QCOMPARE(TransmitModel::amCarrierDspFromPercent(0),   0.0);
        QVERIFY(qFuzzyCompare(TransmitModel::amCarrierDspFromPercent(100), 0.5));
        // 25 % -> sqrt(0.25)*0.5 = 0.25 ; linear waere 0.125 gewesen.
        QVERIFY(qFuzzyCompare(TransmitModel::amCarrierDspFromPercent(25), 0.25));
        // 125 % -> sqrt(1.25)*0.5 = 0.5590...
        QVERIFY(qAbs(TransmitModel::amCarrierDspFromPercent(125) - 0.559017) < 1e-5);
        // Die Kennlinie steigt streng, aber eben NICHT gleichmaessig:
        // die ersten 25 % bringen die Haelfte des Wegs bis 100 %.
        QVERIFY(TransmitModel::amCarrierDspFromPercent(25)
                > TransmitModel::amCarrierDspFromPercent(100) * 0.4);
    }

    void einNegativerReglerwertGibtKeinNaN()
    {
        // Geklammert wird VOR dem Wurzelziehen. Ohne das gaebe ein
        // negativer Wert NaN, und das ginge ungebremst in WDSP -- ein
        // NaN im Sendeweg ist kein stiller Fehler, sondern Rauschen auf
        // dem Band.
        const double d = TransmitModel::amCarrierDspFromPercent(-50);
        QVERIFY(!std::isnan(d));
        QCOMPARE(d, 0.0);
        QVERIFY(!std::isnan(TransmitModel::amCarrierDspFromPercent(99999)));
    }

    void derAmTraegerKlammertImModell()
    {
        TransmitModel tx;
        QCOMPARE(tx.amCarrierPercent(), TransmitModel::kAmCarrierPercentDefault);
        tx.setAmCarrierPercent(999);
        QCOMPARE(tx.amCarrierPercent(), TransmitModel::kAmCarrierPercentMax);
        tx.setAmCarrierPercent(-5);
        QCOMPARE(tx.amCarrierPercent(), TransmitModel::kAmCarrierPercentMin);
        QSignalSpy spy(&tx, &TransmitModel::amCarrierPercentChanged);
        tx.setAmCarrierPercent(0);          // derselbe Wert
        QCOMPARE(spy.count(), 0);
        tx.setAmCarrierPercent(60);
        QCOMPARE(spy.count(), 1);
    }

    void derKanalMerktSichDenAmTraeger()
    {
        TxChannel ch(1, 64, 64);
        ch.setAmCarrierLevel(0.25);
        QCOMPARE(ch.amCarrierLevelForTest(), 0.25);
        // Ohne offenen WDSP-Kanal darf nichts hinausgehen und nichts
        // abstuerzen.
        QCOMPARE(ch.amCarrierLevelInWdsp(), -1.0);
    }

    // ── Die Sperre der Mikrofonbuchse ───────────────────────────────────────

    void dieSperreMeldetSichJetzt()
    {
        // Ohne dieses Signal bleibt jede Oberflaeche, die "Radio"
        // ausgraut, nach dem Verbinden auf dem alten Stand -- und laesst
        // eine Buchse waehlen, die es an diesem Geraet nicht gibt.
        // Das Signal gab es bis 2026-10-12 nicht.
        TransmitModel tx;
        QSignalSpy spy(&tx, &TransmitModel::micSourceLockedChanged);
        QVERIFY(!tx.isMicSourceLocked());
        tx.setMicSourceLocked(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool());
        QVERIFY(tx.isMicSourceLocked());
        tx.setMicSourceLocked(true);        // kein Wechsel
        QCOMPARE(spy.count(), 1);
        tx.setMicSourceLocked(false);
        QCOMPARE(spy.count(), 2);
        QVERIFY(!spy.at(1).at(0).toBool());
    }

    void dieSperreZwingtRadioAufPc()
    {
        // Vorbestehendes Verhalten, hier festgenagelt, weil das neue
        // Signal davor gemeldet wird und die Reihenfolge zaehlt.
        TransmitModel tx;
        tx.setMicSource(MicSource::Radio);
        QCOMPARE(tx.micSource(), MicSource::Radio);
        tx.setMicSourceLocked(true);
        QCOMPARE(tx.micSource(), MicSource::Pc);
    }

    // ── Der zweite Start ───────────────────────────────────────────────────

    void derZweiteStartMeldetDieProfileAuch()
    {
        // Gefunden am 2026-10-12. MicProfileManager::load() meldete
        // profileListChanged NUR beim ersten Saeen ("No emit on subsequent
        // loads — the manifest already exists"). Das stimmt fuer das
        // Manifest, aber nicht fuer die Empfaenger:
        //
        //   1. Fensteraufbau: setMicProfileManager() fuellt die Liste --
        //      noch ohne MAC, also leer
        //   2. Verbinden: setMacAddress() + load()
        //
        // Beim ersten Mal je Geraet kam das Signal und damit die Liste.
        // Danach nie wieder: der Betreiber sah eine LEERE Profilauswahl,
        // obwohl 22 Werksprofile dalagen. Betraf TxApplet (haengt allein
        // an diesem Signal), das Phone/CW-Feld und RadeApplet.
        MicProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("aa:bb:cc:dd:ee:ff"));

        QSignalSpy ersterLauf(&mgr, &MicProfileManager::profileListChanged);
        mgr.load();                       // saet
        QVERIFY(ersterLauf.count() >= 1);
        const QStringList nachDemSaeen = mgr.profileNames();
        QVERIFY(!nachDemSaeen.isEmpty());

        // Der zweite Start: dieselbe MAC, Manifest liegt schon vor.
        MicProfileManager zweiter;
        zweiter.setMacAddress(QStringLiteral("aa:bb:cc:dd:ee:ff"));
        QSignalSpy zweiterLauf(&zweiter, &MicProfileManager::profileListChanged);
        zweiter.load();
        QCOMPARE(zweiter.profileNames(), nachDemSaeen);
        QVERIFY2(zweiterLauf.count() >= 1,
                 "load() meldet beim zweiten Start nicht -- jede Oberflaeche, "
                 "die ihre Liste auf profileListChanged aufbaut, bleibt leer");
    }

    // ── Am echten Fenster ───────────────────────────────────────────────────

    void dieVierFelderAmEchtenFenster()
    {
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        auto* feld = mw->findChild<PhoneCwApplet*>();
        QVERIFY(feld);
        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm);
        TransmitModel& tx = rm->transmitModel();

        // ── 1. Mithoeren ─────────────────────────────────────────────────
        QPushButton* mon = nullptr;
        for (QPushButton* b : feld->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("MON")) { mon = b; break; }
        }
        QVERIFY(mon);
        const bool vorher = tx.monEnabled();
        mon->click();
        QCOMPARE(tx.monEnabled(), !vorher);
        // Und von aussen gestellt (TX-Feld, Profil, TCI) kommt der Knopf
        // mit -- beide Felder bedienen dieselben zwei Eigenschaften.
        tx.setMonEnabled(vorher);
        QCOMPARE(mon->isChecked(), vorher);

        // ── 2. Mikrofonquelle ────────────────────────────────────────────
        QComboBox* quelle = nullptr;
        for (QComboBox* c : feld->findChildren<QComboBox*>()) {
            if (c->findData(static_cast<int>(MicSource::Vax)) >= 0) {
                quelle = c; break;
            }
        }
        QVERIFY2(quelle, "Keine Combo mit Longpaths MicSource-Werten gefunden "
                         "-- stehen dort noch MIC/BAL/LINE/ACC/PC?");
        QCOMPARE(quelle->count(), 3);     // PC, Radio, VAX -- nicht fuenf
        const int iVax = quelle->findData(static_cast<int>(MicSource::Vax));
        QVERIFY(iVax >= 0);
        quelle->setCurrentIndex(iVax);
        QCOMPARE(tx.micSource(), MicSource::Vax);
        // Modell -> Oberflaeche
        tx.setMicSource(MicSource::Pc);
        QCOMPARE(quelle->currentData().toInt(), static_cast<int>(MicSource::Pc));

        // Gesperrte Buchse: der Eintrag wird AUSGEGRAUT, nicht
        // stillschweigend ignoriert.
        const int iRadio = quelle->findData(static_cast<int>(MicSource::Radio));
        QVERIFY(iRadio >= 0);
        tx.setMicSourceLocked(true);
        QVERIFY2(!(quelle->model()->flags(quelle->model()->index(iRadio, 0))
                   & Qt::ItemIsEnabled),
                 "\"Radio\" ist waehlbar, obwohl das Geraet keine "
                 "Mikrofonbuchse hat");
        tx.setMicSourceLocked(false);
        QVERIFY(quelle->model()->flags(quelle->model()->index(iRadio, 0))
                & Qt::ItemIsEnabled);

        // ── 3. Mikrofonprofil ────────────────────────────────────────────
        QComboBox* profil = nullptr;
        MicProfileManager* mgr = rm->micProfileManager();
        QVERIFY(mgr);

        // Die Profile haengen an der MAC des verbundenen Geraets: ohne
        // Verbindung ist die Liste LEER, und zwar absichtlich
        // (MicProfileManager.h: "All ops require a MAC", RadioModel ruft
        // setMacAddress erst beim Verbinden, RadioModel.cpp:6452).
        //
        // Darum erst der Fall OHNE Profile -- und der ist der wichtigere:
        // die Combo darf die erfundenen Namen ("Default"/"DX"/...) NICHT
        // mehr zeigen. Vor dem 2026-10-12 standen genau die darin, und wer
        // eines waehlte, waehlte nichts.
        QVERIFY(mgr->profileNames().isEmpty());
        for (QComboBox* c : feld->findChildren<QComboBox*>()) {
            if (c == quelle) { continue; }
            for (int i = 0; i < c->count(); ++i) {
                const QString t = c->itemText(i);
                QVERIFY2(t != QStringLiteral("DX") && t != QStringLiteral("ESSB"),
                         qPrintable(QStringLiteral("Erfundener Profilname \"%1\" "
                                                   "steht noch in einer Combo")
                                        .arg(t)));
            }
        }

        // Jetzt mit Profilen: die MAC selbst setzen und laden -- das geht
        // ohne Geraet und saet die Werksprofile.
        mgr->setMacAddress(QStringLiteral("00:11:22:33:44:55"));
        mgr->load();
        const QStringList namen = mgr->profileNames();
        QVERIFY2(!namen.isEmpty(),
                 "Der Verwalter saet auch mit MAC keine Profile");
        for (QComboBox* c : feld->findChildren<QComboBox*>()) {
            if (c == quelle) { continue; }
            if (c->count() == namen.size() && c->count() > 0
                && namen.contains(c->itemText(0))) {
                profil = c; break;
            }
        }
        if (!profil) {
            // Wenn das je faellt: zeigen, was wirklich in den Combos
            // steht. Beim ersten Mal war "Microphone profile" count=0,
            // waehrend der Verwalter 22 Profile kannte -- das war die
            // Spur zur Reihenfolgefalle in MainWindow.
            for (QComboBox* c : feld->findChildren<QComboBox*>()) {
                QStringList inhalt;
                for (int i = 0; i < c->count(); ++i) { inhalt << c->itemText(i); }
                qWarning() << "Combo:" << c->accessibleName()
                           << "count=" << c->count() << inhalt;
            }
            qWarning() << "Verwalter kennt:" << namen.size() << namen.mid(0, 5);
        }
        QVERIFY2(profil, "Keine Combo mit den ECHTEN Profilnamen gefunden "
                         "-- stehen dort noch Default/DX/...?");
        // Die erfundenen Namen sind weg.
        for (int i = 0; i < profil->count(); ++i) {
            QVERIFY2(namen.contains(profil->itemText(i)),
                     qPrintable(QStringLiteral("Profil \"%1\" kennt der "
                                               "Verwalter nicht")
                                    .arg(profil->itemText(i))));
        }

        // ── 4. AM-Traegerpegel ───────────────────────────────────────────
        QSlider* amcar = nullptr;
        for (QSlider* sl : feld->findChildren<QSlider*>()) {
            if (sl->maximum() == TransmitModel::kAmCarrierPercentMax
                && sl->minimum() == TransmitModel::kAmCarrierPercentMin) {
                amcar = sl; break;
            }
        }
        QVERIFY2(amcar, "Kein Regler mit dem Bereich 0..125 -- steht er "
                        "noch auf 0..100?");
        amcar->setValue(60);
        QCOMPARE(tx.amCarrierPercent(), 60);
        tx.setAmCarrierPercent(110);
        QCOMPARE(amcar->value(), 110);

        delete mw;
    }
};

QTEST_MAIN(TstPhoneFelder)
#include "tst_phone_felder.moc"
