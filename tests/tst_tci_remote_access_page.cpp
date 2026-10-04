// no-port-check: Longpath-eigen. Die Fernzugriffs-Gruppe auf Setup → CAT &
// Network → TCI Server hat kein Thetis-Vorbild — Thetis kennt weder Token noch
// Sendefreigabe, weil sein TCI-Server nur auf Loopback lauscht.
//
// Geprüft wird das, woran so eine Seite wirklich scheitert: dass sie sich
// bauen lässt, dass die Bedienelemente da sind, und dass der Hinweistext sagt,
// was gerade gilt. Eine Sicherheitsgruppe, die schweigt, wenn sie wirkungslos
// ist, verleitet dazu, sich auf sie zu verlassen.
//
// Der Schlüsselbund wird hier nicht angefasst: CredentialStore nimmt im
// Testbetrieb (QStandardPaths::isTestModeEnabled) den Speicher-Tresor statt
// des Anmeldebunds — siehe CredentialStore.cpp, useSessionVaultOnly().

#include <QtTest>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

#include "core/AppSettings.h"
#include "core/CredentialStore.h"
#include "core/TciServer.h"
#include "gui/setup/CatNetworkSetupPages.h"

using namespace Longpath;

class TestTciRemoteAccessPage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Wenn das hier fehlschlägt, schriebe jeder folgende Prüfpunkt in den
        // echten Anmeldebund des Betreibers. Lieber hier laut scheitern.
        QVERIFY2(!CredentialStore::isPersistent(),
                 "Im Testbetrieb darf der Schlüsselbund nicht benutzt werden");
    }

    void cleanup() {
        TciServer::setRemoteToken(QString());
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteTx"),
                                         QStringLiteral("False"));
    }

    void seite_traegt_die_bedienelemente() {
        CatTciServerPage page;
        QVERIFY(page.findChild<QLineEdit*>() != nullptr);

        // Der Token darf nur erzeugt, nie getippt werden — sonst landet ein
        // schwaches Wunschtoken im Schlüsselbund.
        bool sahSchreibgeschuetztesFeld = false;
        for (auto* e : page.findChildren<QLineEdit*>()) {
            if (e->isReadOnly()) { sahSchreibgeschuetztesFeld = true; }
        }
        QVERIFY2(sahSchreibgeschuetztesFeld,
                 "Das Tokenfeld muss schreibgeschützt sein");

        bool sahSendefreigabe = false;
        for (auto* c : page.findChildren<QCheckBox*>()) {
            if (c->text().contains(QStringLiteral("transmit"), Qt::CaseInsensitive)) {
                sahSendefreigabe = true;
                QVERIFY2(!c->isChecked(), "Senden aus dem Netz muss ab Werk aus sein");
            }
        }
        QVERIFY2(sahSendefreigabe, "Der Sendeschalter muss auf der Seite sein");
    }

    void hinweis_sagt_dass_loopback_nichts_bewirkt() {
        AppSettings::instance().setValue(QStringLiteral("TciServerBindAddress"),
                                         QStringLiteral("127.0.0.1"));
        CatTciServerPage page;

        // Irgendein Label muss das Wort loopback tragen — sonst erzeugt jemand
        // ein Token und wundert sich, dass sein Telefon trotzdem nicht
        // hereinkommt.
        bool sahHinweis = false;
        for (auto* l : page.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("loopback"), Qt::CaseInsensitive)) {
                sahHinweis = true;
            }
        }
        QVERIFY2(sahHinweis,
                 "Bei Loopback-Bindung muss dastehen, dass die Gruppe nichts bewirkt");
    }

    void hinweis_warnt_wenn_offen_aber_ohne_token() {
        AppSettings::instance().setValue(QStringLiteral("TciServerBindAddress"),
                                         QStringLiteral("0.0.0.0"));
        TciServer::setRemoteToken(QString());
        CatTciServerPage page;

        bool sahWarnung = false;
        for (auto* l : page.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("no token"), Qt::CaseInsensitive)) {
                sahWarnung = true;
            }
        }
        QVERIFY2(sahWarnung,
                 "Ohne Token muss dastehen, dass jede Verbindung von aussen "
                 "abgewiesen wird");
    }

    void blatt_zeichnen() {
        // Kein Prüfpunkt im engeren Sinn, sondern der Beleg zum Ansehen: die
        // Seite als PNG, damit die Gestaltung beurteilt werden kann, ohne die
        // ganze Anwendung zu starten. Dasselbe Mittel wie bei den RX-Profilen
        // und dem Logbuch.
        // Der scharfe Zustand: ins Netz gebunden, Token gesetzt. Eine
        // erfundene LAN-Adresse faellt in der Auswahl auf Loopback zurueck
        // (sie existiert auf dem Pruefrechner nicht) — 0.0.0.0 steht dort
        // immer und zeigt darum den Fall, um den es geht.
        AppSettings::instance().setValue(QStringLiteral("TciServerBindAddress"),
                                         QStringLiteral("0.0.0.0"));
        AppSettings::instance().setValue(QStringLiteral("TciServerEnabled"),
                                         QStringLiteral("True"));
        TciServer::setRemoteToken(QStringLiteral("K7M2PQXR4TWH9NBJ63FDYAVC58EG"));

        CatTciServerPage page;
        page.resize(560, 900);

        const QString ziel = qEnvironmentVariable("LONGPATH_BLATT_PNG");
        if (ziel.isEmpty()) { QSKIP("LONGPATH_BLATT_PNG nicht gesetzt"); }
        QPixmap bild = page.grab();
        QVERIFY(bild.save(ziel));
        qInfo() << "Blatt gezeichnet:" << ziel << bild.size();
    }
};

QTEST_MAIN(TestTciRemoteAccessPage)
#include "tst_tci_remote_access_page.moc"
