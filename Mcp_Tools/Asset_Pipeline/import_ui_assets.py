import unreal
import sys
import os

def import_ui_assets(source_file, destination_path="/Game/AOS/UI/Assets"):
    if not os.path.exists(source_file):
        unreal.log_error(f"Source file does not exist: {source_file}")
        return False
        
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.automated = True
    task.replace_existing = True
    task.save = True
    
    # Configure texture import settings for UI
    task.options = unreal.TextureFactory()
    
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])
    
    imported_asset_paths = task.get_editor_property("imported_object_paths")
    if not imported_asset_paths:
        unreal.log_error(f"Failed to import {source_file}")
        return False
        
    asset_path = imported_asset_paths[0]
    unreal.log(f"Successfully imported asset to {asset_path}")
    
    # Set Texture Group to UI, NoMipmaps, and EditorIcon compression
    loaded_texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if isinstance(loaded_texture, unreal.Texture2D):
        loaded_texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        loaded_texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        loaded_texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        unreal.EditorAssetLibrary.save_asset(asset_path)
        unreal.log(f"Configured texture settings for UI: {asset_path}")
        
    return True

# Ensure the function can be called via eval/exec from MCP execute_script
if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) > 0:
        src = args[0]
        dest = args[1] if len(args) > 1 else "/Game/AOS/UI/Assets"
        import_ui_assets(src, dest)
