// =================================================================
// tests/tst_callsign_cache.cpp  (NereusSDR)
// =================================================================
//
// The cache exists so the logbook can show a name and a portrait
// without asking QRZ once per row. That makes two of its properties
// load-bearing in a way the code does not make obvious:
//
//   * it is keyed on the callsign QUERIED, not the one QRZ answered
//     with. Get that backwards and every portable call is a permanent
//     miss and a permanent request.
//   * a stale entry is still handed back. Deleting it on read throws
//     away the only copy at the exact moment the network cannot
//     replace it.
//
// And one trap that has nothing to do with caching: 0,0 is a valid
// coordinate. An entry with no position must not come back claiming
// Null Island, which is what a naive round-trip through JSON produces.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CallsignCache.h"

#include <QtTest/QtTest>

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>

using namespace NereusSDR;

namespace {

CallsignInfo sample()
{
    CallsignInfo i;
    i.call         = QStringLiteral("JA1XYZ");
    i.firstName    = QStringLiteral("Kenji");
    i.lastName     = QStringLiteral("Tanaka");
    i.nameFmt      = QStringLiteral("Kenji Tanaka");
    i.city         = QStringLiteral("Tokyo");
    i.country      = QStringLiteral("Japan");
    i.grid         = QStringLiteral("PM95uq");
    i.licenseClass = QStringLiteral("1");
    i.imageUrl     = QStringLiteral("https://cdn.qrz.com/x/ja1xyz/photo.jpg");
    i.latitude     = 35.68;
    i.longitude    = 139.76;
    i.hasLatLon    = true;
    i.lotw         = true;
    i.mailQsl      = true;
    i.fetchedUtc   = 1'750'000'000LL;
    return i;
}

} // namespace

class TstCallsignCache : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString path(const char* name) const
    {
        return m_dir.path() + QLatin1Char('/') + QLatin1String(name);
    }

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // ── The file ─────────────────────────────────────────────────────

    void every_field_survives_the_round_trip()
    {
        const CallsignInfo in = sample();
        const CallsignInfo out =
            CallsignCache::fromJson(CallsignCache::toJson(in));

        QCOMPARE(out.call,         in.call);
        QCOMPARE(out.firstName,    in.firstName);
        QCOMPARE(out.lastName,     in.lastName);
        QCOMPARE(out.nameFmt,      in.nameFmt);
        QCOMPARE(out.city,         in.city);
        QCOMPARE(out.country,      in.country);
        QCOMPARE(out.grid,         in.grid);
        QCOMPARE(out.licenseClass, in.licenseClass);
        QCOMPARE(out.imageUrl,     in.imageUrl);
        QCOMPARE(out.hasLatLon,    true);
        QVERIFY(std::abs(out.latitude  - in.latitude)  < 1e-9);
        QVERIFY(std::abs(out.longitude - in.longitude) < 1e-9);
        QCOMPARE(out.lotw,         true);
        QCOMPARE(out.eqsl,         false);
        QCOMPARE(out.mailQsl,      true);
        QCOMPARE(out.fetchedUtc,   in.fetchedUtc);
    }

    // 0,0 is in the Gulf of Guinea. An entry that has no position must
    // not come back claiming to be there — the map would draw a line to
    // it and the bearing would be wrong by most of a hemisphere.
    void a_missing_position_does_not_become_null_island()
    {
        CallsignInfo bare;
        bare.call = QStringLiteral("DL2AB");
        QCOMPARE(bare.hasLatLon, false);

        const CallsignInfo out =
            CallsignCache::fromJson(CallsignCache::toJson(bare));
        QVERIFY2(!out.hasLatLon,
                 "an entry with no position came back with one");
    }

    // Epoch seconds are past 2^31 in 2038 and QJsonValue stores numbers
    // as doubles. Reading through toInt() gives zero, which would make
    // every entry permanently stale.
    void a_large_timestamp_is_not_truncated()
    {
        CallsignInfo i = sample();
        i.fetchedUtc = 4'000'000'000LL;   // 2096
        QCOMPARE(CallsignCache::fromJson(CallsignCache::toJson(i)).fetchedUtc,
                 4'000'000'000LL);
    }

    void it_writes_a_file_and_reads_it_back()
    {
        const QString p = path("basic.json");
        {
            CallsignCache c(p);
            c.put(QStringLiteral("JA1XYZ"), sample());
            QVERIFY(c.save());
        }
        QVERIFY(QFile::exists(p));

        CallsignCache c2(p);
        c2.load();
        QCOMPARE(c2.count(), 1);
        QCOMPARE(c2.get(QStringLiteral("JA1XYZ")).nameFmt,
                 QStringLiteral("Kenji Tanaka"));
    }

    // A cache is a convenience. Refusing to start because one is
    // damaged would make it a liability.
    void a_corrupt_file_is_an_empty_cache_not_a_failure()
    {
        const QString p = path("corrupt.json");
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ this is not json at all ");
        f.close();

        CallsignCache c(p);
        c.load();                       // must not crash
        QCOMPARE(c.count(), 0);
        QVERIFY(!c.contains(QStringLiteral("JA1XYZ")));

        // And it must still be writable afterwards, so one bad run does
        // not cost every future one.
        c.put(QStringLiteral("DL2AB"), sample());
        QVERIFY(c.save());
        CallsignCache c2(p);
        c2.load();
        QCOMPARE(c2.count(), 1);
    }

    void a_file_that_is_not_there_is_simply_empty()
    {
        CallsignCache c(path("never-written.json"));
        c.load();
        QCOMPARE(c.count(), 0);
    }

    // ── The keying ───────────────────────────────────────────────────
    //
    // QRZ answers a query for VP2E/K1ABC with a record whose call is
    // K1ABC. Keying on the answer means the query is never a hit.
    void it_is_keyed_on_what_was_asked_not_what_came_back()
    {
        CallsignCache c(path("keying.json"));
        CallsignInfo info = sample();
        info.call = QStringLiteral("K1ABC");        // QRZ's canonical form
        c.put(QStringLiteral("VP2E/K1ABC"), info);  // what we asked

        QVERIFY2(c.contains(QStringLiteral("VP2E/K1ABC")),
                 "the queried call is not a hit — every portable "
                 "callsign would cost a request forever");
        QCOMPARE(c.get(QStringLiteral("VP2E/K1ABC")).call,
                 QStringLiteral("K1ABC"));
    }

    void lookup_ignores_case_and_surrounding_space()
    {
        CallsignCache c(path("case.json"));
        c.put(QStringLiteral("ja1xyz"), sample());
        QVERIFY(c.contains(QStringLiteral("JA1XYZ")));
        QVERIFY(c.contains(QStringLiteral("  ja1xyz ")));
    }

    void an_empty_call_is_not_stored()
    {
        CallsignCache c(path("empty.json"));
        c.put(QString(), sample());
        c.put(QStringLiteral("   "), sample());
        QCOMPARE(c.count(), 0);
    }

    // ── Staleness ────────────────────────────────────────────────────

    void put_stamps_the_time_when_the_caller_did_not()
    {
        CallsignCache c(path("stamp.json"));
        CallsignInfo i = sample();
        i.fetchedUtc = 0;
        c.put(QStringLiteral("DL2AB"), i);

        const qint64 t = c.get(QStringLiteral("DL2AB")).fetchedUtc;
        QVERIFY2(t > 0, "an entry with no timestamp can never go stale");
        QVERIFY(std::abs(t - QDateTime::currentSecsSinceEpoch()) < 5);
    }

    void a_fresh_entry_is_not_stale()
    {
        CallsignCache c(path("fresh.json"));
        CallsignInfo i = sample();
        i.fetchedUtc = QDateTime::currentSecsSinceEpoch() - 60;
        c.put(QStringLiteral("DL2AB"), i);
        QVERIFY(!c.isStale(QStringLiteral("DL2AB")));
    }

    void an_old_entry_is_stale_but_still_returned()
    {
        CallsignCache c(path("stale.json"));
        CallsignInfo i = sample();
        i.fetchedUtc = QDateTime::currentSecsSinceEpoch()
                       - (CallsignCache::kDefaultMaxAgeSeconds + 3600);
        c.put(QStringLiteral("JA1XYZ"), i);

        QVERIFY(c.isStale(QStringLiteral("JA1XYZ")));
        // The part that matters: it is still there. With no network,
        // last year's name beats no name.
        QVERIFY(c.contains(QStringLiteral("JA1XYZ")));
        QCOMPARE(c.get(QStringLiteral("JA1XYZ")).nameFmt,
                 QStringLiteral("Kenji Tanaka"));
    }

    void an_entry_with_no_timestamp_counts_as_stale()
    {
        CallsignInfo i = sample();
        i.fetchedUtc = 0;
        CallsignCache c(path("nostamp.json"));
        QVERIFY(c.isStale(i));
    }

    // A clock that was wrong when the entry was written must not buy it
    // validity until the year it claims.
    void a_timestamp_in_the_future_counts_as_stale()
    {
        CallsignInfo i = sample();
        i.fetchedUtc = QDateTime::currentSecsSinceEpoch() + 86400;
        CallsignCache c(path("future.json"));
        QVERIFY(c.isStale(i));
    }

    void a_missing_entry_is_stale_rather_than_fresh()
    {
        CallsignCache c(path("missing.json"));
        QVERIFY(c.isStale(QStringLiteral("NOBODY")));
    }

    void the_age_limit_can_be_changed()
    {
        CallsignCache c(path("age.json"));
        c.setMaxAgeSeconds(10);
        CallsignInfo i = sample();
        i.fetchedUtc = QDateTime::currentSecsSinceEpoch() - 60;
        c.put(QStringLiteral("DL2AB"), i);
        QVERIFY(c.isStale(QStringLiteral("DL2AB")));
    }

    // ── Overwriting ──────────────────────────────────────────────────

    void a_second_lookup_replaces_the_first()
    {
        CallsignCache c(path("replace.json"));
        c.put(QStringLiteral("DL2AB"), sample());

        CallsignInfo newer = sample();
        newer.nameFmt = QStringLiteral("Someone Else");
        c.put(QStringLiteral("DL2AB"), newer);

        QCOMPARE(c.count(), 1);
        QCOMPARE(c.get(QStringLiteral("DL2AB")).nameFmt,
                 QStringLiteral("Someone Else"));
    }

    void saving_twice_leaves_a_file_that_still_parses()
    {
        const QString p = path("twice.json");
        CallsignCache c(p);
        c.put(QStringLiteral("DL2AB"), sample());
        QVERIFY(c.save());
        c.put(QStringLiteral("JA1XYZ"), sample());
        QVERIFY(c.save());

        CallsignCache c2(p);
        c2.load();
        QCOMPARE(c2.count(), 2);
    }
};

QTEST_APPLESS_MAIN(TstCallsignCache)
#include "tst_callsign_cache.moc"
