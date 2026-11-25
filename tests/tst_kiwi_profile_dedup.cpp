// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ein Endpunkt, ein Profil -- auch beim Laden alter Staende (2026-11-25):
// drei "bern.proxy.kiwis" belegten gleichzeitig drei Kanaele bei einem
// Betreiber, der drei hat.
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/AppSettings.h"
#include "core/KiwiSdrManager.h"

using namespace Longpath;

namespace {
class NullStore final : public IKiwiSdrCredentialStore {
public:
    bool isPersistent() const override { return false; }
    void read(const QString&, QObject*, Callback) override {}
    void write(const QString&, const QString&, QObject*, Callback) override {}
    void remove(const QString&, QObject*, Callback) override {}
};

QJsonObject prof(const char* id, const char* name, const char* ep, bool preview, bool autoc)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), QString::fromLatin1(id));
    o.insert(QStringLiteral("name"), QString::fromLatin1(name));
    o.insert(QStringLiteral("endpoint"), QString::fromLatin1(ep));
    o.insert(QStringLiteral("waterfallPreviewEnabled"), preview);
    o.insert(QStringLiteral("autoConnect"), autoc);
    return o;
}
}

class TstKiwiProfileDedup : public QObject
{
    Q_OBJECT
private slots:
    void doppelteWerdenZusammengefuehrt()
    {
        QJsonArray a;
        a.append(prof("1", "Bern", "bern.proxy.kiwisdr.com:8073", false, false));
        a.append(prof("2", "Bern", "bern.proxy.kiwisdr.com:8073", true, false));
        a.append(prof("3", "Bern", "bern.proxy.kiwisdr.com:8073", false, true));
        a.append(prof("4", "Linz", "linz.example:8073", false, false));
        QJsonObject root;
        root.insert(QStringLiteral("profiles"), a);
        AppSettings::instance().setValue(
            QStringLiteral("KiwiSdrRxAntennas"),
            QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

        KiwiSdrManager mgr(nullptr, std::make_shared<NullStore>());
        QCOMPARE(mgr.profiles().size(), 2);
        const KiwiSdrAntennaProfile bern = mgr.profile(QStringLiteral("1"));
        QCOMPARE(bern.endpoint, QStringLiteral("bern.proxy.kiwisdr.com:8073"));
        // Die Wuensche der Doppelten gehen auf das erste ueber.
        QVERIFY(bern.waterfallPreviewEnabled);
        QVERIFY(bern.autoConnect);
        QVERIFY(!mgr.hasProfile(QStringLiteral("2")));
        QVERIFY(mgr.hasProfile(QStringLiteral("4")));
    }
};

QTEST_MAIN(TstKiwiProfileDedup)
#include "tst_kiwi_profile_dedup.moc"
