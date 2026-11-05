// WindowPlacementBruecke.h -- das Raster fuer Bauteile OHNE Longpaths
// Include-Pfad (2026-11-05)
//
// Die Contest-Bruecke (src/contest) darf `src` nicht auf dem Include-Pfad
// haben: das Contestprogramm hat Koepfe gleichen Namens (core/
// RigctldClient.h, core/DxClusterClient.h), und Longpaths eigene gingen
// vor. WindowPlacement.h zieht gui/StyleConstants.h nach -- darum hier
// ein Kopf, der NICHTS nachzieht und nur die zwei Dinge verspricht, die
// ein fremdes schwebendes Fenster braucht. Rumpf in WindowPlacement.cpp.
#pragma once

class QWidget;

namespace Longpath {

/// Was PanFloatingWindow::moveEvent/resizeEvent tun: gedaempft aufs
/// Raster (snapToGridAfterSettle mit den Vorgaben) und den Zug melden
/// (meldeZug) -- fuer den Schutzrahmen.
void rasterWieSchwebendesFenster(QWidget* w);

/// Siehe WindowPlacement.h: lebt gerade eine EigeneBewegung (Raster,
/// Profil, Mitziehen), ist eine Lageaenderung kein Zug des Bedieners.
bool eigeneBewegungLebt();

} // namespace Longpath
