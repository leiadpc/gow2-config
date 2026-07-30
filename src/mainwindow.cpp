#include "mainwindow.h"
#include "quicksettings.h"
#include "cheats.h"

#include <QAction>
#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>

// ===========================================================================
// Construction
// ===========================================================================

MainWindow::MainWindow(const QString &initialPath, QWidget *parent)
    : QMainWindow(parent)
{
    baseTitle_ = "Gears of War 2 ini Editor";
    setWindowTitle(baseTitle_);
    resize(1200, 750);

    buildUi();
    buildActions();
    buildMenuBar();
    table_->addAction(deleteRowShortcutAct_);

    if (!initialPath.isEmpty()) {
        QFileInfo fi(initialPath);
        if (fi.isDir())
            loadDocuments(findGearIniFiles(initialPath), initialPath);
        else
            loadDocuments({initialPath}, initialPath);
    }
}

// ===========================================================================
// Data access helpers
// ===========================================================================

QStringList &MainWindow::curSectionOrder()
{
    static QStringList empty;
    if (currentDoc_.isEmpty() || !documents_.contains(currentDoc_)) {
        empty.clear();
        return empty;
    }
    return documents_[currentDoc_].sectionOrder;
}

QMap<QString, QVector<IniEntry>> &MainWindow::curSections()
{
    static QMap<QString, QVector<IniEntry>> empty;
    if (currentDoc_.isEmpty() || !documents_.contains(currentDoc_)) {
        empty.clear();
        return empty;
    }
    return documents_[currentDoc_].sections;
}

QString MainWindow::findSectionOwner(const QString &section) const
{
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        if (it.value().sections.contains(section))
            return it.key();
    }
    return QString();
}

int MainWindow::findEntryIndexIn(const QString &docName, const QString &section, const QString &key) const
{
    if (!documents_.contains(docName))
        return -1;
    const auto &sections = documents_[docName].sections;
    if (!sections.contains(section))
        return -1;
    const auto &entries = sections[section];
    for (int i = 0; i < entries.size(); ++i) {
        const QString trimmedKey = entries[i].key.trimmed();
        if (isCommentLine(trimmedKey))
            continue;
        if (trimmedKey.compare(key.trimmed(), Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QString MainWindow::getRawValue(const QString &section, const QString &key, const QString &def) const
{
    const QString owner = findSectionOwner(section);
    if (owner.isEmpty())
        return def;
    const int idx = findEntryIndexIn(owner, section, key);
    if (idx < 0)
        return def;
    return documents_[owner].sections[section][idx].value;
}

void MainWindow::setRawValue(const QString &section, const QString &key, const QString &value)
{
    QString owner = findSectionOwner(section);
    if (owner.isEmpty()) {
        if (documents_.isEmpty()) {
            QMessageBox::information(this, "No files loaded", "Open a file or folder first.");
            return;
        }
        owner = documents_.contains(preferredQuickSettingsFile())
                    ? preferredQuickSettingsFile()
                    : documents_.firstKey();
    }

    IniDocument &doc = documents_[owner];
    if (!doc.sections.contains(section)) {
        doc.sections.insert(section, {});
        doc.sectionOrder.append(section);
        if (owner == currentDoc_)
            refreshSectionList();
    }

    const int idx = findEntryIndexIn(owner, section, key);
    if (idx < 0)
        doc.sections[section].append(IniEntry{key, value});
    else
        doc.sections[section][idx].value = value;

    markDocDirty(owner, true);
    if (owner == currentDoc_ && currentSection_ == section) {
        populateTable();
        sectionTitle_->setText(QString("[%1]  (%2 entries)  -  %3")
                                    .arg(section)
                                    .arg(doc.sections[section].size())
                                    .arg(owner));
    }
}

void MainWindow::markDocDirty(const QString &docName, bool dirty)
{
    if (!documents_.contains(docName))
        return;
    documents_[docName].dirty = dirty;
    refreshFilesList();
    updateWindowTitle();
}

bool MainWindow::anyDirty() const
{
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        if (it.value().dirty)
            return true;
    }
    return false;
}

void MainWindow::updateWindowTitle()
{
    if (documents_.isEmpty()) {
        setWindowTitle(baseTitle_);
        return;
    }
    int dirtyCount = 0;
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        if (it.value().dirty)
            ++dirtyCount;
    }
    QString title = QString("%1 - %2 file(s) loaded").arg(baseTitle_).arg(documents_.size());
    if (dirtyCount > 0)
        title = QString("* %1 (%2 unsaved)").arg(title).arg(dirtyCount);
    setWindowTitle(title);
}

// ===========================================================================
// UI construction
// ===========================================================================

void MainWindow::buildActions()
{
    openFileAct_ = new QAction("Open File...", this);
    openFileAct_->setShortcut(QKeySequence::Open);
    connect(openFileAct_, &QAction::triggered, this, &MainWindow::actionOpenFile);

    openFolderAct_ = new QAction("Open Folder", this);
    openFolderAct_->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(openFolderAct_, &QAction::triggered, this, &MainWindow::actionOpenFolder);

    saveAct_ = new QAction("Save Current File", this);
    saveAct_->setShortcut(QKeySequence::Save);
    connect(saveAct_, &QAction::triggered, this, &MainWindow::actionSaveCurrent);

    saveAllAct_ = new QAction("Save All", this);
    saveAllAct_->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveAllAct_, &QAction::triggered, this, &MainWindow::actionSaveAll);

    saveAsAct_ = new QAction("Save Current As...", this);
    connect(saveAsAct_, &QAction::triggered, this, &MainWindow::actionSaveAs);

    closeCurrentAct_ = new QAction("Close Current File", this);
    closeCurrentAct_->setShortcut(QKeySequence::Close);
    connect(closeCurrentAct_, &QAction::triggered, this, &MainWindow::actionCloseCurrent);

    exitAct_ = new QAction("Exit", this);
    exitAct_->setShortcut(QKeySequence::Quit);
    connect(exitAct_, &QAction::triggered, this, &MainWindow::close);

    // Delete key removes the selected row(s) while the raw editor table has focus.
    deleteRowShortcutAct_ = new QAction(this);
    deleteRowShortcutAct_->setShortcut(QKeySequence::Delete);
    deleteRowShortcutAct_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteRowShortcutAct_, &QAction::triggered, this, &MainWindow::deleteRows);

    // Ctrl+F jumps to the global search box from anywhere in the window.
    focusSearchShortcutAct_ = new QAction(this);
    focusSearchShortcutAct_->setShortcut(QKeySequence::Find);
    focusSearchShortcutAct_->setShortcutContext(Qt::WindowShortcut);
    connect(focusSearchShortcutAct_, &QAction::triggered, this, [this]() {
        globalSearch_->setFocus();
        globalSearch_->selectAll();
    });
    addAction(focusSearchShortcutAct_);
}

void MainWindow::buildMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(openFileAct_);
    fileMenu->addAction(openFolderAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAct_);
    fileMenu->addAction(saveAllAct_);
    fileMenu->addAction(saveAsAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(closeCurrentAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct_);
}

QWidget *MainWindow::makeQuickWidget(int specIndex)
{
    const QuickSettingSpec &spec = quickSettings()[specIndex];

    switch (spec.kind) {
    case QuickKind::Int: {
        QSpinBox *w = new QSpinBox();
        w->setRange(spec.intMin, spec.intMax);
        connect(w, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, specIndex](int val) {
            onQuickSettingChanged(specIndex, QString::number(val));
        });
        return w;
    }
    case QuickKind::Float: {
        QDoubleSpinBox *w = new QDoubleSpinBox();
        w->setRange(spec.floatMin, spec.floatMax);
        w->setDecimals(2);
        connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, specIndex](double val) {
            onQuickSettingChanged(specIndex, QString::number(val, 'f', 6));
        });
        return w;
    }
    case QuickKind::Bool: {
        QCheckBox *w = new QCheckBox();
        connect(w, &QCheckBox::toggled, this, [this, specIndex](bool checked) {
            onQuickSettingChanged(specIndex, checked ? "True" : "False");
        });
        return w;
    }
    case QuickKind::Choice: {
        QComboBox *w = new QComboBox();
        for (int val : spec.choiceValues)
            w->addItem(QString::number(val), val);
        connect(w, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, specIndex, w](int idx) {
            onQuickSettingChanged(specIndex, QString::number(w->itemData(idx).toInt()));
        });
        return w;
    }
    case QuickKind::ChoiceLabel: {
        QComboBox *w = new QComboBox();
        for (const auto &pair : spec.choiceLabelValues)
            w->addItem(pair.first, pair.second);
        connect(w, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, specIndex, w](int idx) {
            onQuickSettingChanged(specIndex, QString::number(w->itemData(idx).toInt()));
        });
        return w;
    }
    }
    return new QWidget();
}

QWidget *MainWindow::buildQuickSettingsTab()
{
    QWidget *container = new QWidget();
    QVBoxLayout *outer = new QVBoxLayout(container);
    QLabel *intro = new QLabel(
        "Common settings, grouped for quick editing. Each field automatically\n"
        "finds whichever loaded file actually contains that section. Everything\n"
        "else is still available in the Raw Editor tab.");
    outer->addWidget(intro);

    QMap<QString, QGroupBox *> groups;
    const auto &specs = quickSettings();
    for (int i = 0; i < specs.size(); ++i) {
        const QuickSettingSpec &spec = specs[i];
        if (!groups.contains(spec.group)) {
            QGroupBox *box = new QGroupBox(spec.group);
            box->setLayout(new QFormLayout());
            groups.insert(spec.group, box);
            outer->addWidget(box);
        }
        QFormLayout *form = qobject_cast<QFormLayout *>(groups[spec.group]->layout());
        QWidget *widget = makeQuickWidget(i);
        form->addRow(spec.label, widget);
        quickWidgets_.append({i, widget});
    }

    outer->addStretch(1);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setWidget(container);
    return scroll;
}

void MainWindow::onQuickSettingChanged(int specIndex, const QString &newValue)
{
    if (loadingQuickSettings_)
        return;
    const QuickSettingSpec &spec = quickSettings()[specIndex];
    setRawValue(spec.section, spec.key, newValue);
}

void MainWindow::refreshQuickSettings()
{
    loadingQuickSettings_ = true;
    for (const auto &entry : quickWidgets_) {
        const QuickSettingSpec &spec = quickSettings()[entry.first];
        QWidget *widget = entry.second;
        const QString owner = findSectionOwner(spec.section);

        QString defaultStr;
        switch (spec.kind) {
        case QuickKind::Int: defaultStr = QString::number(spec.intDefault); break;
        case QuickKind::Float: defaultStr = QString::number(spec.floatDefault); break;
        case QuickKind::Bool: defaultStr = spec.boolDefault ? "True" : "False"; break;
        case QuickKind::Choice: defaultStr = QString::number(spec.choiceDefault); break;
        case QuickKind::ChoiceLabel: defaultStr = QString::number(spec.choiceLabelDefault); break;
        }
        const QString raw = getRawValue(spec.section, spec.key, defaultStr);

        if (!owner.isEmpty()) {
            widget->setToolTip(QString("%1.%2  (in %3)").arg(spec.section, spec.key, owner));
        } else {
            const QString targetFile = documents_.contains(preferredQuickSettingsFile())
                                            ? preferredQuickSettingsFile()
                                            : QString("the first loaded file");
            widget->setToolTip(QString("%1.%2  (not present yet - editing will create it in %3)")
                                    .arg(spec.section, spec.key, targetFile));
        }

        bool ok = false;
        switch (spec.kind) {
        case QuickKind::Int: {
            const double d = raw.toDouble(&ok);
            if (ok)
                qobject_cast<QSpinBox *>(widget)->setValue(static_cast<int>(d));
            break;
        }
        case QuickKind::Float: {
            const double d = raw.toDouble(&ok);
            if (ok)
                qobject_cast<QDoubleSpinBox *>(widget)->setValue(d);
            break;
        }
        case QuickKind::Bool: {
            const QString low = raw.trimmed().toLower();
            qobject_cast<QCheckBox *>(widget)->setChecked(low == "true" || low == "1");
            break;
        }
        case QuickKind::Choice:
        case QuickKind::ChoiceLabel: {
            const double d = raw.toDouble(&ok);
            if (ok) {
                QComboBox *combo = qobject_cast<QComboBox *>(widget);
                const int target = static_cast<int>(d);
                const int matchIdx = combo->findData(target);
                combo->setCurrentIndex(matchIdx >= 0 ? matchIdx : 0);
            }
            break;
        }
        }
    }
    loadingQuickSettings_ = false;
}

QWidget *MainWindow::buildCheatsTab()
{
    QWidget *container = new QWidget();
    QVBoxLayout *outer = new QVBoxLayout(container);
    outer->addWidget(new QLabel("Cheats apply immediately to the loaded file(s) in memory - remember to Save."));

    QGroupBox *box = new QGroupBox("Difficulty");
    QVBoxLayout *boxLayout = new QVBoxLayout(box);

    const auto &specs = cheats();
    for (int i = 0; i < specs.size(); ++i) {
        const CheatSpec &spec = specs[i];

        QCheckBox *cb = new QCheckBox(spec.label);
        connect(cb, &QCheckBox::toggled, this, [this, i](bool checked) {
            onCheatToggled(i, checked);
        });
        boxLayout->addWidget(cb);

        QLabel *note = new QLabel(spec.description);
        note->setWordWrap(true);
        note->setStyleSheet("color: gray; font-size: 11px;");
        boxLayout->addWidget(note);

        QLabel *status = new QLabel("");
        status->setWordWrap(true);
        status->setStyleSheet("color: gray; font-size: 11px; font-style: italic;");
        boxLayout->addWidget(status);

        cheatWidgets_.append({i, cb, status});
    }
    outer->addWidget(box);
    outer->addStretch(1);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setWidget(container);
    return scroll;
}

QVector<DocSectionKey> MainWindow::findCheatMatches(int specIndex) const
{
    const CheatSpec &spec = cheats()[specIndex];
    QVector<DocSectionKey> matches;
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        const QString &docName = it.key();
        const IniDocument &doc = it.value();
        for (const QString &sec : doc.sectionOrder) {
            if (!sec.contains(spec.sectionContains, Qt::CaseInsensitive))
                continue;
            if (findEntryIndexIn(docName, sec, spec.key) >= 0)
                matches.append({docName, sec});
        }
    }
    return matches;
}

bool MainWindow::isCheatApplied(int specIndex) const
{
    const CheatSpec &spec = cheats()[specIndex];
    const QVector<DocSectionKey> matches = findCheatMatches(specIndex);
    if (matches.isEmpty())
        return false;

    for (const auto &m : matches) {
        const int idx = findEntryIndexIn(m.first, m.second, spec.key);
        if (idx >= 0) {
            const QString currentVal = documents_[m.first].sections[m.second][idx].value;
            if (currentVal != spec.onValue)
                return false;
        }
    }
    return true;
}

void MainWindow::refreshCheatsStatus()
{
    for (const auto &entry : cheatWidgets_) {
        const CheatSpec &spec = cheats()[entry.specIndex];
        const QVector<DocSectionKey> matches = findCheatMatches(entry.specIndex);
        if (!matches.isEmpty()) {
            QSet<QString> filesSet;
            for (const auto &m : matches)
                filesSet.insert(m.first);
            QStringList files = filesSet.values();
            std::sort(files.begin(), files.end());
            entry.status->setText(QString("Found %1 in %2 section(s) across: %3")
                                       .arg(spec.key)
                                       .arg(matches.size())
                                       .arg(files.join(", ")));
        } else {
            entry.status->setText("Not found in any currently loaded file.");
        }
    }
}

void MainWindow::onCheatToggled(int specIndex, bool checked)
{
    if (loadingCheats_)
        return;
    toggleCheat(specIndex, checked);
}

void MainWindow::toggleCheat(int specIndex, bool turnOn)
{
    const CheatSpec &spec = cheats()[specIndex];
    const QString label = spec.label;
    bool affectedCurrentSection = false;

    if (turnOn) {
        const QVector<DocSectionKey> matches = findCheatMatches(specIndex);
        if (matches.isEmpty()) {
            QMessageBox::information(this, "Not found",
                QString("No sections containing '%1' with key '%2' were found in the "
                        "currently loaded file(s).").arg(spec.sectionContains, spec.key));
            setCheatCheckbox(label, false);
            return;
        }

        QMap<DocSectionKey, QString> originals;
        for (const auto &m : matches) {
            const int idx = findEntryIndexIn(m.first, m.second, spec.key);
            IniDocument &doc = documents_[m.first];
            originals.insert(m, doc.sections[m.second][idx].value);
            doc.sections[m.second][idx].value = spec.onValue;
            markDocDirty(m.first, true);
            if (m.first == currentDoc_ && m.second == currentSection_)
                affectedCurrentSection = true;
        }
        cheatState_[label] = originals;
    } else {
        const QMap<DocSectionKey, QString> originals = cheatState_.value(label);
        QMap<QString, QString> knownGoodLower;
        for (auto it = spec.knownGood.constBegin(); it != spec.knownGood.constEnd(); ++it)
            knownGoodLower.insert(it.key().toLower(), it.value());

        QMap<DocSectionKey, bool> targets;
        for (const auto &m : findCheatMatches(specIndex))
            targets.insert(m, true);
        for (auto it = originals.constBegin(); it != originals.constEnd(); ++it)
            targets.insert(it.key(), true);

        for (auto it = targets.constBegin(); it != targets.constEnd(); ++it) {
            const DocSectionKey &m = it.key();
            if (!documents_.contains(m.first) || !documents_[m.first].sections.contains(m.second))
                continue;
            const int idx = findEntryIndexIn(m.first, m.second, spec.key);
            if (idx < 0)
                continue;

            QString restoreVal;
            if (knownGoodLower.contains(m.second.toLower()))
                restoreVal = knownGoodLower[m.second.toLower()];
            else if (originals.contains(m))
                restoreVal = originals[m];
            else
                continue;

            documents_[m.first].sections[m.second][idx].value = restoreVal;
            markDocDirty(m.first, true);
            if (m.first == currentDoc_ && m.second == currentSection_)
                affectedCurrentSection = true;
        }
        cheatState_[label] = QMap<DocSectionKey, QString>();
    }

    if (affectedCurrentSection && curSections().contains(currentSection_)) {
        populateTable();
        sectionTitle_->setText(QString("[%1]  (%2 entries)  -  %3")
                                    .arg(currentSection_)
                                    .arg(curSections()[currentSection_].size())
                                    .arg(currentDoc_));
    }
    refreshQuickSettings();
    refreshCheatsStatus();
}

void MainWindow::setCheatCheckbox(const QString &label, bool checked)
{
    for (const auto &entry : cheatWidgets_) {
        if (cheats()[entry.specIndex].label == label) {
            loadingCheats_ = true;
            entry.checkbox->setChecked(checked);
            loadingCheats_ = false;
            break;
        }
    }
}

void MainWindow::buildUi()
{
    tabs_ = new QTabWidget();
    tabs_->addTab(buildQuickSettingsTab(), "Quick Settings");
    tabs_->addTab(buildCheatsTab(), "Cheats");

    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // ---- Left column ----
    QWidget *left = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    leftLayout->addWidget(new QLabel("Files"));
    filesList_ = new QListWidget();
    connect(filesList_, &QListWidget::currentRowChanged, this, &MainWindow::onFileSelected);
    leftLayout->addWidget(filesList_, 1);

    leftLayout->addWidget(new QLabel("Sections"));

    sectionFilter_ = new QLineEdit();
    sectionFilter_->setPlaceholderText("Filter sections...");
    connect(sectionFilter_, &QLineEdit::textChanged, this, [this](const QString &) { refreshSectionList(); });
    leftLayout->addWidget(sectionFilter_);

    sectionList_ = new QListWidget();
    connect(sectionList_, &QListWidget::currentRowChanged, this, &MainWindow::onSectionSelected);
    connect(sectionList_, &QListWidget::itemChanged, this, &MainWindow::onSectionRenamed);
    leftLayout->addWidget(sectionList_, 2);

    QHBoxLayout *secBtnRow = new QHBoxLayout();
    QPushButton *addSecBtn = new QPushButton("Add Section");
    connect(addSecBtn, &QPushButton::clicked, this, &MainWindow::addSection);
    QPushButton *delSecBtn = new QPushButton("Delete Section");
    connect(delSecBtn, &QPushButton::clicked, this, &MainWindow::deleteSection);
    secBtnRow->addWidget(addSecBtn);
    secBtnRow->addWidget(delSecBtn);
    leftLayout->addLayout(secBtnRow);

    leftLayout->addWidget(new QLabel("Global Search (all loaded files)"));
    globalSearch_ = new QLineEdit();
    globalSearch_->setPlaceholderText("Search key or value...");
    connect(globalSearch_, &QLineEdit::textChanged, this, [this](const QString &) { runGlobalSearch(); });
    leftLayout->addWidget(globalSearch_);

    searchResults_ = new QListWidget();
    connect(searchResults_, &QListWidget::itemDoubleClicked, this, &MainWindow::jumpToSearchResult);
    leftLayout->addWidget(searchResults_, 2);

    // ---- Right column ----
    QWidget *right = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 4, 4, 4);

    sectionTitle_ = new QLabel("No section selected");
    rightLayout->addWidget(sectionTitle_);

    rowFilter_ = new QLineEdit();
    rowFilter_->setPlaceholderText("Filter rows in this section...");
    connect(rowFilter_, &QLineEdit::textChanged, this, [this](const QString &) { applyRowFilter(); });
    rightLayout->addWidget(rowFilter_);

    table_ = new QTableWidget(0, 2);
    table_->setHorizontalHeaderLabels({"Key", "Value"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(table_, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    rightLayout->addWidget(table_, 1);

    QHBoxLayout *rowBtnRow = new QHBoxLayout();
    QPushButton *addRowBtn = new QPushButton("Add Row");
    connect(addRowBtn, &QPushButton::clicked, this, &MainWindow::addRow);
    QPushButton *addCommentBtn = new QPushButton("Add Comment Row");
    connect(addCommentBtn, &QPushButton::clicked, this, &MainWindow::addCommentRow);
    QPushButton *delRowBtn = new QPushButton("Delete Selected Row(s)");
    connect(delRowBtn, &QPushButton::clicked, this, &MainWindow::deleteRows);
    rowBtnRow->addWidget(addRowBtn);
    rowBtnRow->addWidget(addCommentBtn);
    rowBtnRow->addWidget(delRowBtn);
    rightLayout->addLayout(rowBtnRow);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({340, 780});

    tabs_->addTab(splitter, "Raw Editor");
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    setCentralWidget(tabs_);
    setStatusBar(new QStatusBar());
}

void MainWindow::onTabChanged(int index)
{
    if (index == 0)
        refreshQuickSettings();
    else if (index == 1)
        refreshCheatsStatus();
}

// ===========================================================================
// File actions
// ===========================================================================

void MainWindow::actionOpenFile()
{
    if (!confirmDiscardChanges())
        return;
    QSettings settings;
    const QString startDir = settings.value("lastDir").toString();
    const QString path = QFileDialog::getOpenFileName(this, "Open ini file", startDir,
                                                        "INI Files (*.ini);;All Files (*)");
    if (!path.isEmpty()) {
        settings.setValue("lastDir", QFileInfo(path).absolutePath());
        loadDocuments({path}, path);
    }
}

void MainWindow::actionOpenFolder()
{
    if (!confirmDiscardChanges())
        return;
    QSettings settings;
    const QString startDir = settings.value("lastDir").toString();
    const QString folder = QFileDialog::getExistingDirectory(this, "Select folder containing Gear*.ini files", startDir);
    if (folder.isEmpty())
        return;
    const QStringList matches = findGearIniFiles(folder);
    if (matches.isEmpty()) {
        QMessageBox::information(this, "No files found",
                                  QString("No Gear*.ini files were found in:\n%1").arg(folder));
        return;
    }
    settings.setValue("lastDir", folder);
    loadDocuments(matches, folder);
}

void MainWindow::loadDocuments(const QStringList &paths, const QString &sourceDesc)
{
    documents_.clear();
    currentDoc_.clear();
    currentSection_.clear();
    cheatState_.clear();

    QStringList errors;
    for (const QString &path : paths) {
        QFileInfo fi(path);
        QStringList sectionOrder;
        QMap<QString, QVector<IniEntry>> sections;
        QString err;
        if (!parseIni(path, sectionOrder, sections, err)) {
            errors.append(QString("%1: %2").arg(fi.fileName(), err));
            continue;
        }
        IniDocument doc;
        doc.fileName = fi.fileName();
        doc.path = path;
        doc.sectionOrder = sectionOrder;
        doc.sections = sections;
        doc.dirty = false;
        documents_.insert(doc.fileName, doc);
    }

    if (!errors.isEmpty())
        QMessageBox::warning(this, "Some files failed to load", errors.join("\n"));

    sectionFilter_->clear();
    globalSearch_->clear();
    searchResults_->clear();
    refreshFilesList();

    if (!documents_.isEmpty())
        filesList_->setCurrentRow(0);

    int totalSections = 0;
    int totalEntries = 0;
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        totalSections += it.value().sectionOrder.size();
        for (auto sit = it.value().sections.constBegin(); sit != it.value().sections.constEnd(); ++sit)
            totalEntries += sit.value().size();
    }
    statusBar()->showMessage(QString("Loaded %1 file(s) from %2 (%3 sections, %4 entries total)")
                                  .arg(documents_.size())
                                  .arg(sourceDesc)
                                  .arg(totalSections)
                                  .arg(totalEntries));
    updateWindowTitle();
    refreshQuickSettings();

    for (const auto &entry : cheatWidgets_) {
        loadingCheats_ = true;
        entry.checkbox->setChecked(isCheatApplied(entry.specIndex));
        loadingCheats_ = false;
    }
    refreshCheatsStatus();
}

void MainWindow::actionSaveCurrent()
{
    if (currentDoc_.isEmpty()) {
        QMessageBox::information(this, "No file selected", "Select a file to save first.");
        return;
    }
    saveDocument(currentDoc_);
}

void MainWindow::actionSaveAll()
{
    if (documents_.isEmpty())
        return;
    QStringList dirtyNames;
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        if (it.value().dirty)
            dirtyNames.append(it.key());
    }
    if (dirtyNames.isEmpty()) {
        statusBar()->showMessage("Nothing to save - no unsaved changes.");
        return;
    }
    for (const QString &name : dirtyNames)
        saveDocument(name);
}

void MainWindow::actionSaveAs()
{
    if (currentDoc_.isEmpty()) {
        QMessageBox::information(this, "No file selected", "Select a file to save first.");
        return;
    }
    const IniDocument doc = documents_[currentDoc_];
    const QString path = QFileDialog::getSaveFileName(this, "Save ini file as", doc.path,
                                                        "INI Files (*.ini);;All Files (*)");
    if (path.isEmpty())
        return;

    QString err;
    if (!writeIni(path, doc.sectionOrder, doc.sections, err)) {
        QMessageBox::critical(this, "Error", QString("Could not save file:\n%1").arg(err));
        return;
    }

    const QString oldName = currentDoc_;
    const QFileInfo fi(path);
    const QString newName = fi.fileName();

    IniDocument updated = doc;
    updated.path = path;
    updated.dirty = false;
    updated.fileName = newName;

    if (newName != oldName) {
        documents_.remove(oldName);
        documents_.insert(newName, updated);
        currentDoc_ = newName;
    } else {
        documents_[oldName] = updated;
    }

    refreshFilesList();
    updateWindowTitle();
    statusBar()->showMessage(QString("Saved to %1").arg(path));
}

void MainWindow::saveDocument(const QString &name)
{
    IniDocument &doc = documents_[name];
    QString err;
    if (!writeIni(doc.path, doc.sectionOrder, doc.sections, err)) {
        QMessageBox::critical(this, "Error", QString("Could not save %1:\n%2").arg(name, err));
        return;
    }
    doc.dirty = false;
    refreshFilesList();
    updateWindowTitle();
    statusBar()->showMessage(QString("Saved %1").arg(doc.path));
}

bool MainWindow::confirmDiscardChanges()
{
    if (!anyDirty())
        return true;
    const QMessageBox::StandardButton resp = QMessageBox::question(
        this, "Unsaved changes", "You have unsaved changes in one or more files. Discard them?",
        QMessageBox::Yes | QMessageBox::Cancel);
    return resp == QMessageBox::Yes;
}

bool MainWindow::confirmDiscardForDoc(const QString &name)
{
    if (!documents_.contains(name) || !documents_[name].dirty)
        return true;
    const QMessageBox::StandardButton resp = QMessageBox::question(
        this, "Unsaved changes", QString("%1 has unsaved changes. Discard them?").arg(name),
        QMessageBox::Yes | QMessageBox::Cancel);
    return resp == QMessageBox::Yes;
}

void MainWindow::actionCloseCurrent()
{
    if (currentDoc_.isEmpty()) {
        QMessageBox::information(this, "No file selected", "Select a file to close first.");
        return;
    }
    const QString name = currentDoc_;
    if (!confirmDiscardForDoc(name))
        return;

    documents_.remove(name);
    currentDoc_.clear();
    currentSection_.clear();

    refreshFilesList();
    if (!documents_.isEmpty()) {
        filesList_->setCurrentRow(0);
    } else {
        sectionList_->clear();
        table_->setRowCount(0);
        sectionTitle_->setText("No section selected");
        searchResults_->clear();
    }
    updateWindowTitle();
    statusBar()->showMessage(QString("Closed %1").arg(name));
    refreshQuickSettings();
    refreshCheatsStatus();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardChanges())
        event->accept();
    else
        event->ignore();
}

// ===========================================================================
// Files / sections / rows
// ===========================================================================

void MainWindow::refreshFilesList()
{
    filesList_->blockSignals(true);
    filesList_->clear();
    for (auto it = documents_.constBegin(); it != documents_.constEnd(); ++it) {
        const QString label = it.value().dirty ? "* " + it.key() : it.key();
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, it.key());
        filesList_->addItem(item);
    }
    filesList_->blockSignals(false);
}

void MainWindow::onFileSelected(int row)
{
    QListWidgetItem *item = filesList_->item(row);
    const QString name = item ? item->data(Qt::UserRole).toString() : QString();
    if (name.isEmpty() || !documents_.contains(name)) {
        currentDoc_.clear();
        currentSection_.clear();
        sectionList_->clear();
        table_->setRowCount(0);
        sectionTitle_->setText("No section selected");
        return;
    }
    currentDoc_ = name;
    currentSection_.clear();
    sectionFilter_->clear();
    refreshSectionList();
    if (!curSectionOrder().isEmpty()) {
        sectionList_->setCurrentRow(0);
    } else {
        table_->setRowCount(0);
        sectionTitle_->setText("No section selected");
    }
}

void MainWindow::refreshSectionList()
{
    const QString filt = sectionFilter_->text().trimmed().toLower();
    sectionList_->blockSignals(true);
    sectionList_->clear();
    for (const QString &name : curSectionOrder()) {
        if (!filt.isEmpty() && !name.toLower().contains(filt))
            continue;
        QListWidgetItem *item = new QListWidgetItem(name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        sectionList_->addItem(item);
    }
    sectionList_->blockSignals(false);
}

void MainWindow::onSectionSelected(int row)
{
    QListWidgetItem *item = sectionList_->item(row);
    const QString name = item ? item->text() : QString();
    if (name.isEmpty() || currentDoc_.isEmpty() || !curSections().contains(name)) {
        currentSection_.clear();
        table_->setRowCount(0);
        sectionTitle_->setText("No section selected");
        return;
    }
    currentSection_ = name;
    sectionTitle_->setText(QString("[%1]  (%2 entries)  -  %3")
                                .arg(name)
                                .arg(curSections()[name].size())
                                .arg(currentDoc_));
    rowFilter_->clear();
    populateTable();
}

void MainWindow::onSectionRenamed(QListWidgetItem *item)
{
    if (currentDoc_.isEmpty())
        return;
    const QString newName = item->text().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(this, "Invalid name", "Section name cannot be empty.");
        refreshSectionList();
        return;
    }

    QStringList currentTexts;
    for (int i = 0; i < sectionList_->count(); ++i)
        currentTexts.append(sectionList_->item(i)->text());

    QString oldName;
    for (const QString &n : curSectionOrder()) {
        if (!currentTexts.contains(n)) {
            oldName = n;
            break;
        }
    }
    if (oldName.isEmpty() || oldName == newName)
        return;
    if (curSections().contains(newName)) {
        QMessageBox::warning(this, "Duplicate name", QString("Section '%1' already exists.").arg(newName));
        refreshSectionList();
        return;
    }

    IniDocument &doc = documents_[currentDoc_];
    const int idx = doc.sectionOrder.indexOf(oldName);
    doc.sectionOrder[idx] = newName;
    doc.sections.insert(newName, doc.sections.take(oldName));
    if (currentSection_ == oldName)
        currentSection_ = newName;
    markDocDirty(currentDoc_, true);
    refreshSectionList();
}

void MainWindow::addSection()
{
    if (currentDoc_.isEmpty()) {
        QMessageBox::information(this, "No file selected", "Select a file first.");
        return;
    }
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Section", "Section name (without brackets):",
                                          QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    name = name.trimmed();
    if (curSections().contains(name)) {
        QMessageBox::warning(this, "Duplicate name", QString("Section '%1' already exists.").arg(name));
        return;
    }
    curSectionOrder().append(name);
    curSections().insert(name, {});
    markDocDirty(currentDoc_, true);
    refreshSectionList();
    for (int i = 0; i < sectionList_->count(); ++i) {
        if (sectionList_->item(i)->text() == name) {
            sectionList_->setCurrentRow(i);
            break;
        }
    }
}

void MainWindow::deleteSection()
{
    if (currentDoc_.isEmpty() || currentSection_.isEmpty())
        return;
    const QMessageBox::StandardButton resp = QMessageBox::question(
        this, "Delete section",
        QString("Delete section [%1] from %2 and all its entries?").arg(currentSection_, currentDoc_));
    if (resp != QMessageBox::Yes)
        return;
    const QString name = currentSection_;
    curSectionOrder().removeAll(name);
    curSections().remove(name);
    currentSection_.clear();
    markDocDirty(currentDoc_, true);
    refreshSectionList();
    if (!curSectionOrder().isEmpty()) {
        sectionList_->setCurrentRow(0);
    } else {
        table_->setRowCount(0);
        sectionTitle_->setText("No section selected");
    }
}

void MainWindow::populateTable()
{
    table_->blockSignals(true);
    table_->setRowCount(0);
    if (!currentDoc_.isEmpty() && !currentSection_.isEmpty() && curSections().contains(currentSection_)) {
        const auto &entries = curSections()[currentSection_];
        table_->setRowCount(entries.size());
        for (int row = 0; row < entries.size(); ++row)
            setRowItems(row, entries[row].key, entries[row].value);
    }
    table_->blockSignals(false);
    applyRowFilter();
}

void MainWindow::setRowItems(int row, const QString &col0, const QString &col1)
{
    const bool isComment = isCommentLine(col0.trimmed());
    QTableWidgetItem *item0 = new QTableWidgetItem(col0);
    QTableWidgetItem *item1 = new QTableWidgetItem(isComment ? QString() : col1);
    if (isComment) {
        const QBrush gray(QColor(120, 120, 120));
        item0->setForeground(gray);
        item1->setFlags(item1->flags() & ~Qt::ItemIsEditable);
    }
    table_->setItem(row, 0, item0);
    table_->setItem(row, 1, item1);
}

void MainWindow::onTableItemChanged(QTableWidgetItem *item)
{
    if (currentDoc_.isEmpty() || currentSection_.isEmpty() || !curSections().contains(currentSection_))
        return;
    const int row = item->row();
    auto &entries = curSections()[currentSection_];
    if (row >= entries.size())
        return;
    QTableWidgetItem *col0Item = table_->item(row, 0);
    QTableWidgetItem *col1Item = table_->item(row, 1);
    const QString col0 = col0Item ? col0Item->text() : QString();
    const QString col1 = col1Item ? col1Item->text() : QString();
    entries[row].key = col0;
    entries[row].value = col1;

    table_->blockSignals(true);
    setRowItems(row, col0, col1);
    table_->blockSignals(false);

    markDocDirty(currentDoc_, true);
    sectionTitle_->setText(QString("[%1]  (%2 entries)  -  %3")
                                .arg(currentSection_)
                                .arg(entries.size())
                                .arg(currentDoc_));
}

void MainWindow::addRow()
{
    if (currentDoc_.isEmpty() || currentSection_.isEmpty() || !curSections().contains(currentSection_)) {
        QMessageBox::information(this, "No section", "Select or create a section first.");
        return;
    }
    auto &entries = curSections()[currentSection_];
    entries.append(IniEntry{"NewKey", ""});
    const int row = entries.size() - 1;
    table_->blockSignals(true);
    table_->setRowCount(entries.size());
    setRowItems(row, "NewKey", "");
    table_->blockSignals(false);
    table_->scrollToBottom();
    table_->editItem(table_->item(row, 0));
    markDocDirty(currentDoc_, true);
    applyRowFilter();
}

void MainWindow::addCommentRow()
{
    if (currentDoc_.isEmpty() || currentSection_.isEmpty() || !curSections().contains(currentSection_)) {
        QMessageBox::information(this, "No section", "Select or create a section first.");
        return;
    }
    auto &entries = curSections()[currentSection_];
    entries.append(IniEntry{"; comment", ""});
    const int row = entries.size() - 1;
    table_->blockSignals(true);
    table_->setRowCount(entries.size());
    setRowItems(row, "; comment", "");
    table_->blockSignals(false);
    table_->scrollToBottom();
    table_->editItem(table_->item(row, 0));
    markDocDirty(currentDoc_, true);
    applyRowFilter();
}

void MainWindow::deleteRows()
{
    if (currentDoc_.isEmpty() || currentSection_.isEmpty() || !curSections().contains(currentSection_))
        return;
    QSet<int> rowsSet;
    const auto selected = table_->selectionModel()->selectedIndexes();
    for (const auto &idx : selected)
        rowsSet.insert(idx.row());
    if (rowsSet.isEmpty())
        return;
    QList<int> rows = rowsSet.values();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    auto &entries = curSections()[currentSection_];
    for (int row : rows) {
        if (row >= 0 && row < entries.size())
            entries.remove(row);
    }
    populateTable();
    markDocDirty(currentDoc_, true);
    sectionTitle_->setText(QString("[%1]  (%2 entries)  -  %3")
                                .arg(currentSection_)
                                .arg(entries.size())
                                .arg(currentDoc_));
}

void MainWindow::applyRowFilter()
{
    const QString filt = rowFilter_->text().trimmed().toLower();
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (filt.isEmpty()) {
            table_->setRowHidden(row, false);
            continue;
        }
        QTableWidgetItem *keyItem = table_->item(row, 0);
        QTableWidgetItem *valItem = table_->item(row, 1);
        const QString text = ((keyItem ? keyItem->text() : QString()) + " " +
                               (valItem ? valItem->text() : QString()))
                                  .toLower();
        table_->setRowHidden(row, !text.contains(filt));
    }
}

void MainWindow::runGlobalSearch()
{
    const QString query = globalSearch_->text().trimmed().toLower();
    searchResults_->clear();
    if (query.isEmpty())
        return;

    int count = 0;
    for (auto docIt = documents_.constBegin(); docIt != documents_.constEnd(); ++docIt) {
        const QString &docName = docIt.key();
        const IniDocument &doc = docIt.value();
        for (const QString &sec : doc.sectionOrder) {
            const auto &entries = doc.sections[sec];
            for (int row = 0; row < entries.size(); ++row) {
                const QString &col0 = entries[row].key;
                const QString &col1 = entries[row].value;
                if (col0.toLower().contains(query) || col1.toLower().contains(query)) {
                    const bool isComment = isCommentLine(col0.trimmed());
                    const QString label = isComment
                                               ? QString("[%1] [%2]  %3").arg(docName, sec, col0)
                                               : QString("[%1] [%2]  %3=%4").arg(docName, sec, col0, col1);
                    QListWidgetItem *item = new QListWidgetItem(label);
                    item->setData(Qt::UserRole, QVariant(QVariantList{docName, sec, row}));
                    searchResults_->addItem(item);
                    ++count;
                    if (count >= 500)
                        return;
                }
            }
        }
    }
}

void MainWindow::jumpToSearchResult(QListWidgetItem *item)
{
    const QVariantList data = item->data(Qt::UserRole).toList();
    if (data.size() != 3)
        return;
    const QString docName = data[0].toString();
    const QString sec = data[1].toString();
    const int row = data[2].toInt();

    for (int i = 0; i < filesList_->count(); ++i) {
        if (filesList_->item(i)->data(Qt::UserRole).toString() == docName) {
            filesList_->setCurrentRow(i);
            break;
        }
    }

    sectionFilter_->clear();
    for (int i = 0; i < sectionList_->count(); ++i) {
        if (sectionList_->item(i)->text() == sec) {
            sectionList_->setCurrentRow(i);
            break;
        }
    }

    rowFilter_->clear();
    if (row >= 0 && row < table_->rowCount()) {
        table_->selectRow(row);
        table_->scrollToItem(table_->item(row, 0));
    }
}
