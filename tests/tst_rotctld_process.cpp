// The rotctld command line, and the curated controller list.
//
// The command line is the whole of the "easy installation": get an
// argument wrong and the operator sees a rotator that will not connect,
// with the reason buried in a process they never asked to start. It is
// built by a static function so it can be checked without Hamlib being
// installed, which is also the state most CI machines are in.
// no-port-check: Longpath-original. Hamlib model numbers are quoted
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
    void der_grund_steht_nicht_in_der_letzten_zeile();
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


// Hamlibs stderr ist ein Ablaufprotokoll, und die letzte Zeile ist die
// FOLGE, nicht die Ursache. Am 2026-10-06 stand in Martins Protokoll
// unter "Rotator: rotctld exited" genau ein Wort: "IO error". Wer nichts
// erfaehrt, sucht beim Programm statt beim Kabel.
//
// Die Vorlage unten ist die echte Ausgabe aus jenem Protokoll, gekuerzt
// um die rot_register-Zeilen, die nichts sagen.
void TstRotctldProcess::der_grund_steht_nicht_in_der_letzten_zeile()
{
    const QString echt = QStringLiteral(
        "rot_open: error = rot_register (609)\n"
        "gs232a_rot_init called\n"
        "rot_open called\n"
        "rot_open: using network address 192.168.1.16:4001:TCP\n"
        "network_open: TCP connect\n"
        "network_open: hoststr=192.168.1.16, portstr=4001\n"
        "connect to 192.168.1.16:4001 failed, (trying next interface): "
        "Network error 60: Operation timed out\n"
        "network_open: failed to connect to 192.168.1.16:4001\n"
        "IO error\n");

    const QString grund = RotctldProcess::grundAus(echt);
    // Die Ursache, nicht die Folge -- und mit der Adresse daran, denn genau
    // die war hier falsch.
    QVERIFY2(grund.contains(QStringLiteral("timed out")), qPrintable(grund));
    QVERIFY2(grund.contains(QStringLiteral("192.168.1.16:4001")), qPrintable(grund));
    QVERIFY2(grund != QStringLiteral("IO error"), qPrintable(grund));

    // Zweite Stufe: nennt keine Zeile eine Ursache, nimmt er die letzte, die
    // ueberhaupt von einem Fehlschlag spricht.
    const QString ohneUrsache = QStringLiteral(
        "rot_open called\n"
        "network_open: failed to connect to 10.0.0.9:4533\n"
        "IO error\n");
    QCOMPARE(RotctldProcess::grundAus(ohneUrsache),
             QStringLiteral("network_open: failed to connect to 10.0.0.9:4533"));

    // Dritte Stufe: sagt gar nichts etwas aus, bleibt die letzte nichtleere
    // Zeile -- lieber wenig als nichts.
    QCOMPARE(RotctldProcess::grundAus(QStringLiteral("abc\ndef\n\n")),
             QStringLiteral("def"));
    QCOMPARE(RotctldProcess::grundAus(QString()), QString());

    // Ein belegter Port ist der zweite Fall, der Martin schon einmal
    // getroffen hat (2026-09-16, verwaister rotctld auf 4533).
    QVERIFY(RotctldProcess::grundAus(QStringLiteral(
                "bind: Address already in use\nIO error\n"))
                .contains(QStringLiteral("in use")));
}

QTEST_MAIN(TstRotctldProcess)
#include "tst_rotctld_process.moc"
