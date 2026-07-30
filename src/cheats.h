#pragma once

#include <QString>
#include <QVector>
#include <QMap>

struct CheatSpec {
    QString label;
    QString key;
    QString onValue;
    QString sectionContains;
    QMap<QString, QString> knownGood; // section name -> known-good value (case-preserving key)
    QString description;
};

inline const QVector<CheatSpec> &cheats()
{
    static const QVector<CheatSpec> specs = [] {
        QVector<CheatSpec> v;

        {
            CheatSpec c;
            c.label = "Invincibility (Player Health Mod)";
            c.key = "PlayerHealthMod";
            c.onValue = "32767";
            c.sectionContains = "DifficultySettings";
            c.knownGood = {
                {"GearGame.DifficultySettings", "1.0"},
                {"GearGame.DifficultySettings_Casual", "2.35"},
                {"GearGame.DifficultySettings_Normal", "1.5"},
                {"GearGame.DifficultySettings_Hardcore", "1.0"},
                {"GearGame.DifficultySettings_Insane", "0.35"},
            };
            c.description =
                "Sets PlayerHealthMod to 32767 in every DifficultySettings section "
                "across all loaded files. Turning this off resets each section to "
                "its correct known-good default (not just whatever value was there "
                "before).";
            v.append(c);
        }
        {
            CheatSpec c;
            c.label = "Infinite Ammo (Weapon Mag Size)";
            c.key = "WeaponMagSize";
            c.onValue = "-1";
            c.sectionContains = "Weap";
            c.knownGood = {
                {"GearGame.GearWeap_AssaultRifle", "50"},
                {"GearGame.GearWeap_BoomshotBase", "1"},
                {"GearGameContent.GearWeap_Boomer_Flail", "1"},
                {"GearGame.GearWeap_BowBase", "1"},
                {"GearGame.GearWeap_COGPistol", "12"},
                {"GearGame.GearWeap_GrenadeBase", "1"},
                {"GearGame.GearWeap_LocustAssaultRifle", "17"},
                {"GearGame.GearWeap_LocustPistol", "6"},
                {"GearGame.GearWeap_LocustBurstPistolBase", "32"},
                {"GearGameContent.GearWeap_LocustBurstPistol_Skorge", "96"},
                {"GearGame.GearWeap_Shotgun", "8"},
                {"GearGame.GearWeap_SniperRifle", "1"},
                {"GearGameContent.GearWeap_Troika", "0"},
                {"GearGameContent.GearWeap_Troika_Raam", "0"},
                {"GearGame.GearWeap_WretchMelee", "1"},
                {"GearGameContent.GearVGearWeap_UVTurret", "12000"},
                {"GearGameContent.GearWeap_FlameThrower", "50"},
                {"GearGameContent.GearWeap_FlameThrower_Turret", "0"},
                {"GearGameContent.GearWeap_HeavyMiniGun", "250"},
                {"GearGameContent.GearWeap_HeavyMortar", "1"},
                {"GearGameContent.GearVWeap_RocketCannon", "6"},
                {"GearGameContent.GearVWeap_ReaverCannon", "1000"},
                {"GearGameContent.GearVWeap_RideReaverCannon", "1000"},
                {"GearGameContent.GearWeap_BrumakSideGun", "0"},
                {"GearGameContent.GearWeap_BrumakMainGun", "0"},
                {"GearGame.GearWeap_BloodMountMelee", "1"},
                {"GearGame.GearWeap_SireMelee", "1"},
                {"GearGame.GearWeap_RockWormMelee", "1"},
                {"GearGame.GearWeap_NemaSlugMelee", "1"},
                {"GearGameContent.GearWeap_SecurityBotGunFlying", "150"},
                {"GearGameContent.GearWeap_SecurityBotGunStationary", "150"},
            };
            c.description =
                "Sets WeaponMagSize to -1 in all weapon sections to enable infinite ammo "
                "without having to reload. Turning this off restores each weapon to its "
                "known-good default magazine size.";
            v.append(c);
        }

        return v;
    }();
    return specs;
}
