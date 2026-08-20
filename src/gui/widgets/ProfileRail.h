#pragma once

// =================================================================
// src/gui/widgets/ProfileRail.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Die Schiene ──────────────────────────────────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Wenn dies alles erledigt ist, soll die Möglichkeit entstehen, ein
//    zweites Profil oder auch ein drittes anzulegen, welches ich mir
//    selbst wieder individuell gestalten kann."
//
// Bei Zeus sind das runde Abzeichen am linken Rand — ein Buchstabe je
// Arbeitsfläche, darunter ein gestricheltes Plus. Genau so hier.
//
// ── Warum ein Buchstabe und nicht der Name ───────────────────────────
//
// Die Schiene ist 44 Pixel breit; ein Name passt nicht hinein. Das
// Abzeichen trägt den ersten Buchstaben, der volle Name steht im
// Tooltip und im Rechtsklickmenü. Das ist kein Kompromiss, sondern der
// Punkt: die Schiene soll man mit dem Augenwinkel lesen, nicht
// studieren.
//
// ── Was die Schiene NICHT tut ────────────────────────────────────────
//
// Sie schaltet nicht von selbst um. LayoutProfiles::profileFor() kann
// ein Profil an Band und Modus binden, und die Verdrahtung wäre ein
// Signalanschluss — sie bleibt aus, weil der Betreiber es so
// entschieden hat (2026-08-15): „Nein, nur von Hand. Kein Fenster, das
// sich beim Bandwechsel unter dir verändert."
//
// Die Fenstergröße gehört ebenfalls nicht dazu, aus demselben Grund:
// ein Profilwechsel soll Panels tauschen und nicht das Fenster
// umspringen lassen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QPushButton;

namespace Longpath {

class LayoutProfiles;

class ProfileRail : public QWidget {
    Q_OBJECT
public:
    explicit ProfileRail(LayoutProfiles* profiles, QWidget* parent = nullptr);

    /// Abzeichen neu aufbauen. Wird auf profilesChanged() gerufen.
    void rebuild();

    // ── Für Tests ────────────────────────────────────────────────────

    /// Die Namen der Abzeichen, von oben nach unten.
    QStringList badges() const;
    /// Der Name des hervorgehobenen Abzeichens, oder leer.
    QString activeBadge() const;
    /// Ein Abzeichen anklicken. false, wenn es das nicht gibt.
    bool clickBadge(const QString& name);
    /// Der Beschriftungsbuchstabe eines Namens — auch für Namen, die
    /// mit Leerzeichen oder einem Sonderzeichen anfangen.
    static QString initialFor(const QString& name);

    static constexpr int kWidth      = 44;
    static constexpr int kBadgeSide  = 30;

signals:
    /// Der Betreiber will ein neues Profil. Den Namen erfragt der
    /// Empfänger — die Schiene kennt keinen Dialog, damit sie ohne
    /// Oberfläche prüfbar bleibt.
    void newProfileRequested();
    void renameRequested(const QString& name);
    void duplicateRequested(const QString& name);
    void removeRequested(const QString& name);

private:
    void showMenuFor(const QString& name, const QPoint& globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Je Abzeichen ein Loeschkreuz, sichtbar beim Ueberfahren.
    QHash<QString, QPushButton*> m_closers;

    QPointer<LayoutProfiles> m_profiles;
    QVBoxLayout* m_column{nullptr};
    QPushButton* m_plus{nullptr};
    QHash<QString, QPushButton*> m_badges;
    QStringList m_order;
};

} // namespace Longpath
