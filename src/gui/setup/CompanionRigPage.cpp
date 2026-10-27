// =================================================================
// src/gui/setup/CompanionRigPage.cpp  (Longpath)
// =================================================================
//
// Longpath-original — siehe CompanionRigPage.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "CompanionRigPage.h"

#include "core/AppSettings.h"
#include "core/HamlibInstaller.h"
#include "core/RigctldClient.h"
#include "core/RigctldProcess.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Die ueblichen Baudraten eines Kurzwellengeraets am CAT-Anschluss.
// Welche gilt, steht im Geraetemenue (TS-590S Nr. 62, TS-590SG Nr. 68)
// und muss dort uebereinstimmen -- das ist der Teil, den kein
// Programm erraten kann.
const QList<int> kBaudraten = {4800, 9600, 19200, 38400, 57600, 115200};

QString bool2s(bool b)
{
    return b ? QStringLiteral("True") : QStringLiteral("False");
}

bool s2bool(const QVariant& v)
{
    return v.toString() == QStringLiteral("True");
}

} // namespace

QStringList CompanionRigPage::bevorzugteGeraete()
{
    // Nach NAMEN, nicht nach Nummer -- siehe Kopfdatei.
    return {
        QStringLiteral("TS-590"),
        QStringLiteral("K3"),
        QStringLiteral("K4"),
        QStringLiteral("TS-890"),
        QStringLiteral("TS-2000"),
        QStringLiteral("IC-7300"),
        QStringLiteral("IC-7610"),
        QStringLiteral("IC-9700"),
        QStringLiteral("FTDX"),
        QStringLiteral("FT-991"),
        QStringLiteral("NET rigctl"),
        QStringLiteral("Dummy"),
    };
}

qint64 CompanionRigPage::mhzZuHz(double mhz)
{
    return static_cast<qint64>(std::llround(mhz * 1000000.0));
}

CompanionRigPage::CompanionRigPage(QWidget* parent)
    : SetupPage(QStringLiteral("Companion Rig"), parent)
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    m_tv = TransverterSetup::load();

    baueCatGruppe();
    baueTransverterGruppe();

    zeigeTransverter();
}

CompanionRigPage::~CompanionRigPage()
{
    // Der Testlauf endet mit der Seite. Ein rigctld, das diese Seite
    // gestartet hat, wird dabei beendet -- sonst sitzt es nach dem
    // Schliessen des Dialogs auf dem Anschluss und das naechste
    // Programm findet ihn belegt.
    beendeTest();
}

void CompanionRigPage::baueCatGruppe()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(
        QStringLiteral("Companion rig (CAT via Hamlib rigctld)"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* grid = new QGridLayout(group);
    grid->setSpacing(6);
    int row = 0;

    m_folgen = new QCheckBox(
        QStringLiteral("Follow a companion rig"), group);
    // Namen, damit Pruefstand und Automation-Bruecke die Felder finden,
    // ohne nach sichtbarem Text zu suchen -- der darf sich aendern.
    m_folgen->setObjectName(QStringLiteral("companionFollow"));
    m_folgen->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_folgen->setChecked(
        s2bool(s.value(QStringLiteral("CompanionRigEnabled"),
                       QStringLiteral("False"))));
    m_folgen->setToolTip(QStringLiteral(
        "For a transverter station: the companion rig works as the IF and "
        "transmits, Longpath listens on the transverter's IF output and "
        "shows the band. This reads the rig's frequency so the display "
        "knows where in the band you are.\n\n"
        "Longpath never keys the companion rig. It reads PTT, it does not "
        "set it."));
    grid->addWidget(m_folgen, row++, 0, 1, 2);

    auto* geraetLabel = new QLabel(QStringLiteral("Rig:"), group);
    geraetLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(geraetLabel, row, 0);
    m_geraet = new QComboBox(group);
    m_geraet->setObjectName(QStringLiteral("companionRigModel"));
    m_geraet->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_geraet->setToolTip(QStringLiteral(
        "Read at run time from `rigctl --list`, so it is whatever the "
        "installed Hamlib knows. The usual IF rigs are listed first."));
    grid->addWidget(m_geraet, row++, 1);

    auto* anschlussLabel = new QLabel(QStringLiteral("Serial port:"), group);
    anschlussLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(anschlussLabel, row, 0);
    m_anschluss = new QLineEdit(
        s.value(QStringLiteral("CompanionRigDevice")).toString(), group);
    m_anschluss->setObjectName(QStringLiteral("companionRigDevice"));
    m_anschluss->setStyleSheet(Style::lineEditStyle());
    m_anschluss->setPlaceholderText(
        QStringLiteral("/dev/cu.usbserial-…"));
    m_anschluss->setToolTip(QStringLiteral(
        "The device the rig appears as. Plug the USB cable in and look: "
        "`ls /dev/cu.*`. Leave empty for a network rig, where Hamlib takes "
        "an address instead."));
    grid->addWidget(m_anschluss, row++, 1);

    auto* baudLabel = new QLabel(QStringLiteral("Baud rate:"), group);
    baudLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(baudLabel, row, 0);
    m_baud = new QComboBox(group);
    m_baud->setObjectName(QStringLiteral("companionRigBaud"));
    m_baud->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    for (int b : kBaudraten) {
        m_baud->addItem(QString::number(b), b);
    }
    const int gespeicherterBaud =
        s.value(QStringLiteral("CompanionRigBaud"),
                QStringLiteral("9600")).toInt();
    const int baudIdx = m_baud->findData(gespeicherterBaud);
    m_baud->setCurrentIndex(baudIdx >= 0 ? baudIdx : 1);
    m_baud->setToolTip(QStringLiteral(
        "Must match the rig's own menu setting (TS-590S menu 62, "
        "TS-590SG menu 68). This is the one thing no program can guess."));
    grid->addWidget(m_baud, row++, 1);

    m_selbstStarten = new QCheckBox(
        QStringLiteral("Start rigctld automatically"), group);
    m_selbstStarten->setStyleSheet(
        QString::fromLatin1(Style::kCheckBoxStyle));
    m_selbstStarten->setChecked(
        s2bool(s.value(QStringLiteral("CompanionRigStartDaemon"),
                       QStringLiteral("True"))));
    m_selbstStarten->setToolTip(QStringLiteral(
        "On: Longpath starts rigctld itself, on loopback only, and stops it "
        "again. Off: run rigctld yourself and point the host and port below "
        "at it."));
    grid->addWidget(m_selbstStarten, row++, 0, 1, 2);

    auto* hostLabel = new QLabel(QStringLiteral("rigctld host:"), group);
    hostLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(hostLabel, row, 0);
    m_host = new QLineEdit(
        s.value(QStringLiteral("CompanionRigHost"),
                QStringLiteral("127.0.0.1")).toString(), group);
    m_host->setStyleSheet(Style::lineEditStyle());
    grid->addWidget(m_host, row++, 1);

    auto* portLabel = new QLabel(QStringLiteral("rigctld port:"), group);
    portLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(portLabel, row, 0);
    m_port = new QSpinBox(group);
    m_port->setStyleSheet(Style::spinBoxStyle());
    m_port->setRange(1024, 65535);
    m_port->setValue(s.value(QStringLiteral("CompanionRigPort"),
                             QStringLiteral("4532")).toInt());
    m_port->setToolTip(QStringLiteral(
        "4532 is rigctld's own port, the sibling of rotctld's 4533."));
    grid->addWidget(m_port, row++, 1);

    m_test = new QPushButton(QStringLiteral("Test"), group);
    m_test->setObjectName(QStringLiteral("companionRigTest"));
    m_test->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_test->setToolTip(QStringLiteral(
        "Connects for as long as this page is open and shows what the rig "
        "reports. The permanent connection is not made here."));
    grid->addWidget(m_test, row, 0);

    m_status = new QLabel(QStringLiteral("Not tested"), group);
    m_status->setObjectName(QStringLiteral("companionRigStatus"));
    m_status->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_status->setWordWrap(true);
    grid->addWidget(m_status, row++, 1);

    // VOR den Platzhalter, den SetupPage::init() schon angelegt hat --
    // dahinter eingehaengt beginnt die Seite erst unterhalb einer
    // handbreiten Luecke.
    contentLayout()->insertWidget(contentLayout()->count() - 1, group);

    ladeGeraeteListe();

    connect(m_folgen, &QCheckBox::toggled,
            this, &CompanionRigPage::speichereCat);
    connect(m_geraet, &QComboBox::currentIndexChanged,
            this, &CompanionRigPage::speichereCat);
    connect(m_anschluss, &QLineEdit::editingFinished,
            this, &CompanionRigPage::speichereCat);
    connect(m_baud, &QComboBox::currentIndexChanged,
            this, &CompanionRigPage::speichereCat);
    connect(m_selbstStarten, &QCheckBox::toggled,
            this, &CompanionRigPage::speichereCat);
    connect(m_host, &QLineEdit::editingFinished,
            this, &CompanionRigPage::speichereCat);
    connect(m_port, &QSpinBox::valueChanged,
            this, &CompanionRigPage::speichereCat);
    connect(m_test, &QPushButton::clicked, this, [this]() {
        if (m_client) { beendeTest(); } else { starteTest(); }
    });
}

void CompanionRigPage::ladeGeraeteListe()
{
    const QVector<HamlibRotorEntry> alle = HamlibInstaller::supportedRigs();
    if (alle.isEmpty()) {
        m_geraet->setEnabled(false);
        m_geraet->addItem(
            QStringLiteral("Hamlib not found — install it to pick a rig"), 0);
        // Keine Behauptung, dass es keine Geraete gibt: die Liste ist
        // leer, weil rigctl fehlt. Dieselbe Regel wie bei den Rotoren.
        return;
    }

    const QStringList bevorzugt = bevorzugteGeraete();
    QVector<QPair<int, HamlibRotorEntry>> obenMitRang;
    QVector<HamlibRotorEntry> rest;
    for (const HamlibRotorEntry& e : alle) {
        int rang = -1;
        for (int i = 0; i < bevorzugt.size(); ++i) {
            if (e.name.contains(bevorzugt.at(i), Qt::CaseInsensitive)) {
                rang = i;
                break;
            }
        }
        if (rang >= 0) {
            obenMitRang.append({rang, e});
        } else {
            rest.append(e);
        }
    }

    // Nach dem Rang aus bevorzugteGeraete(), nicht nach der
    // Hamlib-Nummer: sonst steht Hamlibs Dummy (Nummer 1) als erstes
    // da, und das ist das eine Geraet, das sicher nicht am Kabel
    // haengt.
    std::stable_sort(obenMitRang.begin(), obenMitRang.end(),
                     [](const QPair<int, HamlibRotorEntry>& a,
                        const QPair<int, HamlibRotorEntry>& b) {
                         return a.first < b.first;
                     });
    QVector<HamlibRotorEntry> oben;
    oben.reserve(obenMitRang.size());
    for (const QPair<int, HamlibRotorEntry>& p : obenMitRang) {
        oben.append(p.second);
    }

    const auto eintragen = [this](const QVector<HamlibRotorEntry>& v) {
        for (const HamlibRotorEntry& e : v) {
            m_geraet->addItem(
                QStringLiteral("%1 — %2 %3")
                    .arg(QString::number(e.model), e.manufacturer, e.name),
                e.model);
        }
    };
    eintragen(oben);
    if (!oben.isEmpty() && !rest.isEmpty()) {
        m_geraet->insertSeparator(m_geraet->count());
    }
    eintragen(rest);

    const int gespeichert =
        AppSettings::instance()
            .value(QStringLiteral("CompanionRigModel"), 0).toInt();
    const int idx = m_geraet->findData(gespeichert);
    if (idx >= 0) { m_geraet->setCurrentIndex(idx); }
}

int CompanionRigPage::gewaehltesModell() const
{
    return m_geraet ? m_geraet->currentData().toInt() : 0;
}

void CompanionRigPage::speichereCat()
{
    if (m_fuellt) { return; }
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("CompanionRigEnabled"),
               bool2s(m_folgen->isChecked()));
    s.setValue(QStringLiteral("CompanionRigModel"), gewaehltesModell());
    s.setValue(QStringLiteral("CompanionRigDevice"),
               m_anschluss->text().trimmed());
    s.setValue(QStringLiteral("CompanionRigBaud"),
               m_baud->currentData().toInt());
    s.setValue(QStringLiteral("CompanionRigStartDaemon"),
               bool2s(m_selbstStarten->isChecked()));
    s.setValue(QStringLiteral("CompanionRigHost"), m_host->text().trimmed());
    s.setValue(QStringLiteral("CompanionRigPort"), m_port->value());
}

void CompanionRigPage::baueTransverterGruppe()
{
    auto* group = new QGroupBox(QStringLiteral("Transverter"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* grid = new QGridLayout(group);
    grid->setSpacing(6);
    int row = 0;

    m_tvEin = new QCheckBox(
        QStringLiteral("Show the frequency on the antenna"), group);
    m_tvEin->setObjectName(QStringLiteral("transverterEnabled"));
    m_tvEin->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_tvEin->setChecked(m_tv.enabled);
    m_tvEin->setToolTip(QStringLiteral(
        "On: an IF of 28.300 MHz is shown and logged as 144.300 MHz. Only "
        "frequencies inside the IF range below are converted — tune to 20 m "
        "with this on and you still see 14 MHz."));
    grid->addWidget(m_tvEin, row++, 0, 1, 2);

    auto* vorgabeLabel = new QLabel(QStringLiteral("Preset:"), group);
    vorgabeLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(vorgabeLabel, row, 0);
    m_tvVorgabe = new QComboBox(group);
    m_tvVorgabe->setObjectName(QStringLiteral("transverterPreset"));
    m_tvVorgabe->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_tvVorgabe->addItem(QStringLiteral("Custom"), -1);
    const QVector<TransverterSetup::Preset> v = TransverterSetup::presets();
    for (int i = 0; i < v.size(); ++i) {
        m_tvVorgabe->addItem(QString::fromLatin1(v.at(i).name), i);
    }
    grid->addWidget(m_tvVorgabe, row++, 1);

    const auto macheSpin = [group](const QString& tip) {
        auto* sp = new QDoubleSpinBox(group);
        sp->setStyleSheet(Style::doubleSpinBoxStyle());
        sp->setDecimals(6);
        // Punkt, nicht Komma: die Zusammenfassung unter den Feldern
        // schreibt in der C-Variante (QString::number), und auf einem
        // deutschen Rechner staenden sonst "28,000000 MHz" und
        // "28.000-30.000 MHz" uebereinander.
        sp->setLocale(QLocale::c());
        sp->setSingleStep(0.1);
        sp->setSuffix(QStringLiteral(" MHz"));
        sp->setToolTip(tip);
        return sp;
    };

    auto* vonLabel = new QLabel(QStringLiteral("IF from:"), group);
    vonLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(vonLabel, row, 0);
    m_tvVon = macheSpin(QStringLiteral(
        "Lower end of the IF window the transverter mixes — 28.000 for a "
        "144/28 transverter."));
    m_tvVon->setObjectName(QStringLiteral("transverterIfLow"));
    m_tvVon->setRange(0.0, 30000.0);
    grid->addWidget(m_tvVon, row++, 1);

    auto* bisLabel = new QLabel(QStringLiteral("IF to:"), group);
    bisLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(bisLabel, row, 0);
    m_tvBis = macheSpin(QStringLiteral("Upper end of the IF window."));
    m_tvBis->setObjectName(QStringLiteral("transverterIfHigh"));
    m_tvBis->setRange(0.0, 30000.0);
    grid->addWidget(m_tvBis, row++, 1);

    auto* versatzLabel = new QLabel(QStringLiteral("Offset:"), group);
    versatzLabel->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(versatzLabel, row, 0);
    m_tvVersatz = macheSpin(QStringLiteral(
        "Antenna = IF + offset. 116 MHz for 144/28, 1152 MHz for 1296/144. "
        "Negative for a transverter that mixes down."));
    m_tvVersatz->setObjectName(QStringLiteral("transverterOffset"));
    m_tvVersatz->setRange(-30000.0, 30000.0);
    grid->addWidget(m_tvVersatz, row++, 1);

    m_tvText = new QLabel(QString(), group);
    m_tvText->setObjectName(QStringLiteral("transverterSummary"));
    m_tvText->setStyleSheet(
        QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_tvText->setWordWrap(true);
    grid->addWidget(m_tvText, row++, 0, 1, 2);

    // VOR den Platzhalter, den SetupPage::init() schon angelegt hat --
    // dahinter eingehaengt beginnt die Seite erst unterhalb einer
    // handbreiten Luecke.
    contentLayout()->insertWidget(contentLayout()->count() - 1, group);

    connect(m_tvEin, &QCheckBox::toggled, this, [this](bool on) {
        m_tv.enabled = on;
        speichereTransverter();
    });
    connect(m_tvVorgabe, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_fuellt) { return; }
        const int i = m_tvVorgabe->currentData().toInt();
        if (i < 0) { return; }
        const QVector<TransverterSetup::Preset> p =
            TransverterSetup::presets();
        if (i >= p.size()) { return; }
        m_tv.ifLowHz  = p.at(i).ifLowHz;
        m_tv.ifHighHz = p.at(i).ifHighHz;
        m_tv.offsetHz = p.at(i).offsetHz;
        zeigeTransverter();
        speichereTransverter();
    });
    const auto ausDenFeldern = [this]() {
        if (m_fuellt) { return; }
        m_tv.ifLowHz  = mhzZuHz(m_tvVon->value());
        m_tv.ifHighHz = mhzZuHz(m_tvBis->value());
        m_tv.offsetHz = mhzZuHz(m_tvVersatz->value());
        // Von Hand getippt heisst „Custom" -- sonst behauptet die
        // Auswahl eine Vorgabe, die nicht mehr drinsteht.
        m_fuellt = true;
        m_tvVorgabe->setCurrentIndex(0);
        m_fuellt = false;
        m_tvText->setText(m_tv.describe());
        speichereTransverter();
    };
    connect(m_tvVon, &QDoubleSpinBox::editingFinished, this, ausDenFeldern);
    connect(m_tvBis, &QDoubleSpinBox::editingFinished, this, ausDenFeldern);
    connect(m_tvVersatz, &QDoubleSpinBox::editingFinished,
            this, ausDenFeldern);
}

void CompanionRigPage::zeigeTransverter()
{
    m_fuellt = true;
    m_tvEin->setChecked(m_tv.enabled);
    m_tvVon->setValue(static_cast<double>(m_tv.ifLowHz) / 1e6);
    m_tvBis->setValue(static_cast<double>(m_tv.ifHighHz) / 1e6);
    m_tvVersatz->setValue(static_cast<double>(m_tv.offsetHz) / 1e6);

    // Trifft die Einstellung eine Vorgabe, soll die Auswahl das zeigen.
    int treffer = 0;
    const QVector<TransverterSetup::Preset> p = TransverterSetup::presets();
    for (int i = 0; i < p.size(); ++i) {
        if (p.at(i).ifLowHz == m_tv.ifLowHz
            && p.at(i).ifHighHz == m_tv.ifHighHz
            && p.at(i).offsetHz == m_tv.offsetHz) {
            treffer = m_tvVorgabe->findData(i);
            break;
        }
    }
    m_tvVorgabe->setCurrentIndex(treffer >= 0 ? treffer : 0);
    m_fuellt = false;

    m_tvText->setText(m_tv.describe());
}

void CompanionRigPage::speichereTransverter()
{
    m_tv.save();
    m_tvText->setText(m_tv.describe());
    emit transverterChanged(m_tv);
}

void CompanionRigPage::starteTest()
{
    speichereCat();

    QString host = m_host->text().trimmed();
    quint16 port = static_cast<quint16>(m_port->value());

    if (m_selbstStarten->isChecked()) {
        const int modell = gewaehltesModell();
        if (modell <= 0) {
            zeigeStatus(QStringLiteral(
                "Pick a rig first — without a Hamlib model number there is "
                "nothing to start."));
            return;
        }
        m_proc = std::make_unique<RigctldProcess>();
        QString fehler;
        if (!m_proc->start(modell, m_anschluss->text().trimmed(),
                           m_baud->currentData().toInt(), port, &fehler)) {
            zeigeStatus(fehler);
            m_proc.reset();
            return;
        }
        // Der Prozess darf auf einen anderen Port ausgewichen sein.
        host = QStringLiteral("127.0.0.1");
        port = m_proc->listenPort();
        connect(m_proc.get(), &RigctldProcess::exited, this,
                [this](int code, const QString& err) {
            zeigeStatus(QStringLiteral("rigctld stopped (%1): %2")
                            .arg(QString::number(code),
                                 RigctldProcess::grundAus(err)));
        });
    }

    m_client = std::make_unique<RigctldClient>();
    connect(m_client.get(), &RigctldClient::frequencyChanged, this,
            [this](qint64 hz) {
        const qint64 rf = m_tv.rfFrequencyHz(hz);
        QString text = QStringLiteral("Rig reads %1 MHz")
                           .arg(QString::number(hz / 1e6, 'f', 6));
        if (rf != hz) {
            text += QStringLiteral("  →  %1 MHz on the antenna")
                        .arg(QString::number(rf / 1e6, 'f', 6));
        }
        if (!m_client->mode().isEmpty()) {
            text += QStringLiteral("  (%1)").arg(m_client->mode());
        }
        zeigeStatus(text);
    });
    connect(m_client.get(), &RigctldClient::errorOccurred,
            this, &CompanionRigPage::zeigeStatus);
    connect(m_client.get(), &RigctldClient::stateChanged, this,
            [this](RigctldClient::State s) {
        if (s == RigctldClient::State::Connecting) {
            zeigeStatus(QStringLiteral("Connecting to %1…")
                            .arg(m_client->description()));
        }
    });

    m_client->setTarget(host, port);
    m_client->connectToRig();
    m_test->setText(QStringLiteral("Stop test"));
}

void CompanionRigPage::beendeTest()
{
    if (m_client) {
        m_client->disconnectFromRig();
        m_client.reset();
    }
    if (m_proc) {
        m_proc->stop();
        m_proc.reset();
    }
    if (m_test) { m_test->setText(QStringLiteral("Test")); }
}

void CompanionRigPage::zeigeStatus(const QString& text)
{
    if (m_status) { m_status->setText(text); }
}

} // namespace Longpath
