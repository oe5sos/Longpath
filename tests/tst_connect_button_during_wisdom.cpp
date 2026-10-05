// no-port-check: Longpath-original. Thetis baut seine FFTW-Plaene
// synchron vor dem ersten Connect (radio.cs:140-158 [v2.10.3.13]) und
// kennt diesen Zustand deshalb gar nicht; es gibt nichts zu portieren.
// =================================================================
// tests/tst_connect_button_during_wisdom.cpp  (Longpath)
// =================================================================
//
// Schwester von tst_connect_during_wisdom. Dort wird
// RadioModel::connectToRadio() direkt gerufen; hier wird der Knopf
// gedrueckt, den der Betreiber drueckt.
//
// Der Unterschied ist nicht akademisch: der gemeldete Befund vom
// 2026-10-05 ("Danach bleibt der Zustand auf Disconnected stehen ...
// der Nutzer sieht nur, dass nichts passiert") hat zwei Haelften. Die
// eine liegt im Modell (der Wunsch wurde beim Abwickeln der
// geschachtelten Warteschleife doppelt gefahren) — die deckt der
// Schwester-Prueffall ab. Die andere liegt hier: die Statusleiste des
// Verbindungsfelds kannte nur Connected und "alles andere", und "alles
// andere" las sich als "Disconnected". Genau waehrend der 15 Minuten,
// in denen etwas passiert.
//
// Warum ein unbekanntes Protokoll: RadioConnection::create() liefert
// dafuer nullptr, und connectToRadio() steigt dort aus — nach der
// gesamten Vorbereitung, aber bevor ein Socket aufgeht. Der Prueffall
// braucht damit kein Netz und kein Geraet.
//
// Warum sendEvent statt QTest::mouseClick: ohne Schirm verpuffen
// QTest-Mausklicks (Hausregel, 2026-09-30). Der Knopf bekommt Press und
// Release direkt zugestellt.
// =================================================================

#include <QtTest/QtTest>

#include <chrono>
#include <QApplication>
#include <QHostAddress>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QLayout>
#include <QTableWidget>
#include <QTimer>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "gui/ConnectionPanel.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Unbekanntes Protokoll -> RadioConnection::create() gibt nullptr, der
// Rumpf steigt vor dem ersten Socket aus. Alles davor laeuft echt.
constexpr auto kNoSuchProtocol = static_cast<ProtocolVersion>(99);

RadioInfo makeInfo()
{
    RadioInfo info;
    info.name = QStringLiteral("Pruefgeraet");
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.address = QHostAddress(QStringLiteral("127.0.0.1"));
    info.port = 1024;
    info.protocol = kNoSuchProtocol;
    return info;
}

// Den Knopf wirklich druecken. QAbstractButton::click() wuerde den
// Bedienweg ueberspringen (kein Hit-Test, kein Druck/Loslass-Paar);
// hier soll genau der gefahren werden.
void pressButton(QPushButton* btn)
{
    const QPoint c = btn->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress, c, btn->mapToGlobal(c),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, c, btn->mapToGlobal(c),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(btn, &press);
    QApplication::sendEvent(btn, &release);
}

// Exakt, nicht contains(): "Connect" steckt in "Disconnect". Der erste
// Entwurf dieses Prueffalls hat deshalb auf TRENNEN gedrueckt und war
// trotzdem gruen, weil der Zustand danach ebenso "nichts passiert" war.
QPushButton* buttonWithText(QWidget* root, const QString& text)
{
    const auto buttons = root->findChildren<QPushButton*>();
    for (QPushButton* b : buttons) {
        if (b->text().compare(text, Qt::CaseInsensitive) == 0) {
            return b;
        }
    }
    return nullptr;
}

QString stripText(QWidget* root)
{
    QWidget* strip = root->findChild<QWidget*>(QStringLiteral("statusStrip"));
    if (!strip) {
        return {};
    }
    QString all;
    const auto labels = strip->findChildren<QLabel*>();
    for (QLabel* l : labels) {
        if (l->isVisible() || !l->text().isEmpty()) {
            all += l->text() + QLatin1Char('\n');
        }
    }
    return all;
}

} // namespace

class TstConnectButtonDuringWisdom : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Ein gespeichertes Geraet, damit das Verbindungsfeld eine Zeile
        // hat, ohne dass eine Rundsendung ein echtes Geraet finden muss
        // (der Konstruktor saet die Tabelle aus den Einstellungen).
        AppSettings& s = AppSettings::instance();
        s.saveRadio(makeInfo(), /*pinToMac=*/false);
        s.save();
    }

    void derKnopfWaehrendDerErzeugung()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);

        // KEINE Rundsendung aus einem Prueflauf. Der erste Entwurf dieses
        // Prueffalls liess den Konstruktor des Verbindungsfelds scannen —
        // und fand prompt das echte Geraet des Betreibers im Shack, das
        // dann als zweite Zeile in der Tabelle stand. Ein Klick auf die
        // falsche Zeile haette eine echte Verbindung aufgebaut.
        // holdOffScans() verschiebt (verwirft nicht) jeden Scan weit ueber
        // die Lebensdauer dieses Prozesses hinaus.
        model.discovery()->holdOffScans(std::chrono::milliseconds(10 * 60 * 1000));

        ConnectionPanel panel(&model);

        // Geometrien setzen, ohne das Fenster zu zeigen (Hausregel:
        // Pruefprogramme nur offscreen). Ohne das haben die Kindwidgets
        // ein leeres rect(), QAbstractButton::hitButton() schlaegt fehl
        // und der zugestellte Klick verpufft lautlos — genau so ist der
        // erste Lauf dieses Prueffalls gruen durchgerutscht, ohne je einen
        // Knopf gedrueckt zu haben.
        panel.ensurePolished();
        panel.resize(panel.sizeHint());
        if (panel.layout()) {
            panel.layout()->activate();
        }

        auto* table = panel.findChild<QTableWidget*>();
        QVERIFY(table);

        // Die Zeile NUR ueber die MAC des Pruefgeraets waehlen, nie ueber
        // den Index: in der Tabelle kann ein echtes Geraet stehen.
        int row = -1;
        for (int r = 0; r < table->rowCount(); ++r) {
            for (int c = 0; c < table->columnCount(); ++c) {
                QTableWidgetItem* it = table->item(r, c);
                if (it && it->text().contains(QStringLiteral("Pruefgeraet"))) {
                    row = r;
                    break;
                }
            }
            if (row >= 0) {
                break;
            }
        }
        QVERIFY2(row >= 0, "Die Zeile des Pruefgeraets steht nicht in der Liste");
        table->setCurrentCell(row, 0);

        QPushButton* connectBtn = buttonWithText(&panel, QStringLiteral("Connect"));
        QVERIFY2(connectBtn, "Connect-Knopf nicht gefunden");

        // infoChanged faellt oben im Rumpf von connectToRadio() — ein
        // Zaehler fuer Rumpfdurchlaeufe.
        QSignalSpy bodyRuns(&model, &RadioModel::infoChanged);

        // Frisches Profil: die FFTW-Tabellen werden gerade gebaut.
        engine->beginWisdomForTests();

        QCOMPARE(connectBtn->text(), QStringLiteral("Connect"));
        QVERIFY2(!connectBtn->rect().isEmpty(),
                 "Der Knopf hat keine Geometrie — ein zugestellter Klick "
                 "wuerde an hitButton() verpuffen");
        QVERIFY2(connectBtn->isEnabled(),
                 "Der Connect-Knopf ist gesperrt — die Zeilenauswahl hat "
                 "nicht gegriffen, der Prueffall wuerde ins Leere druecken");

        QElapsedTimer t;
        t.start();
        pressButton(connectBtn);
        const qint64 blockedMs = t.elapsed();

        QVERIFY2(model.isWaitingForWdspWisdom(),
                 "Der Klick hat keinen Verbindungswunsch ausgeloest");

        const QString duringWisdom = stripText(&panel);
        qInfo("Waehrend der Erzeugung: Knopf hielt %lld ms, Durchlaeufe %d, "
              "Leiste: %s",
              blockedMs, int(bodyRuns.count()),
              qPrintable(QString(duringWisdom).replace(QLatin1Char('\n'),
                                                       QLatin1Char('|'))));

        // 1. Der Klick darf den Hauptfaden nicht festhalten.
        QVERIFY2(blockedMs < 250,
                 qPrintable(QStringLiteral(
                     "Der Connect-Klick hielt die Oberflaeche %1 ms fest; in "
                     "echt waere das die volle Wisdom-Laufzeit (~15 min).")
                     .arg(blockedMs)));

        // 2. Noch ist nichts gefahren.
        QCOMPARE(bodyRuns.count(), 0);

        // 3. Und die Leiste sagt, was los ist — nicht "Disconnected".
        QVERIFY2(!duringWisdom.contains(QStringLiteral("Disconnected")),
                 qPrintable(QStringLiteral(
                     "Die Statusleiste sagt waehrend der Erzeugung "
                     "\"Disconnected\": %1").arg(duringWisdom)));
        QVERIFY2(duringWisdom.contains(QStringLiteral("FFT"))
                     || duringWisdom.contains(QStringLiteral("Preparing")),
                 qPrintable(QStringLiteral(
                     "Die Statusleiste nennt den Grund der Wartezeit nicht: %1")
                     .arg(duringWisdom)));
        QVERIFY(model.isWaitingForWdspWisdom());
        QCOMPARE(model.connectionState(), ConnectionState::Connecting);

        // ── Ende der Erzeugung: der Wunsch wird nachgeholt ─────────────
        engine->completeWisdomForTests();
        QTRY_VERIFY_WITH_TIMEOUT(bodyRuns.count() > 0, 3000);
        QTest::qWait(300);   // einem zweiten Durchlauf Zeit geben

        QCOMPARE(bodyRuns.count(), 1);
        QVERIFY(!model.isWaitingForWdspWisdom());
    }
};

QTEST_MAIN(TstConnectButtonDuringWisdom)
#include "tst_connect_button_during_wisdom.moc"
