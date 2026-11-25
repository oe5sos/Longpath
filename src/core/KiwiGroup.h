// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Entwurf B/C "Bis zu fuenf Kiwis" und "Wo werde
// ich gehoert" (2026-11-25).
//
// Die Mithoerer: Kiwis, die verbunden bleiben und beim Senden mitmessen,
// ob und wie stark sie die eigene Aussendung hoeren. Jeder hat seinen
// eigenen KiwiHearMeMeter. Reine Logik, ohne Netz und Oberflaeche.
//
// Zusammen mit dem Kiwi von "Mein Signal" (der Streifen) sind es hoechstens
// fuenf (kMaxTotal) -- fuer die oeffentlichen Server gilt "je Kiwi ein Kanal,
// hoeflich bleiben", also keine Schwarm-Abfrage.

#pragma once

#include "core/KiwiOwnSignal.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Longpath {

constexpr int kKiwiMaxTotal = 5;

struct KiwiGroupMember {
    QString id;
    QString name;
    double km{-1.0};
    double lat{0.0};
    double lon{0.0};
    bool hasPosition{false};
    KiwiHearMeMeter meter;
};

class KiwiGroup {
public:
    enum class AddResult { Added, AlreadyThere, Full };

    // Wie viele Mithoerer es hoechstens geben darf (kKiwiMaxTotal minus der
    // Kiwi von "Mein Signal", falls einer gewaehlt ist).
    void setCapacity(int capacity);
    int capacity() const { return m_capacity; }

    AddResult add(const KiwiGroupMember& member);
    bool remove(const QString& id);
    bool contains(const QString& id) const;
    void clear();
    int size() const { return m_members.size(); }
    const QVector<KiwiGroupMember>& members() const { return m_members; }
    QStringList ids() const;

    void setMarkHz(double hz);
    void txChanged(bool transmitting, qint64 nowMs);
    // false, wenn id kein Mithoerer ist.
    bool feedRow(const QString& id, const QVector<float>& binsDbm,
                 double lowHz, double highHz, qint64 nowMs);
    KiwiHearMeMeter::Result result(const QString& id) const;
    // Ein Urteil "schwach": gehoert, aber nur knapp ueber dem Rauschen.
    static constexpr double kWeakSnrDb = 12.0;
    static bool isWeak(const KiwiHearMeMeter::Result& r);

    struct Summary {
        int total{0};       // Kiwis insgesamt (mit Urteil oder ohne)
        int judged{0};      // davon mit Urteil (Hears oder Nothing)
        int hears{0};
        int weak{0};        // Teilmenge von hears
        int nothing{0};
        double farthestKm{-1.0};   // weitester, der mich hoert
    };
    static Summary summarize(const QVector<KiwiHearMeMeter::Result>& results,
                             const QVector<double>& kms);

private:
    int indexOf(const QString& id) const;

    int m_capacity{kKiwiMaxTotal};
    QVector<KiwiGroupMember> m_members;
};

} // namespace Longpath
