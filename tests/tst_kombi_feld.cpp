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
        QCOMPARE(k.seiten().first(), static_cast<QWidget*>(rx));
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

    // ── Der Name bleibt, der Inhalt wechselt ────────────────────────
    //
    // Betreiber, 2026-11-01: „das RX/TX (combined) panel sollte
    // generell combined heißen, sofern man hier dann alle widget
    // reinziehen kann!" Ein Name, der mit dem Inhalt wandert, haette
    // das Feld jedes Mal zu etwas anderem gemacht.
    void derNameStehtNurBeimLeerenFeld()
    {
        KombiApplet k(nullptr);
        // Leer: der Name musz dastehen, sonst weisz niemand, was das
        // Feld ist.
        QCOMPARE(k.appletTitle(), QStringLiteral("Combined"));
        k.nimmAuf(new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX")));
        k.nimmAuf(new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX")));
        // Voll: kein ausgeschriebener Name mehr. Betreiber,
        // 2026-11-01: „also Combined muss ja nicht ausgeschrieben
        // sein" -- das Wort sagt nichts, was die Pillen nicht zeigen,
        // und nimmt ihnen in einer schmalen Spalte den Platz.
        QVERIFY2(k.appletTitle().isEmpty(),
                 "der ausgeschriebene Name nimmt den Pillen den Platz");
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
        QCOMPARE(spion.first().first().value<QWidget*>(),
                 static_cast<QWidget*>(rx));
        QCOMPARE(k.seiten().size(), 1);
        QCOMPARE(k.seiten().first(), static_cast<QWidget*>(tx));
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
        const QList<QWidget*> raus = k.gibAlleFrei();
        QCOMPARE(raus.size(), 2);
        QCOMPARE(raus.at(0), static_cast<QWidget*>(rx));
        QCOMPARE(raus.at(1), static_cast<QWidget*>(tx));
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

    // ── Ueber die Fenstergrenze hinweg ──────────────────────────────
    //
    // Betreiber, 2026-11-01: „kann das rx panel nicht reinziehen".
    // Sein RX sitzt in der SPALTE (Fenster „RX1 Main Panel"), das
    // Kombi-Feld schwebt DANEBEN als eigenes Fenster. Beide
    // Ablegewege suchten vorher nur in ihrem eigenen Gehaeuse und
    // fanden ueber die Fenstergrenze nichts.
    //
    // `unter()` kennt keine Gehaeuse: es vergleicht Rechtecke auf dem
    // Schirm. Dieser Fall stellt zwei getrennte Fenster auf und
    // fragt nach Stellen in beiden.
    void unterFindetUeberFenstergrenzen()
    {
        QWidget fensterA;
        fensterA.setGeometry(40, 40, 200, 120);
        auto* a = new KombiApplet(nullptr, &fensterA);
        a->setGeometry(0, 0, 200, 120);
        fensterA.show();

        QWidget fensterB;
        fensterB.setGeometry(400, 300, 200, 120);
        auto* b = new KombiApplet(nullptr, &fensterB);
        b->setGeometry(0, 0, 200, 120);
        fensterB.show();
        QTest::qWait(50);

        QCOMPARE(KombiApplet::unter(a->mapToGlobal(QPoint(10, 10))), a);
        QCOMPARE(KombiApplet::unter(b->mapToGlobal(QPoint(10, 10))), b);
        QVERIFY2(KombiApplet::unter(QPoint(-5000, -5000)) == nullptr,
                 "irgendwo im Nirgendwo darf kein Feld liegen");
    }

    // Und beide Ablegewege muessen DIESE Liste fragen -- zwei Suchen
    // an zwei Stellen laufen frueher oder spaeter auseinander.
    void beideWegeFragenDieselbeListe()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        const auto lies = [&](const QString& rel) {
            QFile f(QString::fromLocal8Bit(root) + rel);
            return f.open(QIODevice::ReadOnly)
                       ? QString::fromUtf8(f.readAll()) : QString();
        };
        for (const QString& datei : {QStringLiteral("/src/gui/applets/AppletPanelWidget.cpp"),
                                     QStringLiteral("/src/gui/MainWindow.cpp")}) {
            const QString src = lies(datei);
            // Mit Klammer. Ohne sie fand dieser Waechter am
            // 2026-11-01 seinen eigenen KOMMENTAR („`KombiApplet::unter`
            // kennt keine Gehaeuse") und blieb gruen, obwohl der Ruf
            // entfernt war. Ein Waechter, der nicht faellt, waecht
            // nicht.
            QVERIFY2(src.contains(QStringLiteral("KombiApplet::unter(")),
                     qPrintable(datei + QStringLiteral(
                         " sucht das Kombi-Feld wieder selbst — dann findet "
                         "es nichts, was in einem anderen Fenster liegt")));
        }
    }

    // ── Ein Panel hat ZWEI Gehaeuse ─────────────────────────────────
    //
    // Betreiber, 2026-11-01: „nimmt es nicht auf". Der erste Versuch
    // hing nur am Ziehen in der SPALTE -- seine Panels schweben aber
    // alle. Dieselbe Lehre wie beim Zahnrad am 2026-10-31: wer nur
    // ein Gehaeuse bedient, baut etwas, das der Betreiber nicht
    // erreicht.
    //
    // Darum beide Wege festhalten: die Spalte fragt beim Loslassen,
    // worueber gezogen wurde; das schwebende Fenster meldet, wo es
    // abgelegt wurde.
    void beideGehaeuseNehmenAuf()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        const auto lies = [&](const QString& rel) {
            QFile f(QString::fromLocal8Bit(root) + rel);
            return f.open(QIODevice::ReadOnly)
                       ? QString::fromUtf8(f.readAll()) : QString();
        };

        const QString spalte = lies(QStringLiteral(
            "/src/gui/applets/AppletPanelWidget.cpp"));
        QVERIFY2(spalte.contains(QStringLiteral("nimmKombiAuf")),
                 "die Spalte fragt beim Loslassen nicht mehr, ob dort ein "
                 "Kombi-Feld liegt");

        const QString fenster = lies(QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY2(fenster.contains(QStringLiteral("AppletFloatingWindow::abgelegtBei")),
                 "ein SCHWEBENDES Panel kann nicht mehr abgelegt werden — und "
                 "genau die schweben beim Betreiber alle");
        QVERIFY2(fenster.contains(QStringLiteral("legeAufKombiFeld")),
                 "das Ablegen fuehrt ins Leere");

        // Und der Name eines Werkzeugfensters muss beim Hineinlegen
        // MITGEHEN: er haengt am Fenster, und das Fenster verschwindet.
        const int a = fenster.indexOf(QStringLiteral("ToolWindow::abgelegtBei"));
        QVERIFY2(a > 0, "ein Werkzeugfenster kann nicht abgelegt werden");
        const int e = fenster.indexOf(QStringLiteral("    });"), a);
        QVERIFY2(e > a, "der Ablegeweg sieht anders aus als erwartet");
        QVERIFY2(fenster.mid(a, e - a).contains(QStringLiteral("setWindowTitle")),
                 "der Name bleibt am verschwindenden Fenster haengen — auf "
                 "dem Reiter steht dann „Panel“");
    }

    // ── Rechtsklick fragt nach der Liste ────────────────────────────
    //
    // Betreiber, 2026-11-01: „oder man öffnet das leere combined
    // fenster, rechte moustaste und dann sieht man alles, was in das
    // fenster kommen kann und klickt es an."
    //
    // Das Feld selbst kennt die Liste nicht -- es fragt nur. Wer sie
    // kennt (das Hauptfenster), zeigt sie.
    void rechtsklickFragtNachDerListe()
    {
        KombiApplet k(nullptr);
        k.resize(200, 120);
        k.show();
        QSignalSpy spion(&k, &KombiApplet::listeGewuenscht);
        emit k.customContextMenuRequested(QPoint(10, 10));
        QCOMPARE(spion.count(), 1);

        // ── UEBERALL, nicht nur auf dem Feldkoerper ──────────────────
        //
        // Betreiber, 2026-11-01, mit Bild („rechte maustaste
        // funktioniert nicht") und dann klar: „muss immer und überall
        // funktionieren in im combined". Rollbereich, Stapel und
        // Hinweis beantworten einen Rechtsklick sonst mit einem leeren
        // Menue und schlucken ihn.
        int gefragt = 0;
        for (QWidget* teil : k.findChildren<QWidget*>()) {
            if (teil->contextMenuPolicy() == Qt::CustomContextMenu) {
                ++gefragt;
            }
        }
        QVERIFY2(gefragt >= 4,
                 qPrintable(QStringLiteral(
                     "nur %1 Stuecke im Feld fragen nach der Liste — "
                     "ein Rechtsklick landet dann irgendwo im Nichts")
                         .arg(gefragt)));

        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { return; }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString src = QString::fromUtf8(f.readAll());
        QVERIFY2(src.contains(QStringLiteral("KombiApplet::listeGewuenscht")),
                 "niemand zeigt die Liste — der Rechtsklick fuehrt ins Leere");
        QVERIFY2(src.contains(QStringLiteral("holeInsKombiFeld")),
                 "die Liste legt nichts hinein");
    }

    // ── Der Fall, der beim Betreiber schiefging ─────────────────────
    //
    // „genau mitten im fenster" -- und es kam das Menue des
    // Containers. Ein Rechtsklick geht an das Widget UNTER dem
    // Zeiger; reicht das ihn weiter, laeuft er die Elternkette hoch.
    //
    // Dieser Fall schickt den Klick genau dorthin: auf eine SEITE im
    // Feld, nicht auf das Feld selbst. Er darf nicht nach oben
    // durchlaufen.
    //
    // EHRLICH DAZU: dieser Lauf bleibt auch OHNE das Fangnetz gruen
    // (Gegenprobe gefahren, 2026-11-01). Er haelt also den normalen
    // Weg fest -- der schon vorher ging --, nicht den Fehler des
    // Betreibers. Was bei ihm den Klick nach oben durchlieszt, ist
    // damit NICHT gefunden; das Netz faengt es nur ab. Wer hier
    // weitersucht: der Unterschied musz in der wirklichen Seite
    // liegen (Rotorpanel, Bandbreitenfilter), nicht im Behaelter.
    void rechtsklickAufEinerSeiteKommtAn()
    {
        auto* rahmen = new QWidget;        // steht fuer den Container
        rahmen->setGeometry(0, 0, 300, 200);
        auto* k = new KombiApplet(nullptr, rahmen);
        k->setGeometry(0, 0, 300, 200);
        auto* seite = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        k->nimmAuf(seite);
        rahmen->show();
        QTest::qWait(50);

        QSignalSpy spion(k, &KombiApplet::listeGewuenscht);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, QPoint(20, 20),
                             seite->mapToGlobal(QPoint(20, 20)));
        QVERIFY2(QApplication::sendEvent(seite, &ev),
                 "der Rechtsklick wurde nicht beantwortet — er laeuft dann "
                 "die Elternkette hoch bis zum Container, und DESSEN Menue "
                 "erscheint (Betreiber, 2026-11-01)");
        QCOMPARE(spion.count(), 1);
        delete rahmen;
    }

    // ── Jedes Widget, nicht nur ein Applet ──────────────────────────
    //
    // Betreiber, 2026-11-01: „ich will da auch den rotor und andere
    // sache reinziehen können" -- und der Rotor ist kein Applet,
    // sondern der Inhalt eines Werkzeugfensters. Ein Behaelter, der
    // nach der Art fragt, kann ihn nicht aufnehmen.
    void auchEinFremdesWidget()
    {
        KombiApplet k(nullptr);
        auto* fremd = new QWidget;
        fremd->setWindowTitle(QStringLiteral("Rotor / Log"));
        k.nimmAuf(fremd);
        QCOMPARE(k.seiten().size(), 1);
        // Der Name kommt vom Fenstertitel -- ohne ihn stand auf dem
        // Reiter „Panel" (Betreiber, 2026-11-01, mit Bild).
        QCOMPARE(KombiApplet::nameVon(fremd), QStringLiteral("Rotor / Log"));
        QCOMPARE(k.seiten().first(), fremd);
        QVERIFY(!k.istLeer());
        k.gibFrei(fremd);
        QVERIFY(k.istLeer());
        QVERIFY2(fremd->parentWidget() == nullptr,
                 "auch ein fremdes Widget kommt ohne Elternteil zurueck");
        delete fremd;
    }

    // Und der Abgleich darf daran nicht scheitern: ein fremdes
    // Widget hat kein syncFromModel.
    void derAbgleichStolpertNichtUeberFremde()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* fremd = new QWidget;
        k.nimmAuf(rx);
        k.nimmAuf(fremd);
        k.syncFromModel();          // darf nicht stuerzen
        QCOMPARE(rx->abgeglichen, 1);
    }
};

QTEST_MAIN(TstKombiFeld)
#include "tst_kombi_feld.moc"
