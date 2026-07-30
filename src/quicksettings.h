#pragma once

#include <QString>
#include <QVector>
#include <QPair>

enum class QuickKind { Int, Float, Bool, Choice, ChoiceLabel };

struct QuickSettingSpec {
    QString label;
    QString section;
    QString key;
    QuickKind kind;
    QString group;

    // Int
    int intDefault = 0;
    int intMin = 0;
    int intMax = 0;

    // Float
    double floatDefault = 0.0;
    double floatMin = 0.0;
    double floatMax = 0.0;

    // Bool
    bool boolDefault = false;

    // Choice: value list, displayed as the value itself
    QVector<int> choiceValues;
    int choiceDefault = 0;

    // ChoiceLabel: (label, value) pairs
    QVector<QPair<QString, int>> choiceLabelValues;
    int choiceLabelDefault = 0;
};

inline const QVector<QuickSettingSpec> &quickSettings()
{
    static const QVector<QuickSettingSpec> specs = [] {
        QVector<QuickSettingSpec> v;

        auto addInt = [&](QString label, QString section, QString key, int def, int lo, int hi, QString group) {
            QuickSettingSpec s;
            s.label = label; s.section = section; s.key = key; s.kind = QuickKind::Int;
            s.intDefault = def; s.intMin = lo; s.intMax = hi; s.group = group;
            v.append(s);
        };
        auto addFloat = [&](QString label, QString section, QString key, double def, double lo, double hi, QString group) {
            QuickSettingSpec s;
            s.label = label; s.section = section; s.key = key; s.kind = QuickKind::Float;
            s.floatDefault = def; s.floatMin = lo; s.floatMax = hi; s.group = group;
            v.append(s);
        };
        auto addBool = [&](QString label, QString section, QString key, bool def, QString group) {
            QuickSettingSpec s;
            s.label = label; s.section = section; s.key = key; s.kind = QuickKind::Bool;
            s.boolDefault = def; s.group = group;
            v.append(s);
        };
        auto addChoice = [&](QString label, QString section, QString key, int def, QVector<int> choices, QString group) {
            QuickSettingSpec s;
            s.label = label; s.section = section; s.key = key; s.kind = QuickKind::Choice;
            s.choiceDefault = def; s.choiceValues = choices; s.group = group;
            v.append(s);
        };
        auto addChoiceLabel = [&](QString label, QString section, QString key, int def,
                                   QVector<QPair<QString, int>> choices, QString group) {
            QuickSettingSpec s;
            s.label = label; s.section = section; s.key = key; s.kind = QuickKind::ChoiceLabel;
            s.choiceLabelDefault = def; s.choiceLabelValues = choices; s.group = group;
            v.append(s);
        };

        // Display
        addInt("Resolution Width (ResX)", "SystemSettings", "ResX", 1280, 320, 7680, "Display");
        addInt("Resolution Height (ResY)", "SystemSettings", "ResY", 720, 240, 4320, "Display");
        addBool("Fullscreen", "SystemSettings", "Fullscreen", false, "Display");
        addBool("Startup Fullscreen", "WinDrv.WindowsClient", "StartupFullscreen", false, "Display");
        addBool("VSync", "SystemSettings", "UseVsync", false, "Display");
        addFloat("Screen Percentage", "SystemSettings", "ScreenPercentage", 100.0, 10.0, 200.0, "Display");
        addInt("Max Smoothed Framerate", "Engine.GameEngine", "MaxSmoothedFrameRate", 60, 0, 300, "Display");

        // Quality
        addChoice("Max Anisotropy", "SystemSettings", "MaxAnisotropy", 4, {0, 1, 2, 4, 8, 16}, "Quality");
        addChoice("Max Multisamples (AA)", "SystemSettings", "MaxMultisamples", 1, {1, 2, 4, 8}, "Quality");
        addChoiceLabel("Detail Mode", "SystemSettings", "DetailMode", 2,
                       {{"Low", 0}, {"Medium", 1}, {"High", 2}}, "Quality");

        // Effects
        addBool("Motion Blur", "SystemSettings", "MotionBlur", false, "Effects");
        addBool("Depth of Field", "SystemSettings", "DepthOfField", true, "Effects");
        addBool("Ambient Occlusion", "SystemSettings", "AmbientOcclusion", true, "Effects");
        addBool("Bloom", "SystemSettings", "Bloom", true, "Effects");
        addBool("High Quality Bloom", "SystemSettings", "UseHighQualityBloom", true, "Effects");
        addBool("Distortion", "SystemSettings", "Distortion", true, "Effects");
        addBool("Lens Flares", "SystemSettings", "LensFlares", true, "Effects");
        addBool("Fog Volumes", "SystemSettings", "FogVolumes", true, "Effects");
        addBool("Dynamic Shadows", "SystemSettings", "DynamicShadows", true, "Effects");
        addBool("Composite Dynamic Lights", "SystemSettings", "CompositeDynamicLights", true, "Effects");
        addBool("Directional Lightmaps", "SystemSettings", "DirectionalLightmaps", true, "Effects");
        addBool("Floating Point Render Targets", "SystemSettings", "FloatingPointRenderTargets", true, "Effects");
        addBool("One Frame Thread Lag", "SystemSettings", "OneFrameThreadLag", true, "Effects");
        addBool("SpeedTree Leaves", "SystemSettings", "SpeedTreeLeaves", true, "Effects");
        addBool("SpeedTree Fronds", "SystemSettings", "SpeedTreeFronds", true, "Effects");

        // Shadows / LOD
        addInt("Min Shadow Resolution", "SystemSettings", "MinShadowResolution", 64, 1, 4096, "Shadows / LOD");
        addInt("Max Shadow Resolution", "SystemSettings", "MaxShadowResolution", 1024, 1, 4096, "Shadows / LOD");
        addInt("Shadow Filter Quality Bias", "SystemSettings", "ShadowFilterQualityBias", 0, -4, 4, "Shadows / LOD");
        addInt("Skeletal Mesh LOD Bias", "SystemSettings", "SkeletalMeshLODBias", 0, -4, 4, "Shadows / LOD");
        addInt("Particle LOD Bias", "SystemSettings", "ParticleLODBias", 0, -4, 4, "Shadows / LOD");

        return v;
    }();
    return specs;
}

inline const QString &preferredQuickSettingsFile()
{
    static const QString name = "GearEngine.ini";
    return name;
}
