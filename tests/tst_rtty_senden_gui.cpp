// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Weg vom Knopf zum Ton — RTTY.
//
// tst_rtty_senden.cpp prueft den Sender selbst: ASCII hinein, FSK
// hinaus, als Rundlauf gegen den eigenen Dekoder. Dieser Lauf hier
// prueft die Strecke davor, die bis 2026-10-16 fehlte: dass ein Druck
// auf „Senden" den Text ueberhaupt bis zu dieser Quelle bringt, den
// Mikrofonweg umlegt, tastet — und hinterher alles wieder zurueckholt.
//
// ── Warum sendEvent und nicht QTest::mouseClick ─────────────────────
//
// Ohne Schirm verpuffen QTest-Mausklicks. Derselbe Grund wie in
// tst_ft8_senden.cpp, wo er einmal eine Stunde gekostet hat.
//
// ── Was hier NICHT geprueft wird ────────────────────────────────────
//
// Ob auf dem Band etwas ankommt. Das entscheidet der Sender, und der
// hat seinen eigenen Rundlauf.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QLabel>
#include <QLineEdit>
#include <QDir>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>

#include "core/MoxController.h"
#include "core/RttyTx.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/safety/BandPlanGuard.h"
#include "gui/applets/RttyDecoderApplet.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstRttySendenGui : public QObject
{
    Q_OBJECT

private:
    /// Ein Druck auf einen Knopf, per Ereignis statt per QTest — ohne
    /// Schirm kommt sonst nichts an.
    static void druecke(QPushButton* knopf)
    {
        QVERIFY(knopf);
        QVERIFY2(knopf->isEnabled(),
                 "ein Knopf, der aus ist, laesst sich nicht druecken — "
                 "das waere ein Befund, kein Versehen");
        const QPointF mitte(knopf->width() / 2.0, knopf->height() / 2.0);
        QMouseEvent runter(QEvent::MouseButtonPress, mitte,
                           knopf->mapToGlobal(mitte), Qt::LeftButton,
                           Qt::LeftButton, Qt::NoModifier);
        QMouseEvent rauf(QEvent::MouseButtonRelease, mitte,
                         knopf->mapToGlobal(mitte), Qt::LeftButton,
                         Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(knopf, &runter);
        QCoreApplication::sendEvent(knopf, &rauf);
    }

private slots:

    // ── Der Knopf ist ueberhaupt da ─────────────────────────────────
    //
    // Klingt nach nichts. Ist aber genau der Befund, um den es hier
    // geht: der Sender lag seit PR #257 fertig im Baum und war von
    // der Oberflaeche aus nicht erreichbar.
    void dieBedienungIstErreichbar()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        QVERIFY2(applet.textFeldForTest(), "kein Eingabefeld");
        QVERIFY2(applet.sendButtonForTest(), "kein Sendeknopf");
        QVERIFY2(applet.stopButtonForTest(), "kein Stopknopf");
        QVERIFY2(applet.sendButtonForTest()->isEnabled(),
                 "der Sendeknopf muss von Anfang an gehen");
        QVERIFY2(!applet.stopButtonForTest()->isEnabled(),
                 "Stop darf erst gehen, wenn etwas laeuft");
        QVERIFY2(!applet.sendetGerade(),
                 "frisch gebaut wird nicht gesendet");
    }

    // ── Ein Druck legt den Text auf die Quelle und tastet ───────────

    void einDruckBringtDenTextZurQuelleUndTastet()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        auto* quelle = radio.rttyTxSource();
        QVERIFY2(quelle, "ohne Sendequelle hat dieser Lauf keinen Gegenstand");
        auto* router = radio.micRouter();
        QVERIFY(router);
        auto* mox = radio.moxController();
        QVERIFY(mox);

        // Vorher: nichts vorbereitet, Weg steht nicht auf RTTY.
        QCOMPARE(quelle->proben(), 0);
        QVERIFY(router->activeSource() != MicSource::Rtty);

        applet.textFeldForTest()->setText(QStringLiteral("CQ DE OE5SOS"));
        druecke(applet.sendButtonForTest());

        QVERIFY2(applet.sendetGerade(), "nach dem Druck muss es laufen");
        QCOMPARE(quelle->text(), QStringLiteral("CQ DE OE5SOS"));
        QVERIFY2(quelle->proben() > 0,
                 "der Text ist nicht auf Toene gelegt worden");
        QVERIFY2(quelle->laeuft(), "die Quelle laeuft nicht");
        QCOMPARE(router->activeSource(), MicSource::Rtty);
        QVERIFY2(mox->isMox(), "es wird nicht getastet");

        // Waehrend des Sendens ist der Sendeknopf zu und Stop offen.
        QVERIFY(!applet.sendButtonForTest()->isEnabled());
        QVERIFY(applet.stopButtonForTest()->isEnabled());

        // Und am Ende kommt alles zurueck.
        applet.sendeEndeForTest();
        QVERIFY2(!applet.sendetGerade(), "nach dem Ende laeuft nichts mehr");
        QVERIFY2(!mox->isMox(), "der Sender bleibt getastet");
        QVERIFY2(router->activeSource() != MicSource::Rtty,
                 "der Mikrofonweg bleibt auf RTTY stehen — damit waere "
                 "das Mikrofon nach dem Senden abgeklemmt");
        QVERIFY(applet.sendButtonForTest()->isEnabled());
        QVERIFY(!applet.stopButtonForTest()->isEnabled());
    }

    // ── Der Mikrofonweg kommt auf den Stand von vorher zurueck ──────
    //
    // Nicht auf „irgendeinen" — auf den, der vorher eingestellt war.
    void derMikrofonwegKommtGenauZurueck()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        auto* router = radio.micRouter();
        QVERIFY(router);
        router->setActiveSource(MicSource::Vax);
        QCOMPARE(router->activeSource(), MicSource::Vax);

        applet.textFeldForTest()->setText(QStringLiteral("TEST"));
        druecke(applet.sendButtonForTest());
        QCOMPARE(router->activeSource(), MicSource::Rtty);

        applet.sendeEndeForTest();
        QCOMPARE(router->activeSource(), MicSource::Vax);
    }

    // ── Stop bricht ab ──────────────────────────────────────────────

    void stopHoertSofortAuf()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        applet.textFeldForTest()->setText(QStringLiteral("CQ CQ"));
        druecke(applet.sendButtonForTest());
        QVERIFY(applet.sendetGerade());
        QVERIFY(radio.rttyTxSource()->laeuft());

        druecke(applet.stopButtonForTest());

        QVERIFY2(!applet.sendetGerade(), "Stop hat nicht gewirkt");
        QVERIFY2(!radio.rttyTxSource()->laeuft(),
                 "die Quelle laeuft weiter, obwohl Stop gedrueckt wurde");
        QVERIFY2(!radio.moxController()->isMox(),
                 "der Sender bleibt getastet, obwohl Stop gedrueckt wurde");
    }

    // ── Sagt der Bandplan nein, geht nichts hinaus ──────────────────
    //
    // Der teure Fall. Bei FT8 lief der Ton vor der Behebung das ganze
    // Fenster weiter, waehrend nichts hinausging und „sendet" dastand.
    // Hier darf dasselbe nicht noch einmal entstehen.
    void einAbgelehntesTastenLaesstNichtsLaufen()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        auto* mox = radio.moxController();
        QVERIFY(mox);
        int gefragt = 0;
        mox->setMoxCheck([&gefragt]() {
            ++gefragt;
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("out of band")};
        });

        applet.textFeldForTest()->setText(QStringLiteral("CQ DE OE5SOS"));
        druecke(applet.sendButtonForTest());

        QCOMPARE(gefragt, 1);
        QVERIFY2(!applet.sendetGerade(),
                 "nach einer Ablehnung darf nichts weiterlaufen");
        QVERIFY2(!radio.rttyTxSource()->laeuft(),
                 "der Ton laeuft weiter, obwohl der Bandplan nein sagt");
        QVERIFY2(!mox->isMox(), "getastet wurde trotz Ablehnung");
        QVERIFY2(radio.micRouter()->activeSource() != MicSource::Rtty,
                 "der Mikrofonweg blieb auf RTTY — genau der Zustand, in "
                 "dem das Mikrofon abgeklemmt ist und nichts hinausgeht");
        QVERIFY2(applet.sendeStatusForTest()->text().contains(
                     QStringLiteral("out of band")),
                 qPrintable(QStringLiteral("die Zeile nennt den Grund nicht: ")
                            + applet.sendeStatusForTest()->text()));
        QVERIFY(applet.sendButtonForTest()->isEnabled());
    }

    // ── Leerer Text geht nicht auf Sendung ──────────────────────────

    void leererTextTastetNicht()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        applet.textFeldForTest()->setText(QStringLiteral("   "));
        druecke(applet.sendButtonForTest());

        QVERIFY2(!applet.sendetGerade(), "Leerraum ist kein Text");
        QVERIFY2(!radio.moxController()->isMox(),
                 "fuer nichts wurde getastet");
    }

    // ── Zwei Druecke hintereinander ueberholen sich nicht ───────────

    void einZweiterDruckWaehrendDesSendensWirdNichtAngenommen()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        applet.textFeldForTest()->setText(QStringLiteral("ERSTER"));
        druecke(applet.sendButtonForTest());
        QCOMPARE(radio.rttyTxSource()->text(), QStringLiteral("ERSTER"));

        // Der Knopf ist jetzt aus; der Aufruf direkt darf trotzdem
        // nichts umwerfen — ein Fenster kann ihn ueber die Eingabetaste
        // erreichen.
        applet.sendeText(QStringLiteral("ZWEITER"));
        QCOMPARE(radio.rttyTxSource()->text(), QStringLiteral("ERSTER"));
    }

    // ── Wie es aussieht ─────────────────────────────────────────────
    //
    // Nicht nur „die Zeiger stimmen", sondern: die Zeile ist wirklich
    // da und hat eine Groesse. Das Bild wird abgelegt, damit man die
    // Behauptung nachsehen kann, statt sie zu glauben.
    //
    // Offscreen und im Sandkasten des Prueflaufs — die echte App
    // anzuwerfen hat schon einmal Martins Fenstergeometrie
    // ueberschrieben.
    void dieSendezeileIstSichtbar()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);
        applet.resize(420, 340);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 5000));

        QVERIFY2(applet.textFeldForTest()->isVisible(),
                 "das Eingabefeld ist nicht zu sehen");
        QVERIFY2(applet.sendButtonForTest()->isVisible(),
                 "der Sendeknopf ist nicht zu sehen");
        QVERIFY2(applet.sendButtonForTest()->width() > 0
                     && applet.sendButtonForTest()->height() > 0,
                 "der Sendeknopf hat keine Flaeche");

        const QPixmap bild = applet.grab();
        QVERIFY(!bild.isNull());
        const QString ziel = QDir::temp().filePath(
            QStringLiteral("rtty-sendezeile.png"));
        QVERIFY2(bild.save(ziel), qPrintable(ziel));
        qInfo() << "Bild abgelegt:" << ziel
                << bild.width() << "x" << bild.height();
    }

    // ── Ohne Sendeweg wird es gesagt, nicht verschwiegen ────────────

    void ohneSendewegSagtEsDieZeile()
    {
        RadioModel radio;   // ohne installRttyTxForTest()
        RttyDecoderApplet applet(&radio);

        applet.textFeldForTest()->setText(QStringLiteral("CQ"));
        druecke(applet.sendButtonForTest());

        QVERIFY(!applet.sendetGerade());
        QVERIFY2(!applet.sendeStatusForTest()->text().isEmpty(),
                 "stillschweigend nichts tun ist die schlechteste Antwort");
    }
};

QTEST_MAIN(TstRttySendenGui)
#include "tst_rtty_senden_gui.moc"
