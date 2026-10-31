// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_ft8_plaetze_panel.cpp  (Longpath)
// =================================================================
//
// Das Plaetze-Panel: was es zeigt, und DASS es etwas zu zeigen
// bekommt.
//
// Der zweite Teil ist der wichtigere. `Ft8Mehrfach::belegung()`
// rechnete die Belegung seit dem 2026-10-24 aus, und niemand las sie
// -- mit `slots 3` gingen drei Signale hinaus und der Betreiber sah
// keines davon. Eine Pruefung, die nur `baueStand()` aufruft, haette
// das nie gefunden; sie muss durch das SIGNAL gehen.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/applets/Ft8Applet.h"
#include "gui/applets/Ft8PlaetzeApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFt8PlaetzePanel : public QObject
{
    Q_OBJECT

private:
    static QVector<Ft8Decode> einFund(const QString& text, float snr,
                                      float hz)
    {
        Ft8Decode d;
        d.snrDb = snr; d.dtSeconds = 0.2f; d.freqHz = hz;
        d.score = 40;  d.text = text;
        return {d};
    }

private slots:
    void initTestCase()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));
    }

    // ── Die Zuege heiszen, wie sie im Fenster heiszen ─────────────
    //
    // NICHT `zustandName()` -- das sind die technischen Namen
    // („SendeRReport"), gut fuer Prueflaeufe und zu lang fuer eine
    // Pille von 48 Punkten.
    void dieZuegeTragenDieNamenAusDemBlatt()
    {
        using Z = Ft8Ablauf::Zustand;
        QCOMPARE(Ft8Applet::zugKurz(Z::RufeCq),       QStringLiteral("CQ"));
        QCOMPARE(Ft8Applet::zugKurz(Z::RufeAn),       QStringLiteral("Tx1"));
        QCOMPARE(Ft8Applet::zugKurz(Z::SendeReport),  QStringLiteral("Tx2"));
        QCOMPARE(Ft8Applet::zugKurz(Z::SendeRReport), QStringLiteral("Tx3"));
        QCOMPARE(Ft8Applet::zugKurz(Z::SendeRR73),    QStringLiteral("RR73"));
        QCOMPARE(Ft8Applet::zugKurz(Z::Sende73),      QStringLiteral("73"));
        QVERIFY(Ft8Applet::zugKurz(Z::Ruht).isEmpty());
    }

    // Die fuenf Striche: ein QSO hat Stufen, keinen Verlauf.
    void dieStufeWaechstMitDemQso()
    {
        using Z = Ft8Ablauf::Zustand;
        QCOMPARE(Ft8Applet::stufeFuer(Z::RufeCq),       0);
        QCOMPARE(Ft8Applet::stufeFuer(Z::RufeAn),       1);
        QCOMPARE(Ft8Applet::stufeFuer(Z::SendeReport),  2);
        QCOMPARE(Ft8Applet::stufeFuer(Z::SendeRReport), 3);
        QCOMPARE(Ft8Applet::stufeFuer(Z::SendeRR73),    4);
        QCOMPARE(Ft8Applet::stufeFuer(Z::Fertig),       5);
    }

    // ── Der ANSCHLUSS: vom Fund bis ins Panel ─────────────────────
    //
    // Die gemessene Szene vom 2026-10-30: HB9ZHK im QSO, PD4RJ und
    // 9A6T rufen dazwischen.
    void einLaufendesQsoUndZweiWartendeKommenImPanelAn()
    {
        RadioModel radio;
        Ft8Applet quelle(&radio);
        Ft8PlaetzeApplet panel(&radio);
        QObject::connect(&quelle, &Ft8Applet::plaetzeGeaendert,
                         &panel,  &Ft8PlaetzeApplet::zeigeStand);
        QSignalSpy spion(&quelle, &Ft8Applet::plaetzeGeaendert);

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(7074000.0);
        quelle.setSlice(slice);

        quelle.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        for (const QString& t : {QStringLiteral("OE5SOS PD4RJ JO33"),
                                 QStringLiteral("OE5SOS 9A6T")}) {
            QMetaObject::invokeMethod(
                &quelle, "onDecoded", Qt::DirectConnection,
                Q_ARG(QVector<Longpath::Ft8Decode>, einFund(t, 12.0f, 1338.0f)),
                Q_ARG(QDateTime, fenster));
        }
        quelle.zeigeAblaufForTest();

        QVERIFY2(spion.count() > 0,
                 "das Panel bekommt nichts -- genau so lag "
                 "Ft8Mehrfach::belegung() monatelang unerreichbar da");

        const Ft8PlaetzeStand& st = panel.flaecheForTest()->stand();
        QCOMPARE(st.plaetze.size(), 1);
        QCOMPARE(st.plaetze.first().partner, QStringLiteral("HB9ZHK"));
        QCOMPARE(st.plaetze.first().zug,     QStringLiteral("Tx2"));
        QCOMPARE(st.plaetze.first().stufe,   2);
        QCOMPARE(st.plaetze.first().gesendet, 18);

        QCOMPARE(st.wartende.size(), 2);
        QCOMPARE(st.wartende.at(0).partner, QStringLiteral("PD4RJ"));
        QCOMPARE(st.wartende.at(1).partner, QStringLiteral("9A6T"));

        // ── Ein Blatt davon, wenn eines verlangt wird ────────────
        //
        // Der Betreiber hat dieses Panel GEFUELLT noch nie gesehen
        // (sieben Gruende, alle am 2026-10-30). Hier steht der
        // Zustand, den er sehen will, also kann hier auch das Bild
        // entstehen.
        //
        // NUR bei gesetztem `LONGPATH_GRAB_OUT`. Ohne die Variable
        // passiert nichts -- am 2026-10-15 ist ein Abgriff-Lauf in
        // der CI rot geworden, weil er ohne sie trotzdem malen
        // wollte. Der Fall oben bleibt davon unberuehrt.
        const QByteArray ziel = qgetenv("LONGPATH_GRAB_OUT");
        if (!ziel.isEmpty()) {
            panel.resize(360, panel.sizeHint().height());
            panel.show();
            QVERIFY(QTest::qWaitForWindowExposed(&panel, 20000));
            const QPixmap bild = panel.grab();
            const QString weg = QString::fromLocal8Bit(ziel);
            QVERIFY2(bild.save(weg), qPrintable(weg));
            qInfo().noquote() << "Blatt abgelegt:" << weg
                              << bild.width() << "x" << bild.height();
        }
    }

    // Leer heisst leer: ohne Plaetze und ohne Wartende steht der
    // Hinweis da, nicht eine leere Flaeche.
    void ohneAllesStehtDerHinweisStattEinerLeerenFlaeche()
    {
        RadioModel radio;
        Ft8PlaetzeApplet panel(&radio);
        panel.zeigeStand(Ft8PlaetzeStand{});
        QVERIFY(!panel.flaecheForTest()->isVisible());

        Ft8PlaetzeStand st;
        Ft8PlatzStand z;
        z.partner = QStringLiteral("DL1ABC");
        st.plaetze.append(z);
        panel.zeigeStand(st);
        QVERIFY(panel.flaecheForTest()->stand().plaetze.size() == 1);
    }

    // ── Wann es sich von selbst zeigen soll ───────────────────────
    //
    // Betreiber am 2026-10-30: „waere schoen, wenn man das immer
    // siehst, sobald ...". `hatInhalt()` ist die eine Regel dafuer --
    // das Panel fragt sie (Hinweis oder Kacheln) und MainWindow
    // fragt sie (aufmachen oder nicht). Zwei Stellen mit derselben
    // Regel laufen auseinander.
    void hatInhaltIstDieEineRegel()
    {
        Ft8PlaetzeStand leer;
        QVERIFY2(!hatInhalt(leer), "ohne alles hat es nichts zu zeigen");

        // Ein laufendes QSO reicht -- auch mit einem einzigen Platz.
        Ft8PlaetzeStand mitQso;
        mitQso.plaetzeGesamt = 1;
        Ft8PlatzStand z;
        z.partner = QStringLiteral("HB9ZHK");
        mitQso.plaetze.append(z);
        QVERIFY(hatInhalt(mitQso));

        // Und ein Wartender allein reicht AUCH -- genau der Fall vom
        // 2026-10-30, als F5PZT und OE3TVJ im Protokoll standen und
        // nirgends sonst.
        Ft8PlaetzeStand nurWartend;
        nurWartend.wartende.append(
            Ft8WartenderStand{QStringLiteral("F5PZT"), QString(), 9});
        QVERIFY2(hatInhalt(nurWartend),
                 "ein Wartender ohne laufendes QSO ist auch Inhalt");
    }

    // ── Das Fenster darf nicht als Streifen aufgehen ──────────────
    //
    // `AppletFloatingWindow::applyDefaultSize()` fragt `sizeHint()`,
    // NICHT `minimumHeight()`. Und der Vorgabe-sizeHint eines
    // Applets ist die Summe seiner Anordnung, in der ein VERSTECKTES
    // Widget gar nicht mitzaehlt -- im leeren Zustand ist die
    // Flaeche versteckt.
    //
    // Der Betreiber bekam darum am 2026-10-30 eine Titelleiste von
    // zwanzig Punkten ohne Inhalt: „ein eigenes fenster erscheint
    // hier aber nicht", und spaeter „die einzelnen qso fenster habe
    // ich bis dato nie gesehen".
    void derSizeHintTraegtZweiKacheln()
    {
        RadioModel radio;
        Ft8PlaetzeApplet panel(&radio);

        // Leer -- und trotzdem gross genug fuer ein Fenster.
        panel.zeigeStand(Ft8PlaetzeStand{});
        QVERIFY2(panel.sizeHint().height()
                     >= 2 * Ft8PlaetzeFlaeche::kKachelH,
                 qPrintable(QStringLiteral("leer: %1 Punkte")
                                .arg(panel.sizeHint().height())));
        QVERIFY(panel.sizeHint().width() >= 300);

        // Mit drei Kacheln waechst er.
        Ft8PlaetzeStand st;
        for (int i = 0; i < 3; ++i) { st.plaetze.append(Ft8PlatzStand{}); }
        panel.zeigeStand(st);
        QVERIFY2(panel.sizeHint().height()
                     >= 3 * Ft8PlaetzeFlaeche::kKachelH,
                 qPrintable(QStringLiteral("drei Plaetze: %1 Punkte")
                                .arg(panel.sizeHint().height())));
    }

    // Das Panel waechst mit der Zahl der Plaetze, statt eine feste
    // Hoehe zu behaupten -- bei sechs Plaetzen waere die Haelfte
    // abgeschnitten.
    void diePanelhoeheWaechstMitDenPlaetzen()
    {
        Ft8PlaetzeFlaeche f;
        Ft8PlaetzeStand st;
        f.setzeStand(st);
        const int leer = f.noetigeHoehe();

        for (int i = 0; i < 3; ++i) { st.plaetze.append(Ft8PlatzStand{}); }
        f.setzeStand(st);
        const int drei = f.noetigeHoehe();
        QVERIFY2(drei > leer, "drei Plaetze brauchen mehr Hoehe als keiner");

        for (int i = 0; i < 3; ++i) { st.plaetze.append(Ft8PlatzStand{}); }
        f.setzeStand(st);
        QVERIFY2(f.noetigeHoehe() > drei,
                 "sechs Plaetze brauchen mehr Hoehe als drei");
        // Und zwar genau drei Kacheln mehr.
        QCOMPARE(f.noetigeHoehe() - drei,
                 3 * (Ft8PlaetzeFlaeche::kKachelH
                      + Ft8PlaetzeFlaeche::kKachelLuft));
    }
};

QTEST_MAIN(TstFt8PlaetzePanel)
#include "tst_ft8_plaetze_panel.moc"
