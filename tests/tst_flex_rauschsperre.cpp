// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_rauschsperre.cpp  (Longpath)
// =================================================================
//
// Betreiber 2026-10-31: „wo kann ich den squelch einstellen" -- und
// auf die Antwort „RX-Applet, Knopf SQL": „der funktioniert nicht",
// „schieberegler ohne änderung".
//
// Unsere Rauschsperre schreibt in `RxChannel` (WDSP). Eine
// Flex-Scheibe ist aber eine Scheibe OHNE RxChannel --
// `MainWindow_Flex.cpp:45` sagt das selbst. Der Regler hatte kein
// Ziel.
//
// Die Maschine hat eine eigene. Direkt von ihr gelesen
// (`sub slice all`, nur lesend):  squelch=0 squelch_level=20
// =================================================================

#include <QtTest/QtTest>

#include <QRegularExpression>

#include "core/FlexRadioConnection.h"

using namespace Longpath;

class TstFlexRauschsperre : public QObject
{
    Q_OBJECT

private slots:
    // Die beiden Schluessel stehen so, wie die Maschine sie nennt.
    void dieBefehleTragenDieNamenDerMaschine()
    {
        QCOMPARE(FlexCommandWire::setSliceSquelchCommand(QStringLiteral("0"), true),
                 QStringLiteral("slice set 0 squelch=1"));
        QCOMPARE(FlexCommandWire::setSliceSquelchCommand(QStringLiteral("0"), false),
                 QStringLiteral("slice set 0 squelch=0"));
        QCOMPARE(FlexCommandWire::setSliceSquelchLevelCommand(QStringLiteral("0"), 20),
                 QStringLiteral("slice set 0 squelch_level=20"));
    }

    // Ohne Scheibenkennung geht nichts hinaus -- ein Befehl an
    // niemanden ist schlimmer als keiner.
    void ohneScheibeKeinBefehl()
    {
        QVERIFY(FlexCommandWire::setSliceSquelchCommand(QString(), true).isEmpty());
        QVERIFY(FlexCommandWire::setSliceSquelchLevelCommand(QString(), 20).isEmpty());
    }

    // Die Stufe laeuft von 0 bis 100, wie der Schieber im RX-Applet.
    void dieStufeBleibtImBereich()
    {
        QVERIFY(!FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), 0).isEmpty());
        QVERIFY(!FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), 100).isEmpty());
        QVERIFY(FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), 101).isEmpty());
        QVERIFY(FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), -1).isEmpty());
    }

    // ── Und der Regler erreicht den Weg wirklich ────────────────────
    //
    // Der eigentliche Fehler war nicht ein fehlender Befehl, sondern
    // ein Regler ohne Ziel. Dieser Fall haelt die Verdrahtung fest.
    void derReglerErreichtDieMaschine()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        // JEDEN Rumpf einzeln, bis zu seinem eigenen Ende. Ein Fenster
        // fester Groesse ab dem ersten Treffer reicht NICHT: der Name
        // steht mehrfach in der Datei, und das Fenster erwischte dann
        // den Nachbarn. Beim ersten Versuch am 2026-10-31 blieb die
        // Gegenprobe deswegen gruen, obwohl die Zeile entfernt war --
        // ein Waechter, der nicht faellt, waecht nicht.
        for (const QString& signal : {QStringLiteral("&SliceModel::ssqlEnabledChanged"),
                                      QStringLiteral("&SliceModel::ssqlThreshChanged")}) {
            const int a = src.indexOf(signal);
            QVERIFY2(a > 0, qPrintable(signal));
            const int e = src.indexOf(QStringLiteral("\n    });"), a);
            QVERIFY2(e > a, qPrintable(signal));
            QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("sendeFlexRauschsperre")),
                     qPrintable(signal + QStringLiteral(
                         " geht nicht an die Maschine — bei der Flex "
                         "bewirkt der Regler dann wieder nichts")));
        }
    }

    // ── Der Wunsch kommt vor der Scheibennummer ─────────────────────
    //
    // Gemessen an Martins Protokoll vom 2026-10-31:
    //
    //   19:54:36.430  Flex: Leistung nachgezogen   (hier will sie hinaus)
    //   19:54:36.573  Flex: Ton auf Scheibe "0"    (hier erst ist die Nummer da)
    //
    // 143 ms. In dieser Luecke fiel die Rauschsperre still heraus --
    // der Knopf stand auf an, die Maschine behielt ihren alten Stand.
    // Also vormerken statt verwerfen.
    void ohneScheibeWirdVorgemerkt()
    {
        const QString rumpf = funktionsRumpf(
            QStringLiteral("void RadioModel::sendeFlexRauschsperre"));
        // Leerzeichen zwischen Name und Zuweisung sind Ausrichtung,
        // keine Aussage -- der Waechter darf daran nicht haengen.
        QVERIFY2(rumpf.contains(QRegularExpression(QStringLiteral(
                     "m_flexHatRauschsperreNachzutragen\\s*=\\s*true"))),
                 "ohne Scheibennummer wird der Wunsch verworfen statt vorgemerkt — "
                 "beim Anmelden geht die Rauschsperre dann nie hinaus");
    }

    // Und das Vorgemerkte musz auch wirklich nachgereicht werden,
    // sobald die Maschine ihre Scheibe meldet -- an derselben Stelle,
    // an der Art und Flanken das schon tun.
    void dieScheibenmeldungTraegtNach()
    {
        const QString src = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        const int a = src.indexOf(QStringLiteral("m_flexSliceId       = object.split"));
        QVERIFY2(a > 0, "die Stelle, an der die Scheibennummer ankommt, ist weg");
        const int e = src.indexOf(QStringLiteral("m_flexHatFlankenNachzutragen"), a);
        QVERIFY2(e > a, "der Nachtragsblock ist weg");
        QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("m_flexHatRauschsperreNachzutragen")),
                 "die Scheibenmeldung reicht die vorgemerkte Rauschsperre nicht nach");
    }

    // ── Eine Ablehnung musz sagen, WAS abgelehnt wurde ──────────────
    //
    // Im selben Protokoll stand „Befehl 1 abgelehnt, Code 0x10000002"
    // -- ohne den Wortlaut. Bei „funktioniert nicht" ist genau das
    // die Frage.
    void dieAblehnungNenntDenWortlaut()
    {
        const QString src = quelle(QStringLiteral("/src/core/FlexRadioConnection.cpp"));
        const int a = src.indexOf(QStringLiteral("abgelehnt, Code"));
        QVERIFY2(a > 0, "die Ablehnungsmeldung ist weg");
        // NUR die Meldung selbst, bis zu ihrem Strichpunkt. Weiter
        // gefasst haette dieser Waechter nicht gebissen: das
        // Aufraeumen danach nennt denselben Namen und haette den
        // Fall gruen gehalten, obwohl die Meldung ihn nicht mehr
        // ausgibt (am 2026-10-31 genau so passiert).
        const int e = src.indexOf(QLatin1Char(';'), a);
        QVERIFY2(e > a, "der Antwortzweig sieht anders aus als erwartet");
        QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("wortlaut")),
                 "die Ablehnung nennt weiterhin nur die laufende Nummer, "
                 "nicht den Befehl");
    }

    // ── Und der Flex-Weg musz sie selbst verdrahten ────────────────
    //
    // `RadioModel::wireSliceSignals` steigt in seiner zweiten Zeile
    // aus, wenn `m_connection` fehlt -- das ist die
    // OpenHPSDR-Verbindung, am Flex immer null. Das ganze
    // DSP-Bedienfeld der Scheibe haengt dort also an nichts. Gemessen
    // am 2026-10-31: „SQL-Knopf an (Scheibe 0)" im Protokoll, und
    // danach keine einzige Zeile „Flex: Rauschsperre".
    //
    // Darum verdrahtet der Flex-Weg Art, Flanken und Rauschminderung
    // selbst -- und ab jetzt auch die Rauschsperre.
    void derFlexWegVerdrahtetSieSelbst()
    {
        const QString src = quelle(QStringLiteral("/src/gui/MainWindow_Flex.cpp"));
        QVERIFY2(!src.isEmpty(), "MainWindow_Flex.cpp nicht lesbar");
        for (const QString& signal : {QStringLiteral("SliceModel::ssqlEnabledChanged"),
                                      QStringLiteral("SliceModel::ssqlThreshChanged")}) {
            QVERIFY2(src.contains(signal),
                     qPrintable(signal + QStringLiteral(
                         " haengt am Flex an nichts — wireSliceSignals "
                         "laeuft dort nicht (sie braucht m_connection)")));
        }
        QVERIFY2(src.contains(QStringLiteral("sendeFlexRauschsperre")),
                 "der Flex-Weg schickt die Rauschsperre nicht hinaus");
    }

    // Die Gegenseite des Riegels oben: dasz `wireSliceSignals` wirklich
    // an `m_connection` haengt, ist kein Verdacht, sondern steht da.
    // Faellt diese Zeile einmal weg, ist der Grund fuer die eigene
    // Verdrahtung weg -- und dieser Fall sagt es.
    void wireSliceSignalsBrauchtDieAndereVerbindung()
    {
        const QString src = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        const int a = src.indexOf(QStringLiteral("void RadioModel::wireSliceSignals"));
        QVERIFY2(a > 0, "wireSliceSignals ist weg");
        const int e = src.indexOf(QStringLiteral("connect("), a);
        QVERIFY2(e > a, "wireSliceSignals sieht anders aus als erwartet");
        QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("!m_connection")),
                 "wireSliceSignals haengt nicht mehr an m_connection — dann "
                 "gehoert die eigene Flex-Verdrahtung auf den Pruefstand");
    }

private:
    static QString quelle(const QString& relativ)
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { return QString(); }
        QFile f(QString::fromLocal8Bit(root) + relativ);
        if (!f.open(QIODevice::ReadOnly)) { return QString(); }
        return QString::fromUtf8(f.readAll());
    }

    // Vom Kopf bis zur schliessenden Klammer in Spalte eins -- nicht
    // ein Fenster fester Groesse: siehe die Lehre oben.
    static QString funktionsRumpf(const QString& kopf)
    {
        const QString src = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        const int a = src.indexOf(kopf);
        if (a < 0) { return QString(); }
        const int e = src.indexOf(QStringLiteral("\n}\n"), a);
        if (e < a) { return QString(); }
        return src.mid(a, e - a);
    }
};

QTEST_MAIN(TstFlexRauschsperre)
#include "tst_flex_rauschsperre.moc"
