// The rotctld command line, and the curated controller list.
//
// The command line is the whole of the "easy installation": get an
// argument wrong and the operator sees a rotator that will not connect,
// with the reason buried in a process they never asked to start. It is
// built by a static function so it can be checked without Hamlib being
// installed, which is also the state most CI machines are in.
// no-port-check: NereusSDR-original. Hamlib model numbers are quoted
// from the Hamlib wiki and attributed in RotorModels.h.

#include <QtTest/QtTest>

#include "core/RotctldProcess.h"
#include "core/RotorModels.h"

using namespace Longpath;

class TstRotctldProcess : public QObject {
    Q_OBJECT
private slots:
    void a_serial_rotator_gets_model_device_and_speed();
    void an_empty_device_is_left_out_entirely();
    void it_listens_on_loopback_only();
    void the_model_list_has_no_duplicates();
    void the_model_list_covers_the_controllers_asked_about();
    void every_model_explains_itself();
};

void TstRotctldProcess::a_serial_rotator_gets_model_device_and_speed()
{
    const QStringList a = RotctldProcess::arguments(
        603, QStringLiteral("/dev/tty.usbserial-1410"), 9600, 4533);

    QCOMPARE(a, QStringList({
        QStringLiteral("-m"), QStringLiteral("603"),
        QStringLiteral("-r"), QStringLiteral("/dev/tty.usbserial-1410"),
        QStringLiteral("-s"), QStringLiteral("9600"),
        QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
        QStringLiteral("-t"), QStringLiteral("4533")}));
}

void TstRotctldProcess::an_empty_device_is_left_out_entirely()
{
    // A bare "-r" with nothing after it makes rotctld take the next
    // flag as the device name, and it then fails complaining about a
    // serial port called "-T". Leaving the pair out lets Hamlib use its
    // own default, which is what a network model wants anyway.
    const QStringList a = RotctldProcess::arguments(1501, QString{}, 0, 4533);
    QVERIFY(!a.contains(QStringLiteral("-r")));
    QVERIFY(!a.contains(QStringLiteral("-s")));
    QVERIFY(a.contains(QStringLiteral("-m")));

    const QStringList blank =
        RotctldProcess::arguments(601, QStringLiteral("   "), 9600, 4533);
    QVERIFY(!blank.contains(QStringLiteral("-r")));
}

void TstRotctldProcess::it_listens_on_loopback_only()
{
    // rotctld has no authentication of any kind. Bound to 0.0.0.0 —
    // which is the example in most instructions — anyone on the network
    // can turn the mast. This is the one argument that must not drift.
    for (int model : {601, 603, 404, 403, 901}) {
        const QStringList a = RotctldProcess::arguments(
            model, QStringLiteral("/dev/ttyUSB0"), 9600, 4533);
        const int at = a.indexOf(QStringLiteral("-T"));
        QVERIFY2(at >= 0 && at + 1 < a.size(), "no listen address given");
        QCOMPARE(a.at(at + 1), QStringLiteral("127.0.0.1"));
        QVERIFY(!a.contains(QStringLiteral("0.0.0.0")));
    }
}

void TstRotctldProcess::the_model_list_has_no_duplicates()
{
    // Two entries with the same number is a picker where one choice
    // silently does nothing different from another.
    QSet<int> ids;
    QSet<QString> names;
    for (const RotorModel& m : commonRotorModels()) {
        QVERIFY2(!ids.contains(m.hamlibId),
                 qPrintable(QStringLiteral("duplicate id %1").arg(m.hamlibId)));
        QVERIFY2(!names.contains(m.name), qPrintable(m.name));
        ids.insert(m.hamlibId);
        names.insert(m.name);
        QVERIFY(m.hamlibId > 0);
    }
}

void TstRotctldProcess::the_model_list_covers_the_controllers_asked_about()
{
    // ERC has its own Hamlib driver at 404. ARCO has none, but speaks
    // GS-232A (601), DCU-1 (403) and SPID (902), so all three have to
    // be reachable from the list or an ARCO owner is stuck.
    QSet<int> ids;
    for (const RotorModel& m : commonRotorModels()) { ids.insert(m.hamlibId); }

    QVERIFY2(ids.contains(404), "ERC missing");
    QVERIFY2(ids.contains(601), "GS-232A missing — ARCO needs it");
    QVERIFY2(ids.contains(403), "DCU-1 missing — ARCO needs it");
    QVERIFY2(ids.contains(902), "SPID Rot1Prog missing — ARCO needs it");
    QVERIFY2(ids.contains(603), "GS-232B missing");
}

void TstRotctldProcess::every_model_explains_itself()
{
    // The note is not decoration. Someone holding an ARCO box has no
    // idea that "Yaesu GS-232A" is their entry, and a list of model
    // names alone would send them looking for an ARCO line that does
    // not exist.
    for (const RotorModel& m : commonRotorModels()) {
        QVERIFY2(!m.note.trimmed().isEmpty(), qPrintable(m.name));
    }

    // And specifically: the entries an ARCO or ERC owner would land on
    // have to name their box.
    bool arcoNamed = false;
    bool ercNamed  = false;
    for (const RotorModel& m : commonRotorModels()) {
        if (m.note.contains(QStringLiteral("ARCO"), Qt::CaseInsensitive)) {
            arcoNamed = true;
        }
        if (m.name.contains(QStringLiteral("ERC"))) { ercNamed = true; }
    }
    QVERIFY2(arcoNamed, "no entry mentions ARCO");
    QVERIFY2(ercNamed, "no entry mentions ERC");
}

QTEST_MAIN(TstRotctldProcess)
#include "tst_rotctld_process.moc"
