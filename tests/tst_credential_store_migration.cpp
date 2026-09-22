// =================================================================
// tests/tst_credential_store_migration.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. Der Schluesselbund-Dienstname heisst seit dem
// 2026-09-21 "Longpath: <key>"; ein Eintrag unter dem alten Namen
// "NereusSDR: <key>" wird beim ersten retrieve() umgezogen (unter dem
// neuen Namen abgelegt, der alte geloescht) -- die Bedingung, die im
// CredentialStore seit dem 2026-08-23 fuer die Umbenennung stand.
//
// Beruehrt den ECHTEN Schluesselbund des Benutzers (security CLI); darum
// nur mit LONGPATH_KEYCHAIN_LIVE=1, sonst uebersprungen. Der Eintrag
// heisst eindeutig und wird am Ende in beiden Namen geloescht.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QProcess>

#include "core/CredentialStore.h"

using namespace Longpath;

namespace {
bool security(const QStringList& args, QString* out = nullptr)
{
    QProcess p;
    p.start(QStringLiteral("/usr/bin/security"), args);
    if (!p.waitForFinished(10000)) { p.kill(); return false; }
    if (out) { *out = QString::fromUtf8(p.readAllStandardOutput()).trimmed(); }
    return p.exitCode() == 0;
}
} // namespace

class TstCredentialStoreMigration : public QObject {
    Q_OBJECT
private slots:
    void anOldEntryMovesToTheNewServiceName()
    {
#if !defined(Q_OS_MACOS)
        QSKIP("Schluesselbund: nur macOS");
#else
        if (qEnvironmentVariable("LONGPATH_KEYCHAIN_LIVE") != QStringLiteral("1")) {
            QSKIP("beruehrt den echten Schluesselbund -- LONGPATH_KEYCHAIN_LIVE=1 setzen");
        }
        const QString key = QStringLiteral("lp-migration-test");
        const QString account = QStringLiteral("tester");
        const QString oldService = QStringLiteral("NereusSDR: %1").arg(key);
        const QString newService = QStringLiteral("Longpath: %1").arg(key);
        // Aufraeumen, falls ein frueherer Lauf abbrach.
        security({QStringLiteral("delete-generic-password"), QStringLiteral("-s"), oldService, QStringLiteral("-a"), account});
        security({QStringLiteral("delete-generic-password"), QStringLiteral("-s"), newService, QStringLiteral("-a"), account});

        // Ein Eintrag, wie ihn 0.6.3 hinterlassen hat.
        QVERIFY(security({QStringLiteral("add-generic-password"), QStringLiteral("-U"),
                          QStringLiteral("-s"), oldService, QStringLiteral("-a"), account,
                          QStringLiteral("-w"), QStringLiteral("geheim-alt")}));

        // Der erste Zugriff liest ihn -- und zieht ihn um.
        QCOMPARE(CredentialStore::retrieve(key, account), QStringLiteral("geheim-alt"));
        QString got;
        QVERIFY2(security({QStringLiteral("find-generic-password"), QStringLiteral("-s"), newService,
                           QStringLiteral("-a"), account, QStringLiteral("-w")}, &got),
                 "nach retrieve() muss der Eintrag unter dem neuen Dienstnamen liegen");
        QCOMPARE(got, QStringLiteral("geheim-alt"));
        QVERIFY2(!security({QStringLiteral("find-generic-password"), QStringLiteral("-s"), oldService,
                            QStringLiteral("-a"), account, QStringLiteral("-w")}),
                 "der alte Eintrag muss weg sein");

        // Zweiter Zugriff: kommt jetzt direkt vom neuen Namen.
        QCOMPARE(CredentialStore::retrieve(key, account), QStringLiteral("geheim-alt"));

        // erase() raeumt beide Namen.
        QVERIFY(CredentialStore::erase(key, account));
        QVERIFY(!security({QStringLiteral("find-generic-password"), QStringLiteral("-s"), newService,
                           QStringLiteral("-a"), account, QStringLiteral("-w")}));
#endif
    }
};

QTEST_MAIN(TstCredentialStoreMigration)
#include "tst_credential_store_migration.moc"
