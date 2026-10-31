// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_swr_riegel.cpp  (Longpath)
// =================================================================
//
// „Sollte SWR ueber 2,5 sein, bitte nicht senden!" — Betreiber,
// 2026-10-30. Hier steht, was das genau heiszt, einschliesslich der
// drei Faelle, in denen eine Messung NICHTS heiszt.
//
// Die Zeit kommt von Hand herein; eine Haltbarkeit von zwei Minuten
// laeuft darum in einer Millisekunde durch.
// =================================================================

#include <QtTest/QtTest>

#include "core/safety/SwrRiegel.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"

using namespace Longpath::safety;

class TstSwrRiegel : public QObject
{
    Q_OBJECT

private slots:
    void dieVorgabeIstDieZahlDesBetreibers()
    {
        QCOMPARE(SwrRiegel::kVorgabeGrenze, 2.5f);
        SwrRiegel r;
        QCOMPARE(r.grenze(), 2.5f);
    }

    // ── Der Fall, um den gebeten wurde ────────────────────────────
    void ueberDerGrenzeWirdGebrochenUndDannNichtMehrGetastet()
    {
        SwrRiegel r;
        const qint64 t = 1000000;

        // Eine saubere Sendung: nichts passiert.
        QVERIFY(!r.messung(1.6f, 32.0f, t));
        QVERIFY(r.darfTasten(t));

        // Jetzt reiszt die Antenne ab.
        QVERIFY2(r.messung(3.1f, 30.0f, t + 1000),
                 "die Sendung haette gebrochen werden muessen");
        // Und der naechste Versuch geht gar nicht erst los.
        QVERIFY(!r.darfTasten(t + 2000));
        QVERIFY(r.grund(t + 2000).contains(QStringLiteral("3.10")));

        // Dieselbe Ueberschreitung meldet NICHT noch einmal: der
        // Sender klingt aus, es kommen weitere Messungen herein.
        QVERIFY(!r.messung(3.4f, 20.0f, t + 1100));
        QVERIFY(!r.messung(3.2f, 10.0f, t + 1200));
    }

    // Genau auf der Grenze wird gesendet. 2,5 ist erlaubt, „ueber 2,5"
    // ist es nicht — so stand es in der Bitte.
    void genauAufDerGrenzeWirdNochGesendet()
    {
        SwrRiegel r;
        QVERIFY(!r.messung(2.5f, 20.0f, 0));
        QVERIFY(r.darfTasten(0));
        QVERIFY(r.messung(2.51f, 20.0f, 100));
        QVERIFY(!r.darfTasten(100));
    }

    // ── Wenn eine Messung nichts heiszt ───────────────────────────
    //
    // 1. Ohne Leistung ist die Stehwellenmessung Rauschen.
    void ohneLeistungZaehltKeineMessung()
    {
        SwrRiegel r;
        QVERIFY(!r.messung(9.9f, 0.2f, 0));
        QVERIFY2(r.darfTasten(0),
                 "eine Rauschmessung beim Hochfahren haette jede "
                 "Sendung gesperrt");
        // Sie wird auch nicht gemerkt.
        QCOMPARE(r.letzteSwr(), 0.0f);
    }

    // 2. Wurde nie gemessen, wird nicht gesperrt.
    void ohneJedeMessungWirdGetastet()
    {
        SwrRiegel r;
        QVERIFY(r.darfTasten(0));
        QVERIFY(r.darfTasten(99999999));
        QVERIFY(r.grund(0).isEmpty());
    }

    // 3. Eine Messung altert. Wer die Antenne wechselt, soll nicht
    //    eine Stunde gesperrt bleiben.
    void eineAlteMessungSperrtNichtMehr()
    {
        SwrRiegel r;
        r.messung(4.0f, 20.0f, 0);
        QVERIFY(!r.darfTasten(SwrRiegel::kHaltbarkeitMs - 1));
        QVERIFY(r.darfTasten(SwrRiegel::kHaltbarkeitMs));
    }

    void ausgeschaltetSperrtNie()
    {
        SwrRiegel r;
        r.setzeAn(false);
        QVERIFY(!r.messung(9.0f, 50.0f, 0));
        QVERIFY(r.darfTasten(0));
    }

    void dieGrenzeLaesstSichSetzenAberNichtInsSinnlose()
    {
        SwrRiegel r;
        r.setzeGrenze(2.0f);
        QCOMPARE(r.grenze(), 2.0f);
        // Unter 1,0 gibt es keine Stehwelle -- das waere eine
        // Dauersperre.
        r.setzeGrenze(0.5f);
        QCOMPARE(r.grenze(), 1.0f);
        r.setzeGrenze(99.0f);
        QCOMPARE(r.grenze(), 10.0f);
    }

    // Nach dem Loesen faengt die naechste Sendung ohne alten Bruch an
    // -- aber die Messung bleibt stehen, sie soll ja gerade das
    // Tasten verhindern.
    void zuruecksetzenLoeschtDenBruchNichtDieMessung()
    {
        SwrRiegel r;
        r.messung(3.0f, 20.0f, 0);
        r.zuruecksetzen();
        QVERIFY2(!r.darfTasten(1000),
                 "die Messung darf das Zuruecksetzen ueberleben");
        // Wird es besser, geht es wieder.
        QVERIFY(!r.messung(1.4f, 20.0f, 2000));
        QVERIFY(r.darfTasten(2000));
    }

    // ── Und er ist ANGESCHLOSSEN ──────────────────────────────────
    //
    // Nicht „gibt es", sondern „die Zahl kommt an". Eine Einheit, die
    // geprueft ist, aber die Einstellung nie sieht, steht auf der
    // Vorgabe und tut stillschweigend etwas anderes als gewuenscht.
    void dieGrenzeAusDenEinstellungenKommtImModellAn()
    {
        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrGrenze"),
                                         QStringLiteral("2.0"));
        Longpath::RadioModel m;
        QCOMPARE(m.flexSwrRiegel().grenze(), 2.0f);
        QVERIFY(m.flexSwrRiegel().an());

        // Und abschalten laesst er sich auch.
        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrRiegelAn"),
                                         QStringLiteral("False"));
        Longpath::RadioModel m2;
        QVERIFY(!m2.flexSwrRiegel().an());

        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrRiegelAn"),
                                         QStringLiteral("True"));
        Longpath::AppSettings::instance().setValue(
            QStringLiteral("FlexSwrGrenze"),
            QString::number(SwrRiegel::kVorgabeGrenze));
    }

    // Unsinn in der Einstellungsdatei faellt auf die Vorgabe zurueck,
    // statt den Riegel auf eine Zufallszahl zu stellen.
    void unsinnInDerDateiGibtDieVorgabe()
    {
        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrGrenze"),
                                         QStringLiteral("spaeter"));
        Longpath::RadioModel m;
        QCOMPARE(m.flexSwrRiegel().grenze(), SwrRiegel::kVorgabeGrenze);
        Longpath::AppSettings::instance().setValue(
            QStringLiteral("FlexSwrGrenze"),
            QString::number(SwrRiegel::kVorgabeGrenze));
    }
    // ── Jeder Sendeweg fragt den Riegel (2026-10-30) ─────────────
    //
    // Gefunden bei der Durchsicht desselben Abends: der Riegel hing
    // nur an `flexTasten()`. `flexAbstimmen()` ist der ZWEITE
    // Sendeweg -- TUNE schickt einen Dauertraeger, und das ist der
    // Fall, in dem die Stehwelle am meisten zaehlt, denn man stimmt
    // ab, WEIL die Anpassung nicht stimmt. Ein FT8-Durchgang endet
    // nach 13,2 s von selbst; ein Abstimmtraeger laeuft, bis ihn
    // jemand loest.
    //
    // Ohne echtes Geraet kommt kein Lauf an das vierte Tor (ohne
    // `m_flexConnection` kehrt beides vorher um). Darum liest dieser
    // Fall die Quelle. Sein Nutzen ist der DRITTE Sendeweg, den
    // morgen jemand baut: dann faellt er.
    void jederSendewegFragtDenRiegel()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        // Es gibt GENAU zwei Stellen, die den Sender tasten. Kommt
        // eine dritte hinzu, faellt dieser Lauf -- und wer ihn liest,
        // weiss warum.
        const int tastend =
            src.count(QStringLiteral("FlexCommandWire::xmitCommand("))
            + src.count(QStringLiteral("FlexCommandWire::tuneCommand("));
        QCOMPARE(tastend, 2);

        // Und beide fragen vorher den Riegel. Je Funktionsrumpf
        // geprueft, nicht datei-weit: eine Abfrage irgendwo in der
        // Datei sagt nichts darueber, ob SIE auf diesem Weg liegt.
        //
        // 2026-10-31: beide fragen weiter, aber nicht dasselbe. Das
        // Tasten fragt die gehaltene Messung (`darfTasten`), der
        // Abstimmtraeger nur noch den Riegel selbst
        // (`darfAbstimmen`) — abstimmen ist die Abhilfe, nicht der
        // Verstosz. Was hier zaehlt und weiter zaehlen soll: KEIN Weg
        // tastet, ohne den Riegel ueberhaupt gefragt zu haben.
        const QMap<QString, QString> wege{
            {QStringLiteral("bool RadioModel::flexTasten("),
             QStringLiteral("m_flexSwrRiegel.darfTasten")},
            {QStringLiteral("bool RadioModel::flexAbstimmen("),
             QStringLiteral("m_flexSwrRiegel.darfAbstimmen")}};
        for (auto it = wege.cbegin(); it != wege.cend(); ++it) {
            const QString& kopf = it.key();
            const int a = src.indexOf(kopf);
            QVERIFY2(a >= 0, qPrintable(kopf));
            // Bis zur naechsten Funktion auf Spaltenanfang.
            int e = src.indexOf(QStringLiteral("\n}\n"), a);
            QVERIFY(e > a);
            const QString rumpf = src.mid(a, e - a);
            QVERIFY2(rumpf.contains(it.value()),
                     qPrintable(kopf + QStringLiteral(
                         " tastet den Sender, ohne die Stehwelle zu fragen")));
        }

        // Der Bruch waehrend der Sendung muss BEIDE Wege loesen:
        // `xmit 0` beendet keinen Abstimmtraeger.
        const int bruch = src.indexOf(QStringLiteral("SENDUNG GEBROCHEN"));
        QVERIFY(bruch > 0);
        const QString danach = src.mid(bruch, 1200);
        QVERIFY2(danach.contains(QStringLiteral("flexTasten(false)")),
                 "der Bruch loest das Tasten nicht");
        QVERIFY2(danach.contains(QStringLiteral("flexAbstimmen(false)")),
                 "der Bruch laesst einen laufenden Abstimmtraeger stehen");
    }


    // ── Eine Messung gehoert zu EINEM Band (2026-10-31) ──────────
    //
    // Der Befund des Betreibers: „tune geht auf 20 und 15 meter
    // nicht, 40 schon." Gemessen worden war auf 20 m (7,08 bei
    // 24,1 W); gesperrt war danach auch 15 m. Dasz 40 m ging, war
    // nicht das Band — es war der Ablauf der zwei Minuten.
    void eineMessungAufEinemBandSperrtNichtDasAndere()
    {
        SwrRiegel r;
        QVERIFY(r.messung(7.08f, 24.1f, 1000, QStringLiteral("20m")));

        QVERIFY2(!r.darfTasten(1100, QStringLiteral("20m")),
                 "auf dem gemessenen Band muss die Sperre greifen");
        QVERIFY2(r.darfTasten(1100, QStringLiteral("15m")),
                 "15m hat keine eigene Messung — Abwesenheit ist keine Aussage");
        QVERIFY2(r.darfTasten(1100, QStringLiteral("40m")),
                 "40m hat keine eigene Messung");
        QCOMPARE(r.letztesBand(), QStringLiteral("20m"));

        // Und eine gute Messung auf 15 m laesst 20 m gesperrt.
        QVERIFY(!r.messung(1.2f, 20.0f, 1200, QStringLiteral("15m")));
        QVERIFY(r.darfTasten(1300, QStringLiteral("15m")));
    }

    // ── Abstimmen ist die Abhilfe, nicht der Verstosz ────────────
    //
    // Man stimmt ab, WEIL die Anpassung nicht stimmt. Der Riegel vor
    // dem Abstimmtraeger verweigerte genau den Handgriff, der die
    // Stehwelle gut gemacht haette. Was schuetzt, ist der Riegel
    // WAEHREND der Sendung — am 2026-10-31 um 13:05:14.735 nach
    // 146 ms bei 24,1 W belegt.
    void derAbstimmtraegerBleibtErlaubt()
    {
        SwrRiegel r;
        QVERIFY(r.messung(7.08f, 24.1f, 1000, QStringLiteral("20m")));
        QVERIFY2(!r.darfTasten(1100, QStringLiteral("20m")),
                 "senden bleibt gesperrt");
        QVERIFY2(r.darfAbstimmen(),
                 "abstimmen muss gehen — sonst kommt man aus einer "
                 "Fehlanpassung nicht heraus");
    }

    // `vergessen` ist NICHT `zuruecksetzen`: das eine loescht die
    // Messung, das andere nur den Bruch.
    void vergessenLoeschtDieMessungZuruecksetzenNicht()
    {
        SwrRiegel r;
        QVERIFY(r.messung(7.0f, 20.0f, 1000, QStringLiteral("20m")));
        r.zuruecksetzen();
        QVERIFY2(!r.darfTasten(1100, QStringLiteral("20m")),
                 "zuruecksetzen darf die Messung nicht loeschen");

        r.vergessen();
        QVERIFY2(r.darfTasten(1100, QStringLiteral("20m")),
                 "nach vergessen() liegt keine Messung mehr vor");
        QCOMPARE(r.letzteSwr(), 0.0f);
        QVERIFY(r.letztesBand().isEmpty());
        QVERIFY(r.grund(1100, QStringLiteral("20m")).isEmpty());
    }

    // ── Die Bruecke der Maschine schlaegt die eigene Rechnung ────
    //
    // Die Flex schickt `SWR` als eigenen Messwert mit. Wir lasen ihn,
    // skalierten ihn, schrieben ihn ins Protokoll — und warfen ihn
    // weg. Gerechnet wurde aus FWDPWR und REFPWR, und die kommen
    // nicht verlaeszlich im selben Paket (2026-10-27 nachgemessen).
    // Ein neuer Vorlauf gegen einen alten Ruecklauf ergibt eine Zahl,
    // die es nie gegeben hat.
    void derRiegelNimmtDieBrueckeDerMaschine()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        const int a = src.indexOf(QStringLiteral("m_flexSwrRiegel.messung("));
        QVERIFY2(a > 0, "niemand speist den Riegel mehr");
        const QString umfeld = src.mid(qMax(0, a - 2500), 2900);

        QVERIFY2(umfeld.contains(QStringLiteral("swrGeraet")),
                 "der Riegel rechnet wieder selbst, statt die Bruecke der "
                 "Maschine zu lesen");
        QVERIFY2(umfeld.contains(QStringLiteral("(fwdW && refW)")),
                 "ohne Paar aus EINEM Paket darf nicht gerechnet werden");
        QVERIFY2(umfeld.contains(QStringLiteral("flexSwrBandKennung()")),
                 "die Messung wird ohne Band eingespeist und sperrt dann "
                 "wieder alle Baender");
    }

    // ── Eine stumme Sperre ist ein Defekt (2026-10-30) ───────────
    //
    // `flexSwrGesperrt` hatte den ganzen Nachmittag KEINEN Empfaenger:
    // dreimal ausgesendet, niemand hoerte zu. Der Riegel hielt den
    // Sender an, und der Betreiber haette „sendet nicht" gemeldet,
    // ohne je zu erfahren, dass es die Sperre war -- genau der Abend,
    // den die vier Tore in `flexTasten` mit ihren einzelnen Meldungen
    // vermeiden sollen.
    //
    // Achter Fall „gebaut und unerreichbar" an diesem Tag.
    void dieSperreMeldetSichAmSchirm()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/MainWindow_Flex.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString gui = QString::fromUtf8(f.readAll());

        const int a = gui.indexOf(QStringLiteral("RadioModel::flexSwrGesperrt"));
        QVERIFY2(a > 0, "das Sperrsignal hat keinen Empfaenger — die Sperre "
                        "greift stumm");
        // Und sie sagt es sichtbar, nicht nur im Protokoll.
        const QString rumpf = gui.mid(a, 900);
        QVERIFY2(rumpf.contains(QStringLiteral("showToast")),
                 "der Empfaenger schreibt nur ins Protokoll");
        QVERIFY2(rumpf.contains(QStringLiteral("ToastSeverity::Error")),
                 "eine Sicherheitssperre ist kein Hinweis am Rand");
    }
};

QTEST_MAIN(TstSwrRiegel)
#include "tst_swr_riegel.moc"
