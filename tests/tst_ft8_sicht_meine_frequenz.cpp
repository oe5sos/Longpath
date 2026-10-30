// =================================================================
// tests/tst_ft8_sicht_meine_frequenz.cpp  (Longpath)
// =================================================================
//
// Die Sicht „My freq": was auf der eigenen Stelle liegt, und alles,
// was an mich geht.
//
// ── Woher das kommt (2026-10-21 → 2026-10-30) ────────────────────────
//
// Das war bis zum 2026-10-30 eine zweite SPALTE neben „Band Activity",
// wie in WSJT-X, und dieser Prueflauf hiesz `tst_ft8_zwei_spalten`. Im
// Panel sind zwei Spalten je 214 Punkte breit, und dort passt die
// Zeile nicht hinein: im gerenderten Blatt stand „Austr…" statt des
// Landes, und rechts fehlte das Ende der Nachricht. Ausserdem zeigte
// die rechte Spalte meistens dieselben Zeilen noch einmal.
//
// Seit Entwurf C ist es eine SICHT auf eine breite Liste. Die REGEL
// ist unveraendert — und sie ist das, was hier festgenagelt wird:
//   * ein Fund auf der eigenen Stelle gehoert hinein
//   * ein Fund daneben nicht
//   * ein Ruf an mich immer, egal wie weit weg er liegt
//   * die Grenze ist die Signalbreite (50 Hz), nicht „ungefaehr nah"
//
// Eine Sache hat sich mit Absicht geaendert, und sie hat hier ihren
// eigenen Fall: frueher stand im Prueflauf „das Vergangene wird nicht
// umsortiert -- eine Liste ist ein Protokoll, keine Ansicht." Mit dem
// Zeilenspeicher ist es eine Ansicht geworden. Wer die Sendestelle
// verschiebt, sieht die Sicht vollstaendig neu — auch rueckwirkend.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-10-30 — Von zwei Spalten auf die Sicht „My freq" umgestellt.
// =================================================================

#include <QtTest/QtTest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeZone>

#include "core/AppSettings.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstFt8SichtMeineFrequenz : public QObject {
    Q_OBJECT

private:
    static QVector<Ft8Decode> einFund(const QString& text, float hz)
    {
        Ft8Decode d;
        d.snrDb = -12.0f;
        d.dtSeconds = 0.2f;
        d.freqHz = hz;
        d.score = 40;
        d.text = text;
        return {d};
    }

    static void speise(Ft8Applet& applet, const QString& text, float hz)
    {
        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>, einFund(text, hz)),
            Q_ARG(QDateTime, fenster));
    }

    /// Die Liste, wie sie in einer Sicht aussieht.
    static QString inSicht(Ft8Applet& applet, QPushButton* pille)
    {
        pille->click();
        return applet.listForTest()->toPlainText();
    }

private slots:

    void initTestCase()
    {
        // Das eigene Rufzeichen entscheidet, was „an mich" heisst.
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
    }

    void dieFuenfSichtenSindDa()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY2(applet.listForTest(), "die Liste fehlt");
        QVERIFY2(applet.sichtAllesButtonForTest(), "die Pille All fehlt");
        QVERIFY2(applet.sichtAnMichButtonForTest(), "die Pille To me fehlt");
        QVERIFY2(applet.nurCqButtonForTest(), "die Pille CQ fehlt");
        QVERIFY2(applet.nurNeueLaenderButtonForTest(), "die Pille New DXCC fehlt");
        QVERIFY2(applet.sichtFreqButtonForTest(), "die Pille My freq fehlt");
        // „All" ist die Vorgabe: wer das Fenster oeffnet, soll das
        // Band sehen, nicht einen Ausschnitt davon.
        QVERIFY(applet.sichtAllesButtonForTest()->isChecked());
    }

    void aufDerEigenenStelleGehoertEsHinein()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.sendeTonFeldForTest());
        applet.sendeTonFeldForTest()->setValue(1500);

        // Genau auf der Stelle, und am Rand des 50-Hz-Fensters.
        speise(applet, QStringLiteral("CQ DL1ABC JN49"), 1500.0f);
        speise(applet, QStringLiteral("CQ DL2ABC JN49"), 1550.0f);

        const QString alles = applet.listForTest()->toPlainText();
        QVERIFY2(alles.contains(QStringLiteral("DL1ABC")), qPrintable(alles));
        const QString meine =
            inSicht(applet, applet.sichtFreqButtonForTest());
        QVERIFY2(meine.contains(QStringLiteral("DL1ABC")), qPrintable(meine));
        QVERIFY2(meine.contains(QStringLiteral("DL2ABC")),
                 "der Fund am Rand des Fensters fehlt");
    }

    void danebenGehoertEsNichtHinein()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);

        // 51 Hz daneben: EIN Hertz ausserhalb. Das ist die Gegenprobe
        // zum Fall darueber -- ohne sie wuerde eine Spalte, die ALLES
        // zeigt, genauso gruen melden.
        speise(applet, QStringLiteral("CQ OK1XYZ JO70"), 1551.0f);
        speise(applet, QStringLiteral("CQ SP9ABC JO90"), 900.0f);

        const QString alles = applet.listForTest()->toPlainText();
        QVERIFY2(alles.contains(QStringLiteral("OK1XYZ")), qPrintable(alles));
        QVERIFY2(alles.contains(QStringLiteral("SP9ABC")), qPrintable(alles));
        const QString meine =
            inSicht(applet, applet.sichtFreqButtonForTest());
        QVERIFY2(!meine.contains(QStringLiteral("OK1XYZ")), qPrintable(meine));
        QVERIFY2(!meine.contains(QStringLiteral("SP9ABC")), qPrintable(meine));
    }

    void wasAnMichGehtGehoertHineinEgalWoEsLiegt()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);

        // 1300 Hz weg von der eigenen Stelle -- aber an mich.
        speise(applet, QStringLiteral("OE5SOS DL1ABC -07"), 200.0f);

        const QString meine =
            inSicht(applet, applet.sichtFreqButtonForTest());
        QVERIFY2(meine.contains(QStringLiteral("OE5SOS DL1ABC")),
                 qPrintable(meine));

        // Und die Gegenprobe am Wortrand: OE5SOSX ist eine ANDERE
        // Station. Sie darf die rechte Spalte nicht erreichen.
        speise(applet, QStringLiteral("OE5SOSX DL1ABC -07"), 210.0f);
        const QString meine2 = applet.listForTest()->toPlainText();
        QVERIFY2(!meine2.contains(QStringLiteral("OE5SOSX")),
                 qPrintable(meine2));
    }

    void einBlattZumAnsehen()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);
        applet.resize(620, 420);
        applet.show();
        QTest::qWait(40);
        speise(applet, QStringLiteral("CQ DL1ABC JN49"), 1500.0f);
        speise(applet, QStringLiteral("OE5SOS DL1ABC -07"), 320.0f);
        speise(applet, QStringLiteral("CQ OK1XYZ JO70"), 1890.0f);
        speise(applet, QStringLiteral("CQ SP9ABC JO90"), 905.0f);
        speise(applet, QStringLiteral("DL1ABC OE5SOS RR73"), 1520.0f);
        speise(applet, QStringLiteral("CQ EA7XYZ IM76"), 2210.0f);
        QTest::qWait(60);
        const QString pfad = QDir::tempPath()
                             + QStringLiteral("/longpath-sicht-meine-frequenz.png");
        QVERIFY(applet.grab().save(pfad));
        qInfo() << "Blatt:" << pfad;
    }

    // ── Die eine Umkehr gegenueber den zwei Spalten ──────────────────
    void dieStelleWandertMitUndZwarRueckwirkend()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);
        speise(applet, QStringLiteral("CQ DL1ABC JN49"), 2000.0f);
        QVERIFY(!inSicht(applet, applet.sichtFreqButtonForTest())
                     .contains(QStringLiteral("DL1ABC")));

        // Stelle auf 2000 ziehen.
        applet.sendeTonFeldForTest()->setValue(2000);
        speise(applet, QStringLiteral("CQ HB9XYZ JN47"), 2000.0f);
        const QString meine =
            inSicht(applet, applet.sichtFreqButtonForTest());
        QVERIFY2(meine.contains(QStringLiteral("HB9XYZ")),
                 qPrintable(meine));

        // UND der alte Fund auf 2000 ist jetzt auch dabei.
        //
        // Hier stand bis zum 2026-10-30 das Gegenteil: „das Vergangene
        // wird nicht umsortiert -- eine Liste ist ein Protokoll, keine
        // Ansicht." Mit zwei Spalten stimmte das, weil eine Zeile beim
        // Eintreffen einmal einsortiert wurde und dann lag. Eine SICHT
        // auf einen Speicher ist etwas anderes: sie zeigt, was gerade
        // gilt. Wer die Stelle verschiebt und dann „My freq" waehlt,
        // will wissen, was DORT los ist -- nicht, was dort los war,
        // als er noch woanders stand.
        QVERIFY2(meine.contains(QStringLiteral("DL1ABC")),
                 qPrintable(meine));
    }
};

QTEST_MAIN(TstFt8SichtMeineFrequenz)
#include "tst_ft8_sicht_meine_frequenz.moc"
