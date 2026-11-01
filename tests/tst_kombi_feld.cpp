// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_kombi_feld.cpp  (Longpath)
// =================================================================
//
// Betreiber, 2026-11-01: „in dieses leere widget sollte man generell
// unterschiedlich widget reinziehen können" -- zuerst am Beispiel
// RX und TX, dann allgemein.
//
// Der Kern ist, dasz ein aufgenommenes Panel DASSELBE Objekt bleibt:
// `RxApplet` und `TxApplet` haengen an einem halben Dutzend Dingen,
// die MainWindow ihnen von auszen reicht. Wer sie neu baut, baut die
// Verdrahtung neu -- und vergiszt eine.
// =================================================================

#include <QtTest/QtTest>

#include "gui/applets/KombiApplet.h"

using namespace Longpath;

namespace {

// Ein Panel, das nichts kann auszer sich melden. Genug: geprueft wird
// der Behaelter, nicht sein Inhalt.
class Probepanel : public AppletWidget
{
public:
    explicit Probepanel(const QString& id, const QString& titel)
        : AppletWidget(nullptr, nullptr), m_id(id), m_titel(titel) {}

    QString appletId() const override    { return m_id; }
    QString appletTitle() const override { return m_titel; }
    void    syncFromModel() override     { ++abgeglichen; }

    int abgeglichen{0};

private:
    QString m_id;
    QString m_titel;
};

}  // namespace

class TstKombiFeld : public QObject
{
    Q_OBJECT

private slots:
    // Leer ist leer -- und sagt es.
    void leerIstLeer()
    {
        KombiApplet k(nullptr);
        QVERIFY(k.istLeer());
        QVERIFY(k.seiten().isEmpty());
        QVERIFY(!k.hasExtendedSettings());
    }

    // ── Aufnehmen ist ein Elternwechsel, kein Neubau ────────────────
    //
    // Der wichtigste Fall ueberhaupt: dasselbe Objekt, kein zweites.
    void aufnehmenBehaeltDasselbeObjekt()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        k.nimmAuf(rx);
        QCOMPARE(k.seiten().size(), 1);
        QCOMPARE(k.seiten().first(), static_cast<AppletWidget*>(rx));
        QVERIFY(!k.istLeer());
        // Und es haengt wirklich im Behaelter, nicht irgendwo.
        QVERIFY(rx->parentWidget() != nullptr);
    }

    // Zweimal dasselbe aufnehmen ist kein zweiter Reiter.
    void zweimalDasselbeIstEinmal()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        k.nimmAuf(rx);
        k.nimmAuf(rx);
        QCOMPARE(k.seiten().size(), 1);
    }

    // Der Titel sagt, was drin ist -- nicht „Kombi".
    void derTitelNenntDenInhalt()
    {
        KombiApplet k(nullptr);
        k.nimmAuf(new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX")));
        k.nimmAuf(new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX")));
        QCOMPARE(k.appletTitle(), QStringLiteral("RX / TX"));
    }

    // Umschalten zeigt die andere Seite, und die Wahl bleibt in den
    // Grenzen.
    void umschaltenBleibtInDenGrenzen()
    {
        KombiApplet k(nullptr);
        k.nimmAuf(new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX")));
        k.nimmAuf(new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX")));
        k.zeigeSeite(1);
        QCOMPARE(k.seite(), 1);
        k.zeigeSeite(0);
        QCOMPARE(k.seite(), 0);
        k.zeigeSeite(7);
        QCOMPARE(k.seite(), 1);
        k.zeigeSeite(-3);
        QCOMPARE(k.seite(), 0);
    }

    // ── Herausloesen gibt es unversehrt zurueck ─────────────────────
    //
    // Ohne Elternteil, aber am Leben: der Aufrufer legt es in ein
    // eigenes Feld. Wer es hier abbaute, haette alle Verbindungen
    // mitgenommen.
    void herausloesenGibtDasObjektZurueck()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);

        QSignalSpy spion(&k, &KombiApplet::herausgeloest);
        k.gibFrei(rx);
        QCOMPARE(spion.count(), 1);
        QCOMPARE(spion.first().first().value<AppletWidget*>(),
                 static_cast<AppletWidget*>(rx));
        QCOMPARE(k.seiten().size(), 1);
        QCOMPARE(k.seiten().first(), static_cast<AppletWidget*>(tx));
        QVERIFY2(rx->parentWidget() == nullptr,
                 "herausgeloest heiszt ohne Elternteil -- sonst haengt es "
                 "noch im Behaelter und bekommt nie ein eigenes Feld");
        delete rx;
    }

    // Alle hergeben leert das Feld und gibt die Reihenfolge zurueck.
    void alleHergebenLeertDasFeld()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);
        const QList<AppletWidget*> raus = k.gibAlleFrei();
        QCOMPARE(raus.size(), 2);
        QCOMPARE(raus.at(0), static_cast<AppletWidget*>(rx));
        QCOMPARE(raus.at(1), static_cast<AppletWidget*>(tx));
        QVERIFY(k.istLeer());
        delete rx;
        delete tx;
    }

    // Der Abgleich geht an ALLE, nicht nur an die sichtbare Seite:
    // wer umschaltet, soll keinen veralteten Stand vorfinden.
    void derAbgleichGehtAnAlleSeiten()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);
        k.zeigeSeite(0);
        k.syncFromModel();
        QCOMPARE(rx->abgeglichen, 1);
        QVERIFY2(tx->abgeglichen == 1,
                 "die unsichtbare Seite wurde nicht abgeglichen — beim "
                 "Umschalten stuende dort ein alter Stand");
    }
};

QTEST_MAIN(TstKombiFeld)
#include "tst_kombi_feld.moc"
