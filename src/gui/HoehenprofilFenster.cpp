// no-port-check: Longpath-original, no Thetis logic.
//
// src/gui/HoehenprofilFenster.cpp  (Longpath) -- siehe .h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "gui/HoehenprofilFenster.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/HoehenprofilWidget.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace Longpath {

namespace {

constexpr auto kKeyHeim   = "ProfileAntennaHome";
constexpr auto kKeyGipfel = "ProfileAntennaSummit";
constexpr auto kKeyGegen  = "ProfileAntennaOther";

double gemerkt(const char* key, double vorgabe)
{
    bool ok = false;
    const double v = AppSettings::instance()
                         .value(QString::fromLatin1(key), QString::number(vorgabe))
                         .toString().toDouble(&ok);
    return ok && v >= 0.0 && v <= 500.0 ? v : vorgabe;
}

QString endeName(const SotaGipfelVerzeichnis::Ende& e)
{
    if (e.gipfel) {
        return e.g.name.isEmpty() ? e.g.ref : e.g.ref + QLatin1Char(' ') + e.g.name;
    }
    return e.locator;
}

QString endeHerkunft(const SotaGipfelVerzeichnis::Ende& e)
{
    if (e.gipfel) {
        return QStringLiteral("%1, %2, from the SOTA database (%3 m)")
            .arg(endeName(e), e.g.locator).arg(e.g.altM);
    }
    return QStringLiteral("centre of locator %1 (%2)")
        .arg(e.locator, Hoehenprofil::locatorGenauigkeit(e.locator));
}

} // namespace

double HoehenprofilFenster::vorgabeEigene(bool gipfel)
{
    // Spec 2026-11-11: eigene Antenne 10 m, portabel am Gipfel 5 m.
    return gipfel ? gemerkt(kKeyGipfel, 5.0) : gemerkt(kKeyHeim, 10.0);
}

double HoehenprofilFenster::vorgabeGegen()
{
    return gemerkt(kKeyGegen, 10.0);
}

HoehenprofilFenster::HoehenprofilFenster(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Path profile"));
    setStyleSheet(QStringLiteral("QDialog { background: %1; } QLabel { color: %2; }")
                      .arg(QLatin1String(Style::kPanelBg), QLatin1String(Style::kProfilTextLeise)));
    resize(1000, 440);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(6);

    m_profil = new HoehenprofilWidget(this);
    col->addWidget(m_profil, 1);

    auto* zeile = new QHBoxLayout;
    zeile->setSpacing(8);
    auto spin = [this](const QString& tip) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(0.0, 500.0);
        s->setDecimals(0);
        s->setSingleStep(1.0);
        s->setSuffix(QStringLiteral(" m"));
        s->setStyleSheet(Style::spinBoxStyle());
        s->setToolTip(tip);
        return s;
    };
    m_antA = spin(QStringLiteral(
        "Your antenna above ground. Remembered separately for summits (portable) and home."));
    m_antB = spin(QStringLiteral("The other station's antenna above ground."));
    zeile->addWidget(new QLabel(QStringLiteral("Your antenna"), this));
    zeile->addWidget(m_antA);
    zeile->addSpacing(10);
    zeile->addWidget(new QLabel(QStringLiteral("Their antenna"), this));
    zeile->addWidget(m_antB);
    zeile->addSpacing(14);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    zeile->addWidget(m_status, 1);
    auto* zu = m_zu = new QPushButton(QStringLiteral("Close"), this);
    zu->setStyleSheet(Style::buttonBaseStyle());
    connect(zu, &QPushButton::clicked, this, &QDialog::close);
    zeile->addWidget(zu);
    // Der Fokus nicht im Antennenfeld: sonst blinkt dort der Cursor, und
    // Return wuerde die Hoehe uebernehmen statt das Fenster zu schliessen.
    zu->setDefault(true);
    col->addLayout(zeile);

    m_quelle = new QLabel(this);
    m_quelle->setWordWrap(true);
    m_quelle->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: %2px; }")
                                .arg(QLatin1String(Style::kProfilTextLeise))
                                .arg(Style::kFontSmall));
    col->addWidget(m_quelle);

    auto geaendert = [this]() {
        if (m_setzt) { return; }
        AppSettings& s = AppSettings::instance();
        s.setValue(QString::fromLatin1(m_a.gipfel ? kKeyGipfel : kKeyHeim),
                   QString::number(m_antA->value()));
        s.setValue(QString::fromLatin1(kKeyGegen), QString::number(m_antB->value()));
        rechnen();
    };
    connect(m_antA, qOverload<double>(&QDoubleSpinBox::valueChanged), this, geaendert);
    connect(m_antB, qOverload<double>(&QDoubleSpinBox::valueChanged), this, geaendert);

    connect(&m_abruf, &HoehenAbruf::fertig, this,
            [this](const QString& key, const QVector<double>& h) {
        if (key != m_schluessel) { return; }
        m_hoehen = h;
        rechnen();
    });
    connect(&m_abruf, &HoehenAbruf::fehler, this,
            [this](const QString& key, const QString& text) {
        if (key == m_schluessel) { zeigeFehler(text); }
    });
    connect(&m_abruf, &HoehenAbruf::fortschritt, this, [this](int erledigt, int gesamt) {
        if (erledigt < gesamt) {
            m_status->setText(QStringLiteral("Fetching heights from OpenTopoData… %1 of %2 points")
                                  .arg(erledigt).arg(gesamt));
        }
    });

    SotaGipfelVerzeichnis& v = SotaGipfelVerzeichnis::instanz();
    connect(&v, &SotaGipfelVerzeichnis::gipfelBekannt, this,
            &HoehenprofilFenster::gipfelBekannt);
    connect(&v, &SotaGipfelVerzeichnis::gipfelFehler, this,
            [this](const QString& ref, const QString&) { gipfelBekannt(ref); });
}

void HoehenprofilFenster::showEvent(QShowEvent* ev)
{
    QDialog::showEvent(ev);
    m_zu->setFocus(Qt::OtherFocusReason);
}

void HoehenprofilFenster::setEntry(const LogEntry& e, const QString& ersatzLocator)
{
    m_entry = e;
    m_ersatz = ersatzLocator;
    setWindowTitle(QStringLiteral("Path profile — %1").arg(e.call.trimmed().toUpper()));
    neuAufbauen();
}

QString HoehenprofilFenster::statusText() const
{
    return m_status->text();
}

void HoehenprofilFenster::gipfelBekannt(const QString& ref)
{
    const QString r = SotaGipfelVerzeichnis::normalisiere(ref);
    if (r.isEmpty()) { return; }
    if (r == SotaGipfelVerzeichnis::normalisiere(m_entry.mySotaRef)
        || r == SotaGipfelVerzeichnis::normalisiere(m_entry.sotaRef)) {
        neuAufbauen();
    }
}

void HoehenprofilFenster::neuAufbauen()
{
    SotaGipfelVerzeichnis& v = SotaGipfelVerzeichnis::instanz();
    m_hatWeg = false;
    m_hoehen.clear();
    m_schluessel.clear();
    m_status->setStyleSheet(QString());

    const QString call = m_entry.call.trimmed().toUpper();
    const double f = Hoehenprofil::profilFrequenzMHz(m_entry.band, m_entry.freqMHz);
    if (f <= 0.0) {
        m_profil->setHinweis(call, QStringLiteral(
            "Path profiles are for 2 m, 70 cm and 23 cm contacts."));
        m_status->clear();
        m_quelle->clear();
        return;
    }

    // Ein Gipfel, der noch nicht nachgeschlagen ist: erst fragen und auf
    // die Antwort warten -- sonst holte das Fenster die Hoehen fuer den
    // Locator und gleich danach noch einmal fuer den Gipfel. Nach einer
    // Absage (unbekannt, kein Netz) gilt der Locator.
    QStringList offen, hinweise;
    for (const QString& ref : {m_entry.mySotaRef, m_entry.sotaRef}) {
        const QString r = SotaGipfelVerzeichnis::normalisiere(ref);
        SotaGipfel g;
        if (r.isEmpty() || v.finde(r, g)) { continue; }
        if (v.istUnbekannt(r)) {
            hinweise << QStringLiteral("%1 is not in the SOTA summit list").arg(r);
            continue;
        }
        v.anfragen(r);
        if (v.wartet(r)) {
            offen << r;
        } else if (!v.finde(r, g)) {
            hinweise << QStringLiteral("couldn't look up %1").arg(r);
        }
    }
    if (!offen.isEmpty()) {
        m_profil->setHinweis(call, QStringLiteral("Looking up %1 in the SOTA database…")
                                       .arg(offen.join(QStringLiteral(", "))));
        m_status->setText(QStringLiteral("Looking up summit…"));
        m_quelle->clear();
        return;
    }

    m_a = v.eigenesEnde(m_entry, m_ersatz);
    m_b = v.gegenEnde(m_entry);

    if (!m_b.gueltig) {
        m_profil->setHinweis(call, QStringLiteral(
            "No position for %1 — the profile needs their locator (GRIDSQUARE) "
            "or their summit (SOTA_REF).").arg(call));
        m_status->clear();
        m_quelle->clear();
        return;
    }
    if (!m_a.gueltig) {
        m_profil->setHinweis(call, QStringLiteral(
            "No position of your own for this contact — it needs MY_GRIDSQUARE "
            "or MY_SOTA_REF, or your locator under Setup."));
        m_status->clear();
        m_quelle->clear();
        return;
    }

    m_setzt = true;
    m_antA->setValue(vorgabeEigene(m_a.gipfel));
    m_antB->setValue(vorgabeGegen());
    m_setzt = false;

    const double km = Hoehenprofil::grosskreisKm(m_a.lat, m_a.lon, m_b.lat, m_b.lon);
    if (km < 0.5) {
        m_profil->setHinweis(call, QStringLiteral(
            "Both ends are at the same place — nothing to draw."));
        return;
    }
    m_weg = HoehenAbruf::Weg{m_a.lat, m_a.lon, m_b.lat, m_b.lon,
                             Hoehenprofil::punktzahlFuer(km)};
    m_hatWeg = true;
    m_schluessel = HoehenAbruf::schluessel(m_weg);

    QString quelle = QStringLiteral(
        "Heights: EU-DEM 25 m / SRTM 90 m via OpenTopoData, %1 points · Earth curvature "
        "with k = 4/3 · %2 MHz · dashed: horizon rays of both stations\n"
        "Your position: %3 · theirs: %4")
        .arg(m_weg.punkte).arg(f, 0, 'f', 3)
        .arg(endeHerkunft(m_a), endeHerkunft(m_b));
    if (!m_a.gipfel || !m_b.gipfel) {
        quelle += QStringLiteral("\nShaded: terrain within the accuracy of a locator — "
                                 "the station may stand anywhere there, so it counts "
                                 "neither for the verdict nor for the horizon rays.");
    }
    m_quelle->setText(quelle);

    m_profil->setHinweis(call, QStringLiteral("Fetching heights…"));
    m_status->setText(hinweise.isEmpty()
        ? QStringLiteral("Fetching heights from OpenTopoData…")
        : hinweise.join(QStringLiteral(" · ")));
    m_abruf.hole(m_weg);
}

void HoehenprofilFenster::rechnen()
{
    if (!m_hatWeg || m_hoehen.size() != m_weg.punkte) { return; }
    const int n = m_hoehen.size();
    const double D = Hoehenprofil::grosskreisKm(m_a.lat, m_a.lon, m_b.lat, m_b.lon);

    Hoehenprofil::Eingabe ein;
    ein.distKm.resize(n);
    for (int i = 0; i < n; ++i) { ein.distKm[i] = D * i / (n - 1); }
    ein.gelaendeM = m_hoehen;
    // Am Gipfel die Hoehe der SOTA-Datenbank: das Hoehenmodell glaettet
    // Spitzen (EU-DEM gibt am Gmundnerberg 874 m statt 884 m).
    if (m_a.gipfel && m_a.g.altM > 0) { ein.gelaendeM[0] = m_a.g.altM; }
    if (m_b.gipfel && m_b.g.altM > 0) { ein.gelaendeM[n - 1] = m_b.g.altM; }
    ein.antenneA = m_antA->value();
    ein.antenneB = m_antB->value();
    ein.freqMHz = Hoehenprofil::profilFrequenzMHz(m_entry.band, m_entry.freqMHz);
    // Ein Ende aus dem Locator steht irgendwo im Quadrat: das Gelaende
    // dicht an der Mitte zaehlt nicht (siehe Eingabe::unsicherAKm).
    ein.unsicherAKm = m_a.gipfel ? 0.0 : Hoehenprofil::locatorUnsicherheitKm(m_a.locator);
    ein.unsicherBKm = m_b.gipfel ? 0.0 : Hoehenprofil::locatorUnsicherheitKm(m_b.locator);

    const Hoehenprofil::Ergebnis erg = Hoehenprofil::rechne(ein);

    const QString call = m_entry.call.trimmed().toUpper();
    QStringList titel;
    titel << call + QStringLiteral("  ") + (m_b.gipfel ? m_b.g.ref : m_b.locator);
    titel << QStringLiteral("%1 km").arg(D, 0, 'f', 0);
    const QString mode = m_entry.mode.trimmed().toUpper();
    if (!mode.isEmpty()) { titel << mode; }
    if (m_entry.timeOn.isValid()) {
        titel << m_entry.timeOn.toUTC().date().toString(Qt::ISODate);
    }

    HoehenprofilWidget::Beschriftung text;
    text.titel = titel.join(QStringLiteral("   ·   "));
    auto hoehe = [](double v) {
        return std::isfinite(v) ? QString::number(v, 'f', 0) : QStringLiteral("?");
    };
    text.linksUnten = QStringLiteral("%1  %2 m").arg(endeName(m_a), hoehe(ein.gelaendeM.first()));
    text.rechtsUnten = QStringLiteral("%1  %2 m").arg(endeName(m_b), hoehe(ein.gelaendeM.last()));
    text.genauigkeit = m_b.gipfel ? QString()
                                  : Hoehenprofil::locatorGenauigkeit(m_b.locator);
    m_profil->setProfil(ein, erg, text);
    m_status->setText(Hoehenprofil::urteilKurz(erg.urteil));
    emit profilFertig();
}

void HoehenprofilFenster::zeigeFehler(const QString& text)
{
    m_profil->setHinweis(m_entry.call.trimmed().toUpper(),
                         QStringLiteral("Couldn't fetch the terrain heights.\n%1").arg(text));
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                .arg(QLatin1String(Style::kAmberWarn)));
}

} // namespace Longpath
