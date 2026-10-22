// =================================================================
// tests/tst_ft8_zwei_spalten.cpp  (Longpath)
// =================================================================
//
// Die zweite Spalte: „Rx Frequency" neben „Band Activity", wie in
// WSJT-X. Bis zum 2026-10-21 hatte das Fenster nur EINE Liste -- in
// einem vollen Band ist sie nach zwei Durchgaengen voll, und die eine
// Zeile, auf die es ankommt (die eigene Stelle, oder ein Ruf an mich),
// scrollt weg.
//
// Was hier festgenagelt wird:
//   * ein Fund auf der eigenen Stelle steht in BEIDEN Listen
//   * ein Fund daneben steht NUR links
//   * ein Ruf an mich steht rechts, egal wie weit weg er liegt
//   * die Grenze ist die Signalbreite (50 Hz), nicht „ungefaehr nah"
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTimeZone>

#include "core/AppSettings.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstFt8ZweiSpalten : public QObject {
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

private slots:

    void initTestCase()
    {
        // Das eigene Rufzeichen entscheidet, was „an mich" heisst.
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
    }

    void beideSpaltenSindDa()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY2(applet.listForTest(), "die linke Liste fehlt");
        QVERIFY2(applet.listRxForTest(), "die rechte Liste fehlt");
        QVERIFY(applet.listForTest() != applet.listRxForTest());
    }

    void aufDerEigenenStelleStehtEsInBeiden()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.sendeTonFeldForTest());
        applet.sendeTonFeldForTest()->setValue(1500);

        // Genau auf der Stelle, und am Rand des 50-Hz-Fensters.
        speise(applet, QStringLiteral("CQ DL1ABC JN49"), 1500.0f);
        speise(applet, QStringLiteral("CQ DL2ABC JN49"), 1550.0f);

        const QString links  = applet.listForTest()->toPlainText();
        const QString rechts = applet.listRxForTest()->toPlainText();
        QVERIFY2(links.contains(QStringLiteral("DL1ABC")), qPrintable(links));
        QVERIFY2(rechts.contains(QStringLiteral("DL1ABC")), qPrintable(rechts));
        QVERIFY2(rechts.contains(QStringLiteral("DL2ABC")),
                 "der Fund am Rand des Fensters fehlt rechts");
    }

    void danebenStehtEsNurLinks()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);

        // 51 Hz daneben: EIN Hertz ausserhalb. Das ist die Gegenprobe
        // zum Fall darueber -- ohne sie wuerde eine Spalte, die ALLES
        // zeigt, genauso gruen melden.
        speise(applet, QStringLiteral("CQ OK1XYZ JO70"), 1551.0f);
        speise(applet, QStringLiteral("CQ SP9ABC JO90"), 900.0f);

        const QString links  = applet.listForTest()->toPlainText();
        const QString rechts = applet.listRxForTest()->toPlainText();
        QVERIFY2(links.contains(QStringLiteral("OK1XYZ")), qPrintable(links));
        QVERIFY2(links.contains(QStringLiteral("SP9ABC")), qPrintable(links));
        QVERIFY2(!rechts.contains(QStringLiteral("OK1XYZ")),
                 qPrintable(rechts));
        QVERIFY2(!rechts.contains(QStringLiteral("SP9ABC")),
                 qPrintable(rechts));
    }

    void wasAnMichGehtStehtRechtsEgalWoEsLiegt()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);

        // 1300 Hz weg von der eigenen Stelle -- aber an mich.
        speise(applet, QStringLiteral("OE5SOS DL1ABC -07"), 200.0f);

        const QString rechts = applet.listRxForTest()->toPlainText();
        QVERIFY2(rechts.contains(QStringLiteral("OE5SOS DL1ABC")),
                 qPrintable(rechts));

        // Und die Gegenprobe am Wortrand: OE5SOSX ist eine ANDERE
        // Station. Sie darf die rechte Spalte nicht erreichen.
        speise(applet, QStringLiteral("OE5SOSX DL1ABC -07"), 210.0f);
        const QString rechts2 = applet.listRxForTest()->toPlainText();
        QVERIFY2(!rechts2.contains(QStringLiteral("OE5SOSX")),
                 qPrintable(rechts2));
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
                             + QStringLiteral("/longpath-zwei-spalten.png");
        QVERIFY(applet.grab().save(pfad));
        qInfo() << "Blatt:" << pfad;
    }

    void dieStelleWandertMit()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);
        speise(applet, QStringLiteral("CQ DL1ABC JN49"), 2000.0f);
        QVERIFY(!applet.listRxForTest()->toPlainText()
                     .contains(QStringLiteral("DL1ABC")));

        // Stelle auf 2000 ziehen: der NAECHSTE Fund dort gehoert
        // rechts hin. (Das Vergangene wird nicht umsortiert -- eine
        // Liste ist ein Protokoll, keine Ansicht.)
        applet.sendeTonFeldForTest()->setValue(2000);
        speise(applet, QStringLiteral("CQ HB9XYZ JN47"), 2000.0f);
        QVERIFY2(applet.listRxForTest()->toPlainText()
                     .contains(QStringLiteral("HB9XYZ")),
                 qPrintable(applet.listRxForTest()->toPlainText()));
    }
};

QTEST_MAIN(TstFt8ZweiSpalten)
#include "tst_ft8_zwei_spalten.moc"
