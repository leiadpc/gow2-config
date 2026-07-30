#pragma once

#include <QMainWindow>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QString>

#include "iniparser.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QTabWidget;
class QWidget;
class QCheckBox;
class QAction;
class QCloseEvent;

// (document name, section name) - used as a lookup key for cheat bookkeeping.
using DocSectionKey = QPair<QString, QString>;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialPath = QString(), QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // ---- data access helpers (mirror the Python properties/helpers) ----
    QStringList &curSectionOrder();
    QMap<QString, QVector<IniEntry>> &curSections();
    QString findSectionOwner(const QString &section) const;
    int findEntryIndexIn(const QString &docName, const QString &section, const QString &key) const;
    QString getRawValue(const QString &section, const QString &key, const QString &def = QString()) const;
    void setRawValue(const QString &section, const QString &key, const QString &value);
    void markDocDirty(const QString &docName, bool dirty);
    bool anyDirty() const;
    void updateWindowTitle();

    // ---- UI construction ----
    void buildUi();
    void buildActions();
    void buildMenuBar();
    QWidget *buildQuickSettingsTab();
    QWidget *makeQuickWidget(int specIndex);
    QWidget *buildCheatsTab();

    // ---- quick settings ----
    void onQuickSettingChanged(int specIndex, const QString &newValue);
    void refreshQuickSettings();

    // ---- cheats ----
    QVector<DocSectionKey> findCheatMatches(int specIndex) const;
    bool isCheatApplied(int specIndex) const;
    void refreshCheatsStatus();
    void onCheatToggled(int specIndex, bool checked);
    void toggleCheat(int specIndex, bool turnOn);
    void setCheatCheckbox(const QString &label, bool checked);

    // ---- tabs / file actions ----
    void onTabChanged(int index);
    void actionOpenFile();
    void actionOpenFolder();
    void loadDocuments(const QStringList &paths, const QString &sourceDesc);
    void actionSaveCurrent();
    void actionSaveAll();
    void actionSaveAs();
    void actionCloseCurrent();
    void saveDocument(const QString &name);
    bool confirmDiscardChanges();
    bool confirmDiscardForDoc(const QString &name);

    // ---- files / sections / rows ----
    void refreshFilesList();
    void onFileSelected(int row);
    void refreshSectionList();
    void onSectionSelected(int row);
    void onSectionRenamed(QListWidgetItem *item);
    void addSection();
    void deleteSection();
    void populateTable();
    void setRowItems(int row, const QString &col0, const QString &col1);
    void onTableItemChanged(QTableWidgetItem *item);
    void addRow();
    void addCommentRow();
    void deleteRows();
    void applyRowFilter();
    void runGlobalSearch();
    void jumpToSearchResult(QListWidgetItem *item);

private:
    QString baseTitle_;

    QMap<QString, IniDocument> documents_;
    QString currentDoc_;
    QString currentSection_;

    bool loadingQuickSettings_ = false;
    bool loadingCheats_ = false;

    // label -> { (docName, section) -> original value at the time the cheat was enabled }
    QMap<QString, QMap<DocSectionKey, QString>> cheatState_;

    // Quick Settings tab widgets, in the same order as quickSettings().
    QVector<QPair<int, QWidget *>> quickWidgets_;

    // Cheats tab widgets, in the same order as cheats().
    struct CheatWidgetEntry {
        int specIndex;
        QCheckBox *checkbox;
        QLabel *status;
    };
    QVector<CheatWidgetEntry> cheatWidgets_;

    // Left column
    QListWidget *filesList_ = nullptr;
    QLineEdit *sectionFilter_ = nullptr;
    QListWidget *sectionList_ = nullptr;
    QLineEdit *globalSearch_ = nullptr;
    QListWidget *searchResults_ = nullptr;

    // Right column
    QLabel *sectionTitle_ = nullptr;
    QLineEdit *rowFilter_ = nullptr;
    QTableWidget *table_ = nullptr;

    QTabWidget *tabs_ = nullptr;

    // Shared actions (wired once, used by both the toolbar and the File menu).
    QAction *openFileAct_ = nullptr;
    QAction *openFolderAct_ = nullptr;
    QAction *saveAct_ = nullptr;
    QAction *saveAllAct_ = nullptr;
    QAction *saveAsAct_ = nullptr;
    QAction *closeCurrentAct_ = nullptr;
    QAction *exitAct_ = nullptr;

    // Shortcut-only actions (not shown on the toolbar).
    QAction *deleteRowShortcutAct_ = nullptr;
    QAction *focusSearchShortcutAct_ = nullptr;
};
