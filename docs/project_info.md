# kappa_engine info

## Objective
Make a proof of concept 3D open world engine with some simple physics

## Relevant systems

### Resource loading
An asynchronous resource loading system, in the sense that it doesn't block the main thread,

The basic idea is to:
1. Define the resources that will be needed when the scene is loaded
2. Add a request for each resource to a queue
3. Load them in a threadpool. The requesting scene can show a loading screen or something.
4. Once everything loads, send them to the scene manager

#### List of external assets
- 2D sprites
    - In this case, a sprite is defined to be a single image or a series of images loaded from
    an image atlas
    - For characters, UI elements, effects, and any kind of 2D animation or collection of images
    - CHIMA spritesheet format
- Static 3D models
    - Scenery and static props
    - With materials
    - GLTF format
- Rigged 3D models
    - With bone and mesh animations
    - With materials
    - For characters and animated props
    - GLTF format
- Textures
    - Any single image that comes from a single image file (not an atlas!)
    - For backgrounds and skyboxes
    - PNG + json metadata (maybe add a new formats to CHIMA?)
- Fonts
    - For text (lol)
    - Whatever format FreeType loads (TTF or OTF)
- Scene metadata
    - Controls the scene events and sets the initial state
    - Maybe a Lua script?

### Scene management
Some thing that can manage scenes and transitions
1. Load a scene list containing scene names, resource lists and scripts for each one
2. When the game starts, load an initial scene from the list.
3. When the scene starts, call the resource loading system with the asset list and wait
4. When the resources arrive, call the scene set up script.
5. Handle all events that the scene sets up (TODO: Define events)
6. When the scene ends, handle loading the next scene

### Animation system
There has to be some kind of animation system, preferrably instanced for each game object
instead of a global system.

If going for the isntanced way, the animator should be hooked to the object update event, and
updated last after the physics system. Additionally, it has to provide the animation data
on the render loop instead of the fixed tick loop. This is in order for it to be interpolated
according to the host FPS.

There are two main animation types: 2D and 3D

#### 2D animation
A 2D animation is just the playback of some sprite animation and the transition graph between
them.

Each animated object has some state that keeps track of:
- The animation loop time
- The animation current time
- The animation frames

The system just loops the given animation indefinetely until a state change is triggered and the
animation state changes, or the current animation is paused. For now, the animation can be
paused, rewinded or played forward.

> [!NOTE]
> Check out the Taisei animation system, and implement something similar.

So, in general, the state that each animated object instance needs are:
- Animation asset pointer (lists of total time, frames, ...)
- Current animation index
- Current animation time
- Current texture (uvs and pointer). Upload to shader

#### 3D animation
For animating models, the system should be divided in two parts: skeletal animations and mesh
animations.

##### Mesh animation
The simpler one is (should be) the mesh animation. Each model that has an animation in a mesh
provides a set of discrete animation frames for the mesh (just two for a given animation in this
first implementation; base mesh and animated mesh) and then the vertex shader interpolates the
mesh vertices. Multiple animations can be triggered at the same time, the ammount varies depending
on the animations that the mesh provides.

For example, lets say there is a model named `koishi` that, for the mesh `head` provides the
following animations:
- `mouth_smile`
- `mouth_sad`
- `eyes_scared`

The default mesh state will be named `head_default`. Each animation state (including the
default mesh) state gets assigned an alpha value. The vertex shader uses this alpha value
to interpolate between meshes.

The alpha value can be anything between 0.0 and 1.0, as long as the sum for all alpha values for
all animations is == 1.0. For example:
- `head_default: 0.25`
- `mouth_smile: 0.0`
- `mouth_sad: 0.5`
- `eyes_scared: 0.5`

The state that each instance will need is:
- Animation asset pointer (list of meshes)
- For each relevant mesh, an alpha value. Upload to shader

##### Skeletal animation
The skeletal animation involves having a vertex configuration where each mesh provides, at least:
- `vertex_position`: As usual
- `vertex_bone_index[4]`: Pointers to the bones that affect this vertex transform
- `vertex_weights[4]`: How much each bone influences this specific vertex. The sum of the 4 weights
**should** be 1.0

Additionally, a bone hierarchy is provided each frame to calculate the vertex transforms. This
bone hierarchy is just a `mat4` array that provides all the bones specified in each vertex
`vertex_bone_index`

This bone hierachy follows a transform hierarchy. The root bone is located at `bones[0]`, and it
**HAS** to be sorted from root to leafs. For example, in order:

- root: id 0
    - hips: id 1
        - upper_body: id 2
            - left_arm: id 3
            - right_arm: id 4
        - lower_body: id 5
            - left_leg: id 6
            - right_leg  id 7

In the vertex shader, each transform for each bone referenced in `vertex_bone_index[4]` gets
scaled according to `vertex_weights[4]` and used to create a transformation matrix for the
current vertex positions.

Each instance will need the following state:
- Animation asset pointer (bone hierarchy, rest state bone transforms, inverse transform matrices,
local transform matrices)
- Transform output (array of `mat4`). Upload to shader.
