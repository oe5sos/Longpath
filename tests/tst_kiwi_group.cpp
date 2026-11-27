// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wo werde ich gehoert (Entwurf B + C, 2026-11-25): die Mithoerer-Gruppe
// und ihre Kacheln. Wie bei allen Kiwi-Pruefungen wird gelesen, was am
// Bildschirm STEHT (Beschriftungen, Tabellenzellen, Knopfzustaende).

#include <QtTest>
#include <QPushButton>
#include <QTableWidget>

#include "core/KiwiGroup.h"
#include "gui/KiwiHearingDialog.h"

using namespace Longpath;

namespace {

constexpr double kLo = 14185000.0;
constexpr double kHi = 14225000.0;
constexpr double kMark = 14205000.0;

QVector<float> row(double bumpDb, int seed)
{
    QVector<float> v(1024);
    const double binHz = (kHi - kLo) / 1024;
    for (int i = 0; i < 1024; ++i) {
        double d = -110.0 + ((i * 37 + seed * 11) % 7 - 3) * 0.4;
        const double f = kLo + (i + 0.5) * binHz;
        if (bumpDb > 0.0 && std::abs(f - kMark) <= 1200.0) { d += bumpDb; }
        v[i] = float(d);
    }
    return v;
}

// Eine ganze Sendung (1000..6000 ms) mit dem Signal ab 2400 ms.
void sendung(KiwiGroup& g, const QString& id, double bumpDb)
{
    int seed = 0;
    for (qint64 t = 0; t <= 900; t += 100) { g.feedRow(id, row(0, seed++), kLo, kHi, t); }
    for (qint64 t = 1100; t <= 6000; t += 100) {
        g.feedRow(id, row(t >= 2400 ? bumpDb : 0.0, seed++), kLo, kHi, t);
    }
}

KiwiGroupMember member(const char* id, double km)
{
    KiwiGroupMember m;
    m.id = QString::fromLatin1(id);
    m.name = m.id;
    m.km = km;
    return m;
}

} // namespace

class TstKiwiGroup : public QObject
{
    Q_OBJECT
private slots:
    void hoechstensFuenfInsgesamt()
    {
        KiwiGroup g;
        QCOMPARE(g.capacity(), 5);
        g.setCapacity(4);     // ein Kiwi von "Mein Signal" belegt einen Platz
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(g.add(member(qPrintable(QStringLiteral("k%1").arg(i)), 100.0)),
                     KiwiGroup::AddResult::Added);
        }
        QCOMPARE(g.add(member("k9", 1.0)), KiwiGroup::AddResult::Full);
        QCOMPARE(g.add(member("k0", 1.0)), KiwiGroup::AddResult::AlreadyThere);
        QCOMPARE(g.size(), 4);
        g.setCapacity(99);                       // nie ueber fuenf
        QCOMPARE(g.capacity(), 5);
        QVERIFY(g.remove(QStringLiteral("k2")));
        QVERIFY(!g.remove(QStringLiteral("k2")));
        QCOMPARE(g.size(), 3);
    }

    void jederKiwiHatSeinUrteil()
    {
        KiwiGroup g;
        g.add(member("nah", 60.0));
        g.add(member("knapp", 800.0));
        g.add(member("tot", 7800.0));
        g.setMarkHz(kMark);
        g.txChanged(true, 1000);
        sendung(g, QStringLiteral("nah"), 30.0);
        sendung(g, QStringLiteral("knapp"), 8.0);
        sendung(g, QStringLiteral("tot"), 0.0);
        g.txChanged(false, 6000);
        QVERIFY(!g.feedRow(QStringLiteral("fremd"), row(0, 1), kLo, kHi, 100));

        const auto nah = g.result(QStringLiteral("nah"));
        const auto knapp = g.result(QStringLiteral("knapp"));
        const auto tot = g.result(QStringLiteral("tot"));
        QCOMPARE(nah.verdict, KiwiHearMeMeter::Verdict::Hears);
        QVERIFY(!KiwiGroup::isWeak(nah));
        QCOMPARE(knapp.verdict, KiwiHearMeMeter::Verdict::Hears);
        QVERIFY(KiwiGroup::isWeak(knapp));
        QCOMPARE(tot.verdict, KiwiHearMeMeter::Verdict::Nothing);

        const auto s = KiwiGroup::summarize({nah, knapp, tot}, {60.0, 800.0, 7800.0});
        QCOMPARE(s.judged, 3);
        QCOMPARE(s.hears, 2);
        QCOMPARE(s.weak, 1);
        QCOMPARE(s.nothing, 1);
        QCOMPARE(s.farthestKm, 800.0);     // der tote bei 7800 km zaehlt nicht
    }

    void ohneBildKeinUrteil()
    {
        KiwiGroup g;
        g.add(member("a", 10.0));
        g.setMarkHz(kMark);
        g.txChanged(true, 1000);
        g.txChanged(false, 6000);        // gesendet, aber nie eine Zeile
        QCOMPARE(g.result(QStringLiteral("a")).verdict,
                 KiwiHearMeMeter::Verdict::Unknown);
        const auto s = KiwiGroup::summarize({g.result(QStringLiteral("a"))}, {10.0});
        QCOMPARE(s.judged, 0);           // Unbekanntes ist kein Befund
    }

    void kachelnZeigenDasUrteilUndBehaltenDenWasserfall()
    {
        KiwiHearingDialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg, 15000));

        KiwiGroup g;
        g.add(member("nah", 60.0));
        g.add(member("knapp", 800.0));
        g.add(member("tot", 7800.0));
        g.setMarkHz(kMark);
        g.txChanged(true, 1000);
        sendung(g, QStringLiteral("nah"), 30.0);
        sendung(g, QStringLiteral("knapp"), 8.0);
        sendung(g, QStringLiteral("tot"), 0.0);
        g.txChanged(false, 6000);

        QVector<KiwiHearingRow> rows;
        const struct { const char* id; double km; double lat; double lon; } pos[] = {
            {"nah", 60, 48.3, 14.3}, {"knapp", 800, 55.6, 13.0}, {"tot", 7800, 33.7, -84.4}};
        for (int i = 0; i < 3; ++i) {
            KiwiHearingRow r;
            r.id = QString::fromLatin1(pos[i].id);
            r.name = r.id;
            r.km = pos[i].km;
            r.lat = pos[i].lat; r.lon = pos[i].lon; r.hasPosition = true;
            r.connected = true;
            r.stateText = QStringLiteral("connected");
            r.result = g.result(r.id);
            r.primary = (i == 0);
            rows.append(r);
        }
        dlg.setSlotsFree(2);
        dlg.setRows(rows);
        QCOMPARE(dlg.tileCount(), 3);
        QVERIFY2(dlg.tileVerdictText(QStringLiteral("nah")).contains(QStringLiteral("S")),
                 qPrintable(dlg.tileVerdictText(QStringLiteral("nah"))));
        QVERIFY(dlg.tileVerdictText(QStringLiteral("nah")).startsWith(QStringLiteral("hears me")));
        QVERIFY(dlg.tileVerdictText(QStringLiteral("knapp")).startsWith(QStringLiteral("weak")));
        QCOMPARE(dlg.tileVerdictText(QStringLiteral("tot")), QStringLiteral("hears nothing"));
        QCOMPARE(dlg.summaryText(), QStringLiteral("2 of 3 hear me · farthest: 800 km"));
        QCOMPARE(dlg.tableForTest()->rowCount(), 3);
        QCOMPARE(dlg.tableForTest()->item(2, 3)->text(), QStringLiteral("hears nothing"));
        QCOMPARE(dlg.mapForTest()->pointCount(), 3);
        // Der Kiwi von "Mein Signal" hat keinen Schliessen-Knopf.
        QVERIFY(dlg.removeButtonForTest(QStringLiteral("nah"))->isHidden());
        QVERIFY(!dlg.removeButtonForTest(QStringLiteral("knapp"))->isHidden());
        QVERIFY(dlg.addButtonForTest()->isEnabled());

        // Wasserfall der Kachel ueberlebt das naechste Auffrischen.
        for (int i = 0; i < 5; ++i) {
            dlg.pushWaterfallRow(QStringLiteral("knapp"), row(8.0, i), kLo, kHi);
        }
        QCOMPARE(dlg.miniRowsForTest(QStringLiteral("knapp")), 5);
        dlg.setRows(rows);
        QCOMPARE(dlg.miniRowsForTest(QStringLiteral("knapp")), 5);
        QVERIFY(!dlg.waterfallImage(QStringLiteral("knapp")).isNull());

        // Wuensche kommen als Signale an.
        QSignalSpy rem(&dlg, &KiwiHearingDialog::removeRequested);
        QSignalSpy rec(&dlg, &KiwiHearingDialog::recordToggled);
        dlg.removeButtonForTest(QStringLiteral("tot"))->click();
        dlg.recordButtonForTest(QStringLiteral("knapp"))->click();
        QCOMPARE(rem.count(), 1);
        QCOMPARE(rem.at(0).at(0).toString(), QStringLiteral("tot"));
        QCOMPARE(rec.count(), 1);
        QCOMPARE(rec.at(0).at(1).toBool(), true);

        // Voll: der Plus-Knopf ist aus.
        dlg.setSlotsFree(0);
        QVERIFY(!dlg.addButtonForTest()->isEnabled());

        // Weggefallen -> Kachel weg.
        dlg.setRows({rows.first()});
        QTest::qWait(30);
        QCOMPARE(dlg.tileCount(), 1);

        const QString out = qEnvironmentVariable("LONGPATH_KIWI_HEARING_PNG");
        if (!out.isEmpty()) {
            dlg.setSlotsFree(2);
            dlg.setRows(rows);
            for (int i = 0; i < 40; ++i) {
                dlg.pushWaterfallRow(QStringLiteral("nah"), row(30.0, i), kLo, kHi);
                dlg.pushWaterfallRow(QStringLiteral("knapp"), row(8.0, i), kLo, kHi);
                dlg.pushWaterfallRow(QStringLiteral("tot"), row(0.0, i), kLo, kHi);
            }
            dlg.setMarkHz(kMark);
            QTest::qWait(50);
            dlg.grab().save(out);
        }
    }
};

QTEST_MAIN(TstKiwiGroup)
#include "tst_kiwi_group.moc"
