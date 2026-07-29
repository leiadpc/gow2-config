#!/usr/bin/env python3
"""
GearEngine.ini Editor
----------------------
A PySide6 GUI for editing Unreal Engine 3 -style .ini files (like GearEngine.ini)
that contain repeated keys per section (arrays), "+Key=" append syntax, and
comment lines. Standard Python configparser can't round-trip this format
(it chokes on duplicate keys), so this tool uses a small custom parser that
preserves section order, duplicate keys, and comments.

Usage:
    python gearengine_ini_editor.py [path/to/GearEngine.ini | path/to/folder]

Passing a folder loads every Gear*.ini file inside it (same as using the
"Open Folder..." toolbar button).

Features:
    - Load a single ini file, or an entire folder of Gear*.ini files at once
    - Files list on the left lets you switch between loaded documents
    - Section list per document (filterable, add/remove/rename)
    - Key/Value table per section (add/remove rows)
    - Duplicate keys are fully supported (each is its own row)
    - Comment lines (";" or "#") are preserved as editable rows
    - Global search across every loaded file/section/key/value, with jump-to-result
    - Quick Settings tab: curated form for common settings (resolution, AA,
      anisotropy, post-process toggles, shadow/LOD bias...) that automatically
      finds whichever loaded file actually contains that section
    - Save current file, Save All, or Save As, with a per-file unsaved indicator
"""

import re
import sys
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence, QColor, QBrush
from PySide6.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QHBoxLayout,
    QVBoxLayout,
    QFormLayout,
    QGroupBox,
    QScrollArea,
    QTabWidget,
    QSplitter,
    QListWidget,
    QListWidgetItem,
    QTableWidget,
    QTableWidgetItem,
    QPushButton,
    QLineEdit,
    QLabel,
    QSpinBox,
    QDoubleSpinBox,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QMessageBox,
    QInputDialog,
    QAbstractItemView,
    QHeaderView,
    QStatusBar,
    QToolBar,
)

COMMENT_PREFIXES = (";", "#")

# --------------------------------------------------------------------------
# Curated "Quick Settings" definitions.
# --------------------------------------------------------------------------
QUICK_SETTINGS = [
    dict(label="Resolution Width (ResX)", section="SystemSettings", key="ResX",
         kind="int", default=1280, range=(320, 7680), group="Display"),
    dict(label="Resolution Height (ResY)", section="SystemSettings", key="ResY",
         kind="int", default=720, range=(240, 4320), group="Display"),
    dict(label="Fullscreen", section="SystemSettings", key="Fullscreen",
         kind="bool", default=False, group="Display"),
    dict(label="Startup Fullscreen", section="WinDrv.WindowsClient", key="StartupFullscreen",
         kind="bool", default=False, group="Display"),
    dict(label="VSync", section="SystemSettings", key="UseVsync",
         kind="bool", default=False, group="Display"),
    dict(label="Screen Percentage", section="SystemSettings", key="ScreenPercentage",
         kind="float", default=100.0, range=(10.0, 200.0), group="Display"),
    dict(label="Max Smoothed Framerate", section="Engine.GameEngine", key="MaxSmoothedFrameRate",
         kind="int", default=60, range=(0, 300), group="Display"),

    dict(label="Max Anisotropy", section="SystemSettings", key="MaxAnisotropy",
         kind="choice", default=4, choices=[0, 1, 2, 4, 8, 16], group="Quality"),
    dict(label="Max Multisamples (AA)", section="SystemSettings", key="MaxMultisamples",
         kind="choice", default=1, choices=[1, 2, 4, 8], group="Quality"),
    dict(label="Detail Mode", section="SystemSettings", key="DetailMode",
         kind="choice_label", default=2,
         choices=[("Low", 0), ("Medium", 1), ("High", 2)], group="Quality"),

    dict(label="Motion Blur", section="SystemSettings", key="MotionBlur",
         kind="bool", default=False, group="Effects"),
    dict(label="Depth of Field", section="SystemSettings", key="DepthOfField",
         kind="bool", default=True, group="Effects"),
    dict(label="Ambient Occlusion", section="SystemSettings", key="AmbientOcclusion",
         kind="bool", default=True, group="Effects"),
    dict(label="Bloom", section="SystemSettings", key="Bloom",
         kind="bool", default=True, group="Effects"),
    dict(label="High Quality Bloom", section="SystemSettings", key="UseHighQualityBloom",
         kind="bool", default=True, group="Effects"),
    dict(label="Distortion", section="SystemSettings", key="Distortion",
         kind="bool", default=True, group="Effects"),
    dict(label="Lens Flares", section="SystemSettings", key="LensFlares",
         kind="bool", default=True, group="Effects"),
    dict(label="Fog Volumes", section="SystemSettings", key="FogVolumes",
         kind="bool", default=True, group="Effects"),
    dict(label="Dynamic Shadows", section="SystemSettings", key="DynamicShadows",
         kind="bool", default=True, group="Effects"),
    dict(label="Composite Dynamic Lights", section="SystemSettings", key="CompositeDynamicLights",
         kind="bool", default=True, group="Effects"),
    dict(label="Directional Lightmaps", section="SystemSettings", key="DirectionalLightmaps",
         kind="bool", default=True, group="Effects"),
    dict(label="Floating Point Render Targets", section="SystemSettings", key="FloatingPointRenderTargets",
         kind="bool", default=True, group="Effects"),
    dict(label="One Frame Thread Lag", section="SystemSettings", key="OneFrameThreadLag",
         kind="bool", default=True, group="Effects"),
    dict(label="SpeedTree Leaves", section="SystemSettings", key="SpeedTreeLeaves",
         kind="bool", default=True, group="Effects"),
    dict(label="SpeedTree Fronds", section="SystemSettings", key="SpeedTreeFronds",
         kind="bool", default=True, group="Effects"),

    dict(label="Min Shadow Resolution", section="SystemSettings", key="MinShadowResolution",
         kind="int", default=64, range=(1, 4096), group="Shadows / LOD"),
    dict(label="Max Shadow Resolution", section="SystemSettings", key="MaxShadowResolution",
         kind="int", default=1024, range=(1, 4096), group="Shadows / LOD"),
    dict(label="Shadow Filter Quality Bias", section="SystemSettings", key="ShadowFilterQualityBias",
         kind="int", default=0, range=(-4, 4), group="Shadows / LOD"),
    dict(label="Skeletal Mesh LOD Bias", section="SystemSettings", key="SkeletalMeshLODBias",
         kind="int", default=0, range=(-4, 4), group="Shadows / LOD"),
    dict(label="Particle LOD Bias", section="SystemSettings", key="ParticleLODBias",
         kind="int", default=0, range=(-4, 4), group="Shadows / LOD"),
]

PREFERRED_QUICK_SETTINGS_FILE = "GearEngine.ini"

# --------------------------------------------------------------------------
# Curated "Cheats" definitions.
# --------------------------------------------------------------------------
CHEATS = [
    dict(
        label="Invincibility (Player Health Mod)",
        key="PlayerHealthMod",
        on_value="32767",
        section_contains="DifficultySettings",
        known_good={
            "GearGame.DifficultySettings": "1.0",
            "GearGame.DifficultySettings_Casual": "2.35",
            "GearGame.DifficultySettings_Normal": "1.5",
            "GearGame.DifficultySettings_Hardcore": "1.0",
            "GearGame.DifficultySettings_Insane": "0.35",
        },
        description=(
            "Sets PlayerHealthMod to 32767 in every DifficultySettings section "
            "across all loaded files. Turning this off resets each section to "
            "its correct known-good default (not just whatever value was there "
            "before)."
        ),
    ),
    dict(
        label="Infinite Ammo (Weapon Mag Size)",
        key="WeaponMagSize",
        on_value="-1",
        section_contains="Weap",
        known_good={
            "GearGame.GearWeap_AssaultRifle": "50",
            "GearGame.GearWeap_BoomshotBase": "1",
            "GearGameContent.GearWeap_Boomer_Flail": "1",
            "GearGame.GearWeap_BowBase": "1",
            "GearGame.GearWeap_COGPistol": "12",
            "GearGame.GearWeap_GrenadeBase": "1",
            "GearGame.GearWeap_LocustAssaultRifle": "17",
            "GearGame.GearWeap_LocustPistol": "6",
            "GearGame.GearWeap_LocustBurstPistolBase": "32",
            "GearGameContent.GearWeap_LocustBurstPistol_Skorge": "96",
            "GearGame.GearWeap_Shotgun": "8",
            "GearGame.GearWeap_SniperRifle": "1",
            "GearGameContent.GearWeap_Troika": "0",
            "GearGameContent.GearWeap_Troika_Raam": "0",
            "GearGame.GearWeap_WretchMelee": "1",
            "GearGameContent.GearVGearWeap_UVTurret": "12000",
            "GearGameContent.GearWeap_FlameThrower": "50",
            "GearGameContent.GearWeap_FlameThrower_Turret": "0",
            "GearGameContent.GearWeap_HeavyMiniGun": "250",
            "GearGameContent.GearWeap_HeavyMortar": "1",
            "GearGameContent.GearVWeap_RocketCannon": "6",
            "GearGameContent.GearVWeap_ReaverCannon": "1000",
            "GearGameContent.GearVWeap_RideReaverCannon": "1000",
            "GearGameContent.GearWeap_BrumakSideGun": "0",
            "GearGameContent.GearWeap_BrumakMainGun": "0",
            "GearGame.GearWeap_BloodMountMelee": "1",
            "GearGame.GearWeap_SireMelee": "1",
            "GearGame.GearWeap_RockWormMelee": "1",
            "GearGame.GearWeap_NemaSlugMelee": "1",
            "GearGameContent.GearWeap_SecurityBotGunFlying": "150",
            "GearGameContent.GearWeap_SecurityBotGunStationary": "150"
        },
        description=(
            "Sets WeaponMagSize to -1 in all weapon sections to enable infinite ammo "
            "without having to reload. Turning this off restores each weapon to its "
            "known-good default magazine size."
        ),
    ),
]


# --------------------------------------------------------------------------
# Parsing / writing
# --------------------------------------------------------------------------

def parse_ini(path: Path):
    section_order = []
    sections = {}
    current = None

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for raw_line in f:
            line = raw_line.rstrip("\r\n")
            stripped = line.strip()

            if not stripped:
                continue

            m = re.match(r"^\[(.+)\]$", stripped)
            if m:
                current = m.group(1)
                if current not in sections:
                    sections[current] = []
                    section_order.append(current)
                continue

            if current is None:
                continue

            if stripped.startswith(COMMENT_PREFIXES):
                sections[current].append([stripped, ""])
                continue

            if "=" in stripped:
                key, _, value = stripped.partition("=")
                sections[current].append([key, value])
            else:
                sections[current].append([stripped, ""])

    return section_order, sections


def write_ini(path: Path, section_order, sections):
    lines = []
    for sec in section_order:
        lines.append(f"[{sec}]")
        for col0, col1 in sections[sec]:
            text = col0.strip()
            if text.startswith(COMMENT_PREFIXES):
                lines.append(col0)
            else:
                lines.append(f"{col0}={col1}")
        lines.append("")

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def find_gear_ini_files(folder: Path):
    return sorted(
        (p for p in folder.glob("*.ini") if p.is_file() and p.name.lower().startswith("gear")),
        key=lambda p: p.name.lower(),
    )


# --------------------------------------------------------------------------
# Main window
# --------------------------------------------------------------------------

class IniEditor(QMainWindow):
    def __init__(self, initial_path: str | None = None):
        super().__init__()
        self.base_title = "Gear*.ini Editor"
        self.setWindowTitle(self.base_title)
        self.resize(1200, 750)

        self.documents: dict[str, dict] = {}
        self.current_doc: str | None = None
        self.current_section: str | None = None
        self._loading_quick_settings = False
        self._loading_cheats = False
        self._cheat_state: dict[str, dict] = {}

        self._build_ui()
        self._build_toolbar()

        if initial_path:
            p = Path(initial_path)
            if p.is_dir():
                self._load_documents(find_gear_ini_files(p), source_desc=str(p))
            else:
                self._load_documents([p], source_desc=str(p))

    @property
    def cur_section_order(self) -> list:
        if self.current_doc is None:
            return []
        return self.documents[self.current_doc]["section_order"]

    @property
    def cur_sections(self) -> dict:
        if self.current_doc is None:
            return {}
        return self.documents[self.current_doc]["sections"]

    def find_section_owner(self, section: str):
        for name, doc in self.documents.items():
            if section in doc["sections"]:
                return name
        return None

    def find_entry_index_in(self, doc_name: str, section: str, key: str):
        entries = self.documents[doc_name]["sections"].get(section)
        if not entries:
            return None
        for i, (col0, _col1) in enumerate(entries):
            if col0.strip().startswith(COMMENT_PREFIXES):
                continue
            if col0.strip().lower() == key.strip().lower():
                return i
        return None

    def get_raw_value(self, section: str, key: str, default: str = "") -> str:
        owner = self.find_section_owner(section)
        if owner is None:
            return default
        idx = self.find_entry_index_in(owner, section, key)
        if idx is None:
            return default
        return self.documents[owner]["sections"][section][idx][1]

    def set_raw_value(self, section: str, key: str, value: str):
        owner = self.find_section_owner(section)
        if owner is None:
            if not self.documents:
                QMessageBox.information(self, "No files loaded", "Open a file or folder first.")
                return
            owner = PREFERRED_QUICK_SETTINGS_FILE if PREFERRED_QUICK_SETTINGS_FILE in self.documents else next(iter(self.documents))

        doc = self.documents[owner]
        if section not in doc["sections"]:
            doc["sections"][section] = []
            doc["section_order"].append(section)
            if owner == self.current_doc:
                self.refresh_section_list()

        idx = self.find_entry_index_in(owner, section, key)
        if idx is None:
            doc["sections"][section].append([key, value])
        else:
            doc["sections"][section][idx][1] = value

        self.mark_doc_dirty(owner, True)
        if owner == self.current_doc and self.current_section == section:
            self.populate_table()
            self.section_title.setText(f"[{section}]  ({len(doc['sections'][section])} entries)  -  {owner}")

    def mark_doc_dirty(self, doc_name: str, value: bool):
        self.documents[doc_name]["dirty"] = value
        self.refresh_files_list()
        self.update_window_title()

    def any_dirty(self) -> bool:
        return any(d["dirty"] for d in self.documents.values())

    def update_window_title(self):
        if not self.documents:
            self.setWindowTitle(self.base_title)
            return
        dirty_count = sum(1 for d in self.documents.values() if d["dirty"])
        title = f"{self.base_title} - {len(self.documents)} file(s) loaded"
        if dirty_count:
            title = f"* {title} ({dirty_count} unsaved)"
        self.setWindowTitle(title)

    def _build_toolbar(self):
        tb = QToolBar("Main")
        tb.setMovable(False)
        self.addToolBar(tb)

        open_file_act = QAction("Open File...", self)
        open_file_act.setShortcut(QKeySequence.Open)
        open_file_act.triggered.connect(self.action_open_file)
        tb.addAction(open_file_act)

        open_folder_act = QAction("Open Folder (Gear*.ini)...", self)
        open_folder_act.setShortcut(QKeySequence("Ctrl+Shift+O"))
        open_folder_act.triggered.connect(self.action_open_folder)
        tb.addAction(open_folder_act)

        tb.addSeparator()

        save_act = QAction("Save Current File", self)
        save_act.setShortcut(QKeySequence.Save)
        save_act.triggered.connect(self.action_save_current)
        tb.addAction(save_act)

        save_all_act = QAction("Save All", self)
        save_all_act.setShortcut(QKeySequence("Ctrl+Shift+S"))
        save_all_act.triggered.connect(self.action_save_all)
        tb.addAction(save_all_act)

        save_as_act = QAction("Save Current As...", self)
        save_as_act.triggered.connect(self.action_save_as)
        tb.addAction(save_as_act)

    def _build_quick_settings_tab(self) -> QWidget:
        self.quick_widgets = []

        container = QWidget()
        outer = QVBoxLayout(container)
        outer.addWidget(QLabel(
            "Common settings, grouped for quick editing. Each field automatically\n"
            "finds whichever loaded file actually contains that section. Everything\n"
            "else is still available in the Raw Editor tab."
        ))

        groups = {}
        for spec in QUICK_SETTINGS:
            group_name = spec["group"]
            if group_name not in groups:
                box = QGroupBox(group_name)
                box.setLayout(QFormLayout())
                groups[group_name] = box
                outer.addWidget(box)
            form: QFormLayout = groups[group_name].layout()

            widget = self._make_quick_widget(spec)
            form.addRow(spec["label"], widget)
            self.quick_widgets.append((spec, widget))

        outer.addStretch(1)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(container)
        return scroll

    def _make_quick_widget(self, spec: dict) -> QWidget:
        kind = spec["kind"]
        if kind == "int":
            w = QSpinBox()
            lo, hi = spec["range"]
            w.setRange(lo, hi)
            w.valueChanged.connect(lambda val, s=spec: self.on_quick_setting_changed(s, str(val)))
            return w
        if kind == "float":
            w = QDoubleSpinBox()
            lo, hi = spec["range"]
            w.setRange(lo, hi)
            w.setDecimals(2)
            w.valueChanged.connect(lambda val, s=spec: self.on_quick_setting_changed(s, f"{val:.6f}"))
            return w
        if kind == "bool":
            w = QCheckBox()
            w.stateChanged.connect(lambda state, s=spec: self.on_quick_setting_changed(s, "True" if state else "False"))
            return w
        if kind == "choice":
            w = QComboBox()
            for val in spec["choices"]:
                w.addItem(str(val), val)
            w.currentIndexChanged.connect(lambda idx, s=spec, cb=w: self.on_quick_setting_changed(s, str(cb.itemData(idx))))
            return w
        if kind == "choice_label":
            w = QComboBox()
            for label, val in spec["choices"]:
                w.addItem(label, val)
            w.currentIndexChanged.connect(lambda idx, s=spec, cb=w: self.on_quick_setting_changed(s, str(cb.itemData(idx))))
            return w
        raise ValueError(f"Unknown quick setting kind: {kind}")

    def on_quick_setting_changed(self, spec: dict, new_value: str):
        if self._loading_quick_settings:
            return
        self.set_raw_value(spec["section"], spec["key"], new_value)

    def refresh_quick_settings(self):
        self._loading_quick_settings = True
        for spec, widget in self.quick_widgets:
            owner = self.find_section_owner(spec["section"])
            raw = self.get_raw_value(spec["section"], spec["key"], default=str(spec["default"]))
            if owner:
                widget.setToolTip(f"{spec['section']}.{spec['key']}  (in {owner})")
            else:
                widget.setToolTip(
                    f"{spec['section']}.{spec['key']}  (not present yet - "
                    f"editing will create it in {PREFERRED_QUICK_SETTINGS_FILE if PREFERRED_QUICK_SETTINGS_FILE in self.documents else 'the first loaded file'})"
                )
            kind = spec["kind"]
            try:
                if kind == "int":
                    widget.setValue(int(float(raw)))
                elif kind == "float":
                    widget.setValue(float(raw))
                elif kind == "bool":
                    widget.setChecked(str(raw).strip().lower() in ("true", "1"))
                elif kind in ("choice", "choice_label"):
                    target = int(float(raw))
                    match_idx = widget.findData(target)
                    widget.setCurrentIndex(match_idx if match_idx >= 0 else 0)
            except (ValueError, TypeError):
                pass
        self._loading_quick_settings = False

    def _build_cheats_tab(self) -> QWidget:
        self.cheat_widgets = []

        container = QWidget()
        outer = QVBoxLayout(container)
        outer.addWidget(QLabel(
            "Cheats apply immediately to the loaded file(s) in memory - remember to Save."
        ))

        box = QGroupBox("Difficulty")
        box_layout = QVBoxLayout(box)
        for spec in CHEATS:
            cb = QCheckBox(spec["label"])
            cb.stateChanged.connect(lambda state, s=spec: self.on_cheat_toggled(s, bool(state)))
            box_layout.addWidget(cb)

            note = QLabel(spec["description"])
            note.setWordWrap(True)
            note.setStyleSheet("color: gray; font-size: 11px;")
            box_layout.addWidget(note)

            status = QLabel("")
            status.setWordWrap(True)
            status.setStyleSheet("color: gray; font-size: 11px; font-style: italic;")
            box_layout.addWidget(status)

            self.cheat_widgets.append((spec, cb, status))
        outer.addWidget(box)
        outer.addStretch(1)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(container)
        return scroll

    def find_cheat_matches(self, spec: dict):
        matches = []
        for doc_name, doc in self.documents.items():
            for sec in doc["section_order"]:
                if spec["section_contains"].lower() not in sec.lower():
                    continue
                if self.find_entry_index_in(doc_name, sec, spec["key"]) is not None:
                    matches.append((doc_name, sec))
        return matches

    def is_cheat_applied(self, spec: dict) -> bool:
        """
        Checks if the cheat is already active in the loaded documents.
        Returns True if at least one matching section exists AND all
        matching sections have their key set to the 'on_value'.
        """
        matches = self.find_cheat_matches(spec)
        if not matches:
            return False

        for doc_name, sec in matches:
            idx = self.find_entry_index_in(doc_name, sec, spec["key"])
            if idx is not None:
                current_val = self.documents[doc_name]["sections"][sec][idx][1]
                if current_val != spec["on_value"]:
                    return False

        return True

    def refresh_cheats_status(self):
        for spec, cb, status in self.cheat_widgets:
            matches = self.find_cheat_matches(spec)
            if matches:
                files = sorted({doc_name for doc_name, _sec in matches})
                status.setText(f"Found {spec['key']} in {len(matches)} section(s) across: {', '.join(files)}")
            else:
                status.setText("Not found in any currently loaded file.")

    def on_cheat_toggled(self, spec: dict, checked: bool):
        if self._loading_cheats:
            return
        self.toggle_cheat(spec, checked)

    def toggle_cheat(self, spec: dict, turn_on: bool):
        label = spec["label"]
        affected_current_section = False

        if turn_on:
            matches = self.find_cheat_matches(spec)
            if not matches:
                QMessageBox.information(
                    self, "Not found",
                    f"No sections containing '{spec['section_contains']}' with key "
                    f"'{spec['key']}' were found in the currently loaded file(s)."
                )
                self._set_cheat_checkbox(label, False)
                return

            originals = {}
            for doc_name, sec in matches:
                idx = self.find_entry_index_in(doc_name, sec, spec["key"])
                doc = self.documents[doc_name]
                originals[(doc_name, sec)] = doc["sections"][sec][idx][1]
                doc["sections"][sec][idx][1] = spec["on_value"]
                self.mark_doc_dirty(doc_name, True)
                if doc_name == self.current_doc and sec == self.current_section:
                    affected_current_section = True
            self._cheat_state[label] = originals
        else:
            originals = self._cheat_state.get(label, {})
            known_good = {k.lower(): v for k, v in spec.get("known_good", {}).items()}
            targets = set(self.find_cheat_matches(spec)) | set(originals.keys())
            for doc_name, sec in targets:
                if doc_name not in self.documents or sec not in self.documents[doc_name]["sections"]:
                    continue
                idx = self.find_entry_index_in(doc_name, sec, spec["key"])
                if idx is None:
                    continue
                if sec.lower() in known_good:
                    restore_val = known_good[sec.lower()]
                elif (doc_name, sec) in originals:
                    restore_val = originals[(doc_name, sec)]
                else:
                    continue
                self.documents[doc_name]["sections"][sec][idx][1] = restore_val
                self.mark_doc_dirty(doc_name, True)
                if doc_name == self.current_doc and sec == self.current_section:
                    affected_current_section = True
            self._cheat_state[label] = {}

        if affected_current_section:
            self.populate_table()
            self.section_title.setText(
                f"[{self.current_section}]  ({len(self.cur_sections[self.current_section])} entries)  -  {self.current_doc}"
            )
        self.refresh_quick_settings()
        self.refresh_cheats_status()

    def _set_cheat_checkbox(self, label: str, checked: bool):
        for spec, cb, _status in self.cheat_widgets:
            if spec["label"] == label:
                self._loading_cheats = True
                cb.setChecked(checked)
                self._loading_cheats = False
                break

    def _build_ui(self):
        tabs = QTabWidget()
        tabs.addTab(self._build_quick_settings_tab(), "Quick Settings")
        tabs.addTab(self._build_cheats_tab(), "Cheats")

        splitter = QSplitter(Qt.Horizontal)

        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(4, 4, 4, 4)

        left_layout.addWidget(QLabel("Files"))
        self.files_list = QListWidget()
        self.files_list.currentTextChanged.connect(self.on_file_selected)
        left_layout.addWidget(self.files_list, stretch=1)

        left_layout.addWidget(QLabel("Sections"))

        self.section_filter = QLineEdit()
        self.section_filter.setPlaceholderText("Filter sections...")
        self.section_filter.textChanged.connect(self.refresh_section_list)
        left_layout.addWidget(self.section_filter)

        self.section_list = QListWidget()
        self.section_list.currentTextChanged.connect(self.on_section_selected)
        self.section_list.itemChanged.connect(self.on_section_renamed)
        left_layout.addWidget(self.section_list, stretch=2)

        sec_btn_row = QHBoxLayout()
        add_sec_btn = QPushButton("Add Section")
        add_sec_btn.clicked.connect(self.add_section)
        del_sec_btn = QPushButton("Delete Section")
        del_sec_btn.clicked.connect(self.delete_section)
        sec_btn_row.addWidget(add_sec_btn)
        sec_btn_row.addWidget(del_sec_btn)
        left_layout.addLayout(sec_btn_row)

        left_layout.addWidget(QLabel("Global Search (all loaded files)"))
        self.global_search = QLineEdit()
        self.global_search.setPlaceholderText("Search key or value...")
        self.global_search.textChanged.connect(self.run_global_search)
        left_layout.addWidget(self.global_search)

        self.search_results = QListWidget()
        self.search_results.itemDoubleClicked.connect(self.jump_to_search_result)
        left_layout.addWidget(self.search_results, stretch=2)

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(4, 4, 4, 4)

        self.section_title = QLabel("No section selected")
        right_layout.addWidget(self.section_title)

        self.row_filter = QLineEdit()
        self.row_filter.setPlaceholderText("Filter rows in this section...")
        self.row_filter.textChanged.connect(self.apply_row_filter)
        right_layout.addWidget(self.row_filter)

        self.table = QTableWidget(0, 2)
        self.table.setHorizontalHeaderLabels(["Key", "Value"])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.itemChanged.connect(self.on_table_item_changed)
        right_layout.addWidget(self.table, stretch=1)

        row_btn_row = QHBoxLayout()
        add_row_btn = QPushButton("Add Row")
        add_row_btn.clicked.connect(self.add_row)
        del_row_btn = QPushButton("Delete Selected Row(s)")
        del_row_btn.clicked.connect(self.delete_rows)
        add_comment_btn = QPushButton("Add Comment Row")
        add_comment_btn.clicked.connect(self.add_comment_row)
        row_btn_row.addWidget(add_row_btn)
        row_btn_row.addWidget(add_comment_btn)
        row_btn_row.addWidget(del_row_btn)
        right_layout.addLayout(row_btn_row)

        splitter.addWidget(left)
        splitter.addWidget(right)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([340, 780])

        tabs.addTab(splitter, "Raw Editor")
        tabs.currentChanged.connect(self.on_tab_changed)
        self.tabs = tabs

        self.setCentralWidget(tabs)

        self.status = QStatusBar()
        self.setStatusBar(self.status)

    def on_tab_changed(self, index: int):
        if index == 0:
            self.refresh_quick_settings()
        elif index == 1:
            self.refresh_cheats_status()

    def action_open_file(self):
        if not self.confirm_discard_changes():
            return
        path, _ = QFileDialog.getOpenFileName(self, "Open ini file", "", "INI Files (*.ini);;All Files (*)")
        if path:
            self._load_documents([Path(path)], source_desc=path)

    def action_open_folder(self):
        if not self.confirm_discard_changes():
            return
        folder = QFileDialog.getExistingDirectory(self, "Select folder containing Gear*.ini files")
        if not folder:
            return
        matches = find_gear_ini_files(Path(folder))
        if not matches:
            QMessageBox.information(self, "No files found", f"No Gear*.ini files were found in:\n{folder}")
            return
        self._load_documents(matches, source_desc=folder)

    def _load_documents(self, paths: list, source_desc: str):
        self.documents = {}
        self.current_doc = None
        self.current_section = None
        self._cheat_state = {}

        errors = []
        for p in paths:
            try:
                section_order, sections = parse_ini(p)
            except OSError as e:
                errors.append(f"{p.name}: {e}")
                continue
            self.documents[p.name] = {
                "path": p,
                "section_order": section_order,
                "sections": sections,
                "dirty": False,
            }

        if errors:
            QMessageBox.warning(self, "Some files failed to load", "\n".join(errors))

        self.section_filter.clear()
        self.global_search.clear()
        self.search_results.clear()
        self.refresh_files_list()

        if self.documents:
            self.files_list.setCurrentRow(0)

        total_sections = sum(len(d["section_order"]) for d in self.documents.values())
        total_entries = sum(len(v) for d in self.documents.values() for v in d["sections"].values())
        self.status.showMessage(
            f"Loaded {len(self.documents)} file(s) from {source_desc} "
            f"({total_sections} sections, {total_entries} entries total)"
        )
        self.update_window_title()
        self.refresh_quick_settings()

        for spec, cb, _status in getattr(self, "cheat_widgets", []):
            self._loading_cheats = True
            cb.setChecked(self.is_cheat_applied(spec))
            self._loading_cheats = False

        self.refresh_cheats_status()

    def action_save_current(self):
        if self.current_doc is None:
            QMessageBox.information(self, "No file selected", "Select a file to save first.")
            return
        self.save_document(self.current_doc)

    def action_save_all(self):
        if not self.documents:
            return
        dirty_names = [name for name, d in self.documents.items() if d["dirty"]]
        if not dirty_names:
            self.status.showMessage("Nothing to save - no unsaved changes.")
            return
        for name in dirty_names:
            self.save_document(name)

    def action_save_as(self):
        if self.current_doc is None:
            QMessageBox.information(self, "No file selected", "Select a file to save first.")
            return
        doc = self.documents[self.current_doc]
        path, _ = QFileDialog.getSaveFileName(self, "Save ini file as", str(doc["path"]), "INI Files (*.ini);;All Files (*)")
        if not path:
            return
        new_path = Path(path)
        try:
            write_ini(new_path, doc["section_order"], doc["sections"])
        except OSError as e:
            QMessageBox.critical(self, "Error", f"Could not save file:\n{e}")
            return

        old_name = self.current_doc
        new_name = new_path.name
        doc["path"] = new_path
        doc["dirty"] = False
        if new_name != old_name:
            del self.documents[old_name]
            self.documents[new_name] = doc
            self.current_doc = new_name
        self.refresh_files_list()
        self.update_window_title()
        self.status.showMessage(f"Saved to {new_path}")

    def save_document(self, name: str):
        doc = self.documents[name]
        try:
            write_ini(doc["path"], doc["section_order"], doc["sections"])
        except OSError as e:
            QMessageBox.critical(self, "Error", f"Could not save {name}:\n{e}")
            return
        doc["dirty"] = False
        self.refresh_files_list()
        self.update_window_title()
        self.status.showMessage(f"Saved {doc['path']}")

    def confirm_discard_changes(self) -> bool:
        if not self.any_dirty():
            return True
        resp = QMessageBox.question(
            self,
            "Unsaved changes",
            "You have unsaved changes in one or more files. Discard them?",
            QMessageBox.Yes | QMessageBox.Cancel,
        )
        return resp == QMessageBox.Yes

    def closeEvent(self, event):
        if self.confirm_discard_changes():
            event.accept()
        else:
            event.ignore()

    def refresh_files_list(self):
        self.files_list.blockSignals(True)
        self.files_list.clear()
        for name, doc in self.documents.items():
            label = f"* {name}" if doc["dirty"] else name
            item = QListWidgetItem(label)
            item.setData(Qt.UserRole, name)
            self.files_list.addItem(item)
        self.files_list.blockSignals(False)

    def on_file_selected(self, label: str):
        item = self.files_list.currentItem()
        name = item.data(Qt.UserRole) if item else None
        if not name or name not in self.documents:
            self.current_doc = None
            self.current_section = None
            self.section_list.clear()
            self.table.setRowCount(0)
            self.section_title.setText("No section selected")
            return
        self.current_doc = name
        self.current_section = None
        self.section_filter.clear()
        self.refresh_section_list()
        if self.cur_section_order:
            self.section_list.setCurrentRow(0)
        else:
            self.table.setRowCount(0)
            self.section_title.setText("No section selected")

    def refresh_section_list(self):
        filt = self.section_filter.text().strip().lower()
        self.section_list.blockSignals(True)
        self.section_list.clear()
        for name in self.cur_section_order:
            if filt and filt not in name.lower():
                continue
            item = QListWidgetItem(name)
            item.setFlags(item.flags() | Qt.ItemIsEditable)
            self.section_list.addItem(item)
        self.section_list.blockSignals(False)

    def on_section_selected(self, name: str):
        if not name or self.current_doc is None or name not in self.cur_sections:
            self.current_section = None
            self.table.setRowCount(0)
            self.section_title.setText("No section selected")
            return
        self.current_section = name
        self.section_title.setText(f"[{name}]  ({len(self.cur_sections[name])} entries)  -  {self.current_doc}")
        self.row_filter.clear()
        self.populate_table()

    def on_section_renamed(self, item: QListWidgetItem):
        if self.current_doc is None:
            return
        new_name = item.text().strip()
        if not new_name:
            QMessageBox.warning(self, "Invalid name", "Section name cannot be empty.")
            self.refresh_section_list()
            return

        current_texts = [self.section_list.item(i).text() for i in range(self.section_list.count())]
        missing = [n for n in self.cur_section_order if n not in current_texts]
        old_name = missing[0] if missing else None
        if old_name is None or old_name == new_name:
            return
        if new_name in self.cur_sections:
            QMessageBox.warning(self, "Duplicate name", f"Section '{new_name}' already exists.")
            self.refresh_section_list()
            return

        doc = self.documents[self.current_doc]
        idx = doc["section_order"].index(old_name)
        doc["section_order"][idx] = new_name
        doc["sections"][new_name] = doc["sections"].pop(old_name)
        if self.current_section == old_name:
            self.current_section = new_name
        self.mark_doc_dirty(self.current_doc, True)
        self.refresh_section_list()

    def add_section(self):
        if self.current_doc is None:
            QMessageBox.information(self, "No file selected", "Select a file first.")
            return
        name, ok = QInputDialog.getText(self, "Add Section", "Section name (without brackets):")
        if not ok or not name.strip():
            return
        name = name.strip()
        if name in self.cur_sections:
            QMessageBox.warning(self, "Duplicate name", f"Section '{name}' already exists.")
            return
        self.cur_section_order.append(name)
        self.cur_sections[name] = []
        self.mark_doc_dirty(self.current_doc, True)
        self.refresh_section_list()
        for i in range(self.section_list.count()):
            if self.section_list.item(i).text() == name:
                self.section_list.setCurrentRow(i)
                break

    def delete_section(self):
        if self.current_doc is None or not self.current_section:
            return
        resp = QMessageBox.question(
            self, "Delete section", f"Delete section [{self.current_section}] from {self.current_doc} and all its entries?"
        )
        if resp != QMessageBox.Yes:
            return
        name = self.current_section
        self.cur_section_order.remove(name)
        del self.cur_sections[name]
        self.current_section = None
        self.mark_doc_dirty(self.current_doc, True)
        self.refresh_section_list()
        if self.cur_section_order:
            self.section_list.setCurrentRow(0)
        else:
            self.table.setRowCount(0)
            self.section_title.setText("No section selected")

    def populate_table(self):
        self.table.blockSignals(True)
        self.table.setRowCount(0)
        if self.current_doc is not None and self.current_section:
            entries = self.cur_sections[self.current_section]
            self.table.setRowCount(len(entries))
            for row, (col0, col1) in enumerate(entries):
                self._set_row_items(row, col0, col1)
        self.table.blockSignals(False)
        self.apply_row_filter()

    def _set_row_items(self, row: int, col0: str, col1: str):
        is_comment = col0.strip().startswith(COMMENT_PREFIXES)
        item0 = QTableWidgetItem(col0)
        item1 = QTableWidgetItem("" if is_comment else col1)
        if is_comment:
            gray = QBrush(QColor(120, 120, 120))
            item0.setForeground(gray)
            item1.setFlags(item1.flags() & ~Qt.ItemIsEditable)
        self.table.setItem(row, 0, item0)
        self.table.setItem(row, 1, item1)

    def on_table_item_changed(self, item: QTableWidgetItem):
        if self.current_doc is None or not self.current_section:
            return
        row = item.row()
        entries = self.cur_sections[self.current_section]
        if row >= len(entries):
            return
        col0_item = self.table.item(row, 0)
        col1_item = self.table.item(row, 1)
        col0 = col0_item.text() if col0_item else ""
        col1 = col1_item.text() if col1_item else ""
        entries[row][0] = col0
        entries[row][1] = col1

        self.table.blockSignals(True)
        self._set_row_items(row, col0, col1)
        self.table.blockSignals(False)

        self.mark_doc_dirty(self.current_doc, True)
        self.section_title.setText(f"[{self.current_section}]  ({len(entries)} entries)  -  {self.current_doc}")

    def add_row(self):
        if self.current_doc is None or not self.current_section:
            QMessageBox.information(self, "No section", "Select or create a section first.")
            return
        entries = self.cur_sections[self.current_section]
        entries.append(["NewKey", ""])
        row = len(entries) - 1
        self.table.blockSignals(True)
        self.table.setRowCount(len(entries))
        self._set_row_items(row, "NewKey", "")
        self.table.blockSignals(False)
        self.table.scrollToBottom()
        self.table.editItem(self.table.item(row, 0))
        self.mark_doc_dirty(self.current_doc, True)
        self.apply_row_filter()

    def add_comment_row(self):
        if self.current_doc is None or not self.current_section:
            QMessageBox.information(self, "No section", "Select or create a section first.")
            return
        entries = self.cur_sections[self.current_section]
        entries.append(["; comment", ""])
        row = len(entries) - 1
        self.table.blockSignals(True)
        self.table.setRowCount(len(entries))
        self._set_row_items(row, "; comment", "")
        self.table.blockSignals(False)
        self.table.scrollToBottom()
        self.table.editItem(self.table.item(row, 0))
        self.mark_doc_dirty(self.current_doc, True)
        self.apply_row_filter()

    def delete_rows(self):
        if self.current_doc is None or not self.current_section:
            return
        selected_rows = sorted({idx.row() for idx in self.table.selectedIndexes()}, reverse=True)
        if not selected_rows:
            return
        entries = self.cur_sections[self.current_section]
        for row in selected_rows:
            if 0 <= row < len(entries):
                del entries[row]
        self.populate_table()
        self.mark_doc_dirty(self.current_doc, True)
        self.section_title.setText(f"[{self.current_section}]  ({len(entries)} entries)  -  {self.current_doc}")

    def apply_row_filter(self):
        filt = self.row_filter.text().strip().lower()
        for row in range(self.table.rowCount()):
            if not filt:
                self.table.setRowHidden(row, False)
                continue
            key_item = self.table.item(row, 0)
            val_item = self.table.item(row, 1)
            text = ((key_item.text() if key_item else "") + " " + (val_item.text() if val_item else "")).lower()
            self.table.setRowHidden(row, filt not in text)

    def run_global_search(self):
        query = self.global_search.text().strip().lower()
        self.search_results.clear()
        if not query:
            return
        count = 0
        for doc_name, doc in self.documents.items():
            for sec in doc["section_order"]:
                for row, (col0, col1) in enumerate(doc["sections"][sec]):
                    if query in col0.lower() or query in col1.lower():
                        is_comment = col0.strip().startswith(COMMENT_PREFIXES)
                        label = f"[{doc_name}] [{sec}]  {col0}" if is_comment else f"[{doc_name}] [{sec}]  {col0}={col1}"
                        item = QListWidgetItem(label)
                        item.setData(Qt.UserRole, (doc_name, sec, row))
                        self.search_results.addItem(item)
                        count += 1
                        if count >= 500:
                            return

    def jump_to_search_result(self, item: QListWidgetItem):
        doc_name, sec, row = item.data(Qt.UserRole)

        for i in range(self.files_list.count()):
            if self.files_list.item(i).data(Qt.UserRole) == doc_name:
                self.files_list.setCurrentRow(i)
                break

        self.section_filter.clear()
        for i in range(self.section_list.count()):
            if self.section_list.item(i).text() == sec:
                self.section_list.setCurrentRow(i)
                break

        self.row_filter.clear()
        if 0 <= row < self.table.rowCount():
            self.table.selectRow(row)
            self.table.scrollToItem(self.table.item(row, 0))


def main():
    app = QApplication(sys.argv)
    initial_path = sys.argv[1] if len(sys.argv) > 1 else None
    win = IniEditor(initial_path)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
