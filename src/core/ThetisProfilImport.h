// =================================================================
// src/core/ThetisProfilImport.h  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/database.cs [v2.10.3.15]
//     — 4299-4510   AddTXProfileTable: die 208 Spalten eines TX-Profils
//     — 9470, 9546  DataSet "Data", WriteXml(..., WriteSchema)
//     — 11263-11292 ExpandOldTxProfileTable: fehlende Spalten aus
//                   "Default" fuellen, die alte "Default"-Zeile NICHT
//                   uebernehmen
//   Project Files/Source/Console/setup.cs [v2.10.3.15]
//     — 12448-12520 ExportCurrentTxProfile: die Einzelausfuhr
//     — 3000-3120   isTXProfileSettingDifferent<>: welche Spalte an
//                   welchem Bedienelement haengt (die Belegstelle fuer
//                   JEDE Zuordnung in dieser Datei)
//     — 18959, 18965 die dB->linear-Umrechnung der DEXP-Verhaeltnisse
// Original licences from the Thetis sources are included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Erster Gang: der LESER. Zeus hat in 2.0.50
//                 angekuendigt, Thetis-TX-Profile einlesen zu koennen;
//                 Longpath ist selbst ein Thetis-Port, die Felder
//                 stehen also fast eins zu eins gegenueber. Diese
//                 Schicht kennt WEDER AppSettings NOCH die Oberflaeche:
//                 Dateiinhalt hinein, Profile und Hinweise heraus, am
//                 Schreibtisch vollstaendig pruefbar. Dieselbe Trennung
//                 wie bei CatProtocol und den Verstaerkeranbindungen.
//                 Reimplemented in C++20/Qt6, AI-assisted via Anthropic
//                 Claude Code.
// =================================================================
//
//=================================================================
// database.cs
//=================================================================
// Copyright (C) 2004-2009  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: FlexRadio Systems, 8900 Marybank Dr,
// Austin, TX 78750, USA.
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Longpath {

/// Ein eingelesenes Thetis-TX-Profil, schon auf Longpath-Feldnamen
/// gebracht.
struct ThetisProfil {
    QString name;
    /// Longpath-Feldname -> Wert, in der Schreibweise, die
    /// MicProfileManager erwartet (also "True"/"False" fuer Wahrheits-
    /// werte, Zahlen als Text).
    QHash<QString, QVariant> werte;
    /// Was bei DIESEM Profil angepasst oder weggelassen wurde, in
    /// Worten. Zeus macht es genauso ("you get a note for that profile
    /// telling you what was adjusted") -- und ohne das waere ein
    /// Einlesen eine Behauptung: der Betreiber sieht einen Profilnamen
    /// und weiss nicht, dass darin die halbe Kette fehlt.
    QStringList hinweise;
};

/// Thetis-TX-Profile aus einer Datei lesen.
///
/// Kennt weder AppSettings noch Oberflaeche. Das Schreiben ist ein
/// eigener Gang; dieser hier beantwortet nur: WAS steht in der Datei.
class ThetisProfilImport {
public:
    /// Welche Art Datei vor uns liegt.
    ///
    /// Beide Arten haben DIESELBE Form: Wurzel `<Data>`, darin ein
    /// eingebettetes XSD und je Profil ein `<TXProfile>`-Element
    /// (database.cs:9546 und setup.cs:12505 schreiben beide
    /// `WriteXml(..., XmlWriteMode.WriteSchema)`). Der Unterschied ist
    /// nur, wie viele TABELLEN darin stehen: die Einzelausfuhr wirft
    /// alle ausser TXProfile weg (setup.cs:12486-12497), die Sicherung
    /// behaelt alle.
    ///
    /// Erkannt wird am INHALT, nicht an der Endung -- Zeus: "Zeus
    /// recognizes the Thetis file on its own, even if it has been
    /// renamed."
    enum class Art {
        Unbekannt,
        Profilausfuhr,        ///< nur die TXProfile-Tabelle
        Datenbanksicherung    ///< die ganze Thetis-Datenbank
    };

    struct Ergebnis {
        Art art{Art::Unbekannt};
        QList<ThetisProfil> profile;
        /// Gruende, warum nichts (oder nicht alles) gelesen wurde.
        QStringList fehler;
        /// Hinweise, die die ganze DATEI betreffen, nicht ein Profil.
        QStringList hinweise;
    };

    /// Die Datei lesen. Gibt bei einer fremden Datei `Art::Unbekannt`
    /// und einen Grund zurueck, nie eine halbe Ausbeute.
    static Ergebnis lies(const QByteArray& inhalt);

    /// Die Abbildung Thetis-Spalte -> Longpath-Feldname. Leer heisst:
    /// diese Spalte wird NICHT uebernommen (und das erzeugt einen
    /// Hinweis, statt still zu verschwinden).
    static QString longpathFeld(const QString& thetisSpalte);

    /// Die Spalten, die absichtlich nicht uebernommen werden, mit
    /// Begruendung in Worten. Oeffentlich, weil die Oberflaeche sie
    /// zeigen soll und der Prueffall sie festnagelt.
    static QHash<QString, QString> nichtUebernommen();

    /// Wahrheitswerte: .NET schreibt "true"/"false" (XML-Schema,
    /// klein), Longpaths Einstellungen fuehren "True"/"False".
    /// Angenommen werden beide Schreibweisen und "1"/"0" -- die
    /// GENAUE Schreibweise einer echten Thetis-Ausfuhr ist hier nicht
    /// nachgemessen (es liegt keine auf diesem Rechner), darum
    /// nachsichtig statt streng.
    static QString wahrheitswert(const QString& roh, bool* ok = nullptr);

private:
    ThetisProfilImport() = delete;
};

} // namespace Longpath
