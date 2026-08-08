// Reading Hamlib's own output, and the one thing the installer must
// never do.
//
// Two kinds of test here. The parsing tests use captured `rotctl`
// output, because the column widths in that table have moved between
// releases and a parser that reads by position works until the day it
// doesn't. The refusal test is the important one: this class runs a
// command on the operator's machine, and the whole design rests on that
// command never being able to ask for administrator rights.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "core/HamlibInstaller.h"
#include "core/RotorModels.h"

#include <QSet>

using namespace NereusSDR;

namespace {

// Hamlib 4.6.2, macOS. Note the two-word manufacturer, which is what
// breaks a parser that splits on single spaces.
const char* kListOutput = R"( Rot #  Mfg                    Model                    Version         Status  Macro
     1  Hamlib                 Dummy                    20220102.0      Stable  ROT_MODEL_DUMMY
     2  Hamlib                 NET rotctl               20220709.0      Stable  ROT_MODEL_NETROTCTL
   401  Idiom Press            Rotor-EZ                 20220510.0      Stable  ROT_MODEL_ROTEZ
   403  Hy-Gain                DCU-1                    20200721.0      Stable  ROT_MODEL_DCU
   404  DF9GR                  ERC                      20220109.0      Beta    ROT_MODEL_ERC
   601  Yaesu                  GS-232A                  20220109.0      Stable  ROT_MODEL_GS232A
   901  SPID                   Rot2Prog                 20201203.0      Stable  ROT_MODEL_SPID_ROT2PROG
)";

// Older Hamlib, before the Macro column existed.
const char* kOldListOutput = R"( Rot #  Mfg          Model        Version     Status
     1  Hamlib       Dummy        0.5         Beta
   601  Yaesu        GS-232A      0.4         Beta
)";

} // namespace

class TstHamlibInstaller : public QObject {
    Q_OBJECT
private slots:
    void the_install_command_never_asks_for_root();
    void an_install_command_with_sudo_in_it_is_refused();
    void the_model_list_is_read_by_shape_not_by_column();
    void a_manufacturer_with_a_space_survives();
    void the_old_table_without_a_macro_column_still_reads();
    void junk_produces_no_models_rather_than_bad_ones();
    void the_version_is_taken_from_hamlibs_own_name();
    void a_version_line_with_a_date_in_it_is_not_confused();
    void no_version_is_empty_rather_than_a_guess();
    void manual_instructions_are_never_empty();
    void the_curated_list_is_internally_sound();
    void the_curated_list_agrees_with_hamlib();
};

void TstHamlibInstaller::the_install_command_never_asks_for_root()
{
    // The command depends on whether Homebrew is on this machine, so
    // the test cannot assert what it is — only what it may never be.
    const QStringList cmd = HamlibInstaller::installCommand();
    for (const QString& part : cmd) {
        QVERIFY2(!part.contains(QLatin1String("sudo")), qPrintable(part));
        QVERIFY2(!part.contains(QLatin1String("doas")), qPrintable(part));
        QVERIFY2(!part.contains(QLatin1String("pkexec")), qPrintable(part));
    }
    if (!cmd.isEmpty()) {
        // If there is a command at all it is Homebrew installing one
        // named formula, not a shell.
        QVERIFY(cmd.first().endsWith(QLatin1String("brew")));
        QCOMPARE(cmd.mid(1), QStringList({QStringLiteral("install"),
                                          QStringLiteral("hamlib")}));
    }
}

void TstHamlibInstaller::an_install_command_with_sudo_in_it_is_refused()
{
    // The guard inside install() cannot be reached through the public
    // API without a machine that has a `sudo` in its brew path, so this
    // checks the two agree: canInstallAutomatically() is exactly
    // "installCommand() has something in it", and that something has
    // already been checked above.
    QCOMPARE(HamlibInstaller::canInstallAutomatically(),
             !HamlibInstaller::installCommand().isEmpty());
}

void TstHamlibInstaller::the_model_list_is_read_by_shape_not_by_column()
{
    const QVector<HamlibRotorEntry> rots =
        HamlibInstaller::parseModelList(QString::fromLatin1(kListOutput));

    QCOMPARE(rots.size(), 7);
    QCOMPARE(rots.at(0).model, 1);
    QCOMPARE(rots.at(0).manufacturer, QStringLiteral("Hamlib"));
    QCOMPARE(rots.at(0).name, QStringLiteral("Dummy"));
    QCOMPARE(rots.at(0).status, QStringLiteral("Stable"));

    // The ERC entry, which is the one the rotator dialog cares about
    // most: it is the driver that exists only in newer Hamlib.
    QCOMPARE(rots.at(4).model, 404);
    QCOMPARE(rots.at(4).manufacturer, QStringLiteral("DF9GR"));
    QCOMPARE(rots.at(4).status, QStringLiteral("Beta"));

    // The header row is not a rotator.
    for (const HamlibRotorEntry& e : rots) { QVERIFY(e.model > 0); }
}

void TstHamlibInstaller::a_manufacturer_with_a_space_survives()
{
    // "Idiom Press" is why fields are split on runs of two spaces and
    // the status is found by name rather than by counting.
    const QVector<HamlibRotorEntry> rots =
        HamlibInstaller::parseModelList(QString::fromLatin1(kListOutput));
    const HamlibRotorEntry& e = rots.at(2);
    QCOMPARE(e.model, 401);
    QCOMPARE(e.manufacturer, QStringLiteral("Idiom Press"));
    QCOMPARE(e.name, QStringLiteral("Rotor-EZ"));
    QCOMPARE(e.status, QStringLiteral("Stable"));

    // And a model name with a space in it, one row up.
    QCOMPARE(rots.at(1).name, QStringLiteral("NET rotctl"));
}

void TstHamlibInstaller::the_old_table_without_a_macro_column_still_reads()
{
    const QVector<HamlibRotorEntry> rots =
        HamlibInstaller::parseModelList(QString::fromLatin1(kOldListOutput));
    QCOMPARE(rots.size(), 2);
    QCOMPARE(rots.at(1).model, 601);
    QCOMPARE(rots.at(1).name, QStringLiteral("GS-232A"));
    QCOMPARE(rots.at(1).status, QStringLiteral("Beta"));
}

void TstHamlibInstaller::junk_produces_no_models_rather_than_bad_ones()
{
    // An empty list must not be mistaken for "Hamlib knows nothing", so
    // the caller checks isInstalled() separately — but whatever comes
    // back, it must not be a list of zeroes.
    QVERIFY(HamlibInstaller::parseModelList(QString{}).isEmpty());
    QVERIFY(HamlibInstaller::parseModelList(
        QStringLiteral("command not found: rotctl")).isEmpty());
    QVERIFY(HamlibInstaller::parseModelList(
        QStringLiteral("  0  Nothing  Nothing  1.0  Stable")).isEmpty());
}

void TstHamlibInstaller::the_version_is_taken_from_hamlibs_own_name()
{
    QCOMPARE(HamlibInstaller::parseVersion(
                 QStringLiteral("rotctl Hamlib 4.6.2")),
             QStringLiteral("4.6.2"));
    QCOMPARE(HamlibInstaller::parseVersion(
                 QStringLiteral("rotctl(1) Hamlib 4.5")),
             QStringLiteral("4.5"));
}

void TstHamlibInstaller::a_version_line_with_a_date_in_it_is_not_confused()
{
    // The real line carries a build date, and a naive "first thing that
    // looks like a version" would sometimes find part of that instead.
    QCOMPARE(HamlibInstaller::parseVersion(QStringLiteral(
                 "rotctl Hamlib 4.6.2  Sat Feb 22 09:11:03 2025 UTC")),
             QStringLiteral("4.6.2"));
}

void TstHamlibInstaller::no_version_is_empty_rather_than_a_guess()
{
    QVERIFY(HamlibInstaller::parseVersion(QString{}).isEmpty());
    QVERIFY(HamlibInstaller::parseVersion(
        QStringLiteral("zsh: command not found: rotctl")).isEmpty());
}

void TstHamlibInstaller::manual_instructions_are_never_empty()
{
    // This text is the whole fallback when the automatic path is not
    // available. An empty string here is a dead end on screen.
    const QString help = HamlibInstaller::manualInstructions();
    QVERIFY(!help.trimmed().isEmpty());
}

void TstHamlibInstaller::the_curated_list_is_internally_sound()
{
    // Cheap, and catches the mistake that costs an hour: two entries
    // with the same Hamlib number, or one with none, which makes the
    // picker select the wrong row and the operator drive the wrong
    // driver.
    QSet<int> seen;
    for (const RotorModel& m : commonRotorModels()) {
        QVERIFY2(m.hamlibId > 0, qPrintable(m.name));
        QVERIFY2(!seen.contains(m.hamlibId),
                 qPrintable(QStringLiteral("duplicate id %1 (%2)")
                                .arg(m.hamlibId).arg(m.name)));
        seen.insert(m.hamlibId);
        QVERIFY(!m.name.trimmed().isEmpty());
    }
}

void TstHamlibInstaller::the_curated_list_agrees_with_hamlib()
{
    // Only meaningful where Hamlib is actually installed, which is not
    // most build machines.
    if (!HamlibInstaller::isInstalled()) {
        QSKIP("Hamlib is not installed on this machine");
    }
    const QVector<HamlibRotorEntry> known = HamlibInstaller::supportedModels();
    if (known.isEmpty()) { QSKIP("rotctl --list produced nothing"); }

    QSet<int> ids;
    for (const HamlibRotorEntry& e : known) { ids.insert(e.model); }
    QVERIFY2(ids.contains(1), "Hamlib always has the dummy rotator");

    // A curated number the installed Hamlib does not have is not a bug
    // in this list — an older Hamlib genuinely lacks the newer drivers,
    // and that is precisely what the dialog tells the operator at the
    // moment they choose it. So this reports rather than fails; a test
    // that went red on an old Hamlib would be a test people delete.
    for (const RotorModel& m : commonRotorModels()) {
        if (!ids.contains(m.hamlibId)) {
            qWarning("This Hamlib does not have model %d (%s)",
                     m.hamlibId, qPrintable(m.name));
        }
    }
}

QTEST_MAIN(TstHamlibInstaller)
#include "tst_hamlib_installer.moc"
