#pragma once
// =================================================================
// src/gui/widgets/RxProfilePopup.h  (Longpath)
// =================================================================
//
// Longpath-original file. The receive-profile sheet behind the RX
// applet's gear: a list of the saved profiles, Load / Save / Delete
// for the selected one, and a name field with Save As for a new one.
//
// Qt::Popup like the TX applet's fine sheet -- it closes on a click
// beside it. That rules out modal boxes from inside it (the popup
// would close under them), so the new name is typed inline and the
// delete confirmation is an inline Yes/No row.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace Longpath {

class RxProfileManager;
class SliceModel;

class RxProfilePopup : public QWidget {
    Q_OBJECT
public:
    // `slice` is the slice the profiles are read from and written to
    // (the applet's current slice); may be null, then nothing is
    // enabled but Save As.
    RxProfilePopup(RxProfileManager* manager, SliceModel* slice, QWidget* parent = nullptr);

    void setSlice(SliceModel* slice);

    // Show under `anchor` (right-aligned with it), or at `globalPos`.
    void showBelow(QWidget* anchor);

    // For tests.
    QListWidget*  listForTest() const { return m_list; }
    QLineEdit*    nameEditForTest() const { return m_nameEdit; }
    QPushButton*  loadButtonForTest() const { return m_loadBtn; }
    QPushButton*  saveButtonForTest() const { return m_saveBtn; }
    QPushButton*  saveAsButtonForTest() const { return m_saveAsBtn; }
    QPushButton*  deleteButtonForTest() const { return m_deleteBtn; }
    QPushButton*  confirmYesForTest() const { return m_confirmYes; }
    QWidget*      confirmRowForTest() const { return m_confirmRow; }

public slots:
    void refresh();
    void loadSelected();
    void saveSelected();
    void saveAsNew();
    void deleteSelected();

private:
    void buildUi();
    QString selectedName() const;
    void updateButtons();
    void showConfirm(const QString& name);
    void hideConfirm();

    RxProfileManager* m_manager{nullptr};
    SliceModel*       m_slice{nullptr};
    QListWidget*      m_list{nullptr};
    QLineEdit*        m_nameEdit{nullptr};
    QPushButton*      m_loadBtn{nullptr};
    QPushButton*      m_saveBtn{nullptr};
    QPushButton*      m_saveAsBtn{nullptr};
    QPushButton*      m_deleteBtn{nullptr};
    QWidget*          m_confirmRow{nullptr};
    QLabel*           m_confirmLabel{nullptr};
    QPushButton*      m_confirmYes{nullptr};
    QPushButton*      m_confirmNo{nullptr};
    QLabel*           m_status{nullptr};
    QString           m_pendingDelete;
};

} // namespace Longpath
