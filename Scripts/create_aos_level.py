#!/usr/bin/env python3
"""
Unreal Engine Python Script: Create AOS Test Level
This script automatically creates a new level with MapManager and configures GameMode
"""

import unreal

def create_aos_level():
    """Create and configure the AOS test level"""

    print("=== Creating AOS Test Level ===")

    # Level name and path
    level_path = "/Game/AOS/Lvl_AOS_Test"
    level_name = "Lvl_AOS_Test"

    # Create new level
    editor_lib = unreal.EditorLevelLibrary()
    editor_util = unreal.EditorAssetLibrary()

    try:
        # Create new level with a blank template
        level = editor_lib.new_level(level_name)
        print(f"✅ Created new level: {level_name}")

        # Load the level
        editor_lib.load_level(level_path)
        print(f"✅ Loaded level: {level_path}")

        # Spawn MapManager
        spawn_loc = unreal.Vector(0, 0, 0)
        map_manager_class = unreal.load_asset("/Script/TDProject.AOSMapManager")

        if map_manager_class:
            map_manager = editor_lib.spawn_actor_from_class(
                map_manager_class,
                spawn_loc,
                unreal.Rotator(0, 0, 0)
            )
            print(f"✅ Spawned MapManager at {spawn_loc}")
        else:
            print("❌ Failed to load AOSMapManager class")
            return False

        # Set World Settings GameMode
        world = unreal.get_editor_world()
        world_settings = unreal.get_default_obj(unreal.WorldSettings)

        aos_gamemode_class = unreal.load_asset("/Script/TDProject.AOSGameMode")
        if aos_gamemode_class:
            world_settings.default_game_mode = aos_gamemode_class
            print(f"✅ Set GameMode to AOSGameMode")
        else:
            print("❌ Failed to load AOSGameMode class")
            return False

        # Save the level
        editor_util.save_asset(level_path)
        print(f"✅ Saved level: {level_path}")

        print("\n=== Level Creation Complete ===")
        print(f"Map: {level_path}")
        print(f"GameMode: AOSGameMode")
        print(f"MapManager: Spawned at (0, 0, 0)")
        print("\nNext: Press Play (Alt+P) to see the towers!")

        return True

    except Exception as e:
        print(f"❌ Error creating level: {str(e)}")
        return False

# Execute
if __name__ == "__main__":
    create_aos_level()
