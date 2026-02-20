# Asset loader thingy
- Load things like in gmod
- Show a list of models and create a new instance on click
- Could be scripted

## Basic things
- Textures
- Sounds

## Composed things
- Materials
  - Depends on textures
- Player (rigged) models
  - Depends on materials
- Static models
  - Depends on materials
- Sprites (spritesheets)
  - Depends on textures

## Things that have to be loaded in the renderer thread
- Textures
- Pipelines (from materials)
- Vertex buffers (from models)

## Things that can be loaded multithreaded
- Images (texels)
- Spritesheets (chima)
- Sound files
- Material metadata
- All model data minus materials' textures

## Caveats
- Some things might reference already loaded assets
- Plus i want to be able to use any asset on any other place
- Ex: use the material for cirno in a random cube

# The plan
I want to have an asset singleton that manages these 7 types of assets. I want to do something
similar to the danmaku engine where i define every asset in a config file and just reference them
in code (in another script?) or in a GUI

I define the paths for every asset in the config file, and just load items whenever i need them.
Some assets might need two pass loading (the ones that have things on the renderer thread).
