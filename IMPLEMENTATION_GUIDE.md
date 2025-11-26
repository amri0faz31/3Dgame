# Implementation Guide: "Echoes of the Lighthouse" - Narrative Horror Survival

## ✅ What You Already Have

Your project already includes:
- ✅ Terrain, water, grass, trees, lighthouse, campfire, hut models
- ✅ Character animation system (Animator, skinned meshes)
- ✅ Fire particle system (GPU-driven, 96 particles)
- ✅ Shadow mapping, day/night cycle, fog
- ✅ Collision system, third-person camera
- ✅ Region-based terrain system

---

## 🎯 Implementation Roadmap

### **PHASE 1: Audio System** (2-3 hours)

#### Step 1.1: Install OpenAL

```bash
# Ubuntu/Debian
sudo apt-get install libopenal-dev

# Arch Linux
sudo pacman -S openal

# Verify installation
ls /usr/include/AL/
```

#### Step 1.2: Update CMakeLists.txt

Add to `src/CMakeLists.txt`:

```cmake
# Find OpenAL
find_package(OpenAL REQUIRED)

# Add audio directory
add_subdirectory(audio)

# Link OpenAL to engine
target_link_libraries(engine PUBLIC ${OPENAL_LIBRARY})
target_include_directories(engine PUBLIC ${OPENAL_INCLUDE_DIR})
```

Create `src/audio/CMakeLists.txt`:

```cmake
target_sources(engine PRIVATE
    AudioSystem.cpp
    AudioSystem.h
)
```

#### Step 1.3: Get Free Sound Files

Download from freesound.org (all CC0/Public Domain):
1. **Ocean waves** → `assets/audio/ocean_waves.wav` (looping ambient)
2. **Fire crackling** → `assets/audio/fire_crackle.wav` (looping, 3D positional)
3. **Wind ambience** → `assets/audio/wind.wav` (looping)
4. **Paper pickup** → `assets/audio/paper_pickup.wav` (one-shot)
5. **Monster growl** → `assets/audio/monster_growl.wav` (occasional)
6. **Torch ignite** → `assets/audio/torch_light.wav` (one-shot)

Convert to WAV PCM format if needed:
```bash
ffmpeg -i input.mp3 -acodec pcm_s16le -ar 44100 output.wav
```

#### Step 1.4: Initialize Audio in Game::init()

Add to `Game.cpp` init():

```cpp
// Initialize audio system
if(!m_audio.init()){
    std::cerr << "[Game] Failed to initialize audio" << std::endl;
}

// Load ambient sounds
m_soundOcean = m_audio.loadSound("assets/audio/ocean_waves.wav", SoundType::Ambient);
m_soundWind = m_audio.loadSound("assets/audio/wind.wav", SoundType::Ambient);

// Load positional sounds
m_soundFire = m_audio.loadSound("assets/audio/fire_crackle.wav", SoundType::Positional);

// Load effect sounds
m_soundPickup = m_audio.loadSound("assets/audio/paper_pickup.wav", SoundType::Effect);
m_soundMonster = m_audio.loadSound("assets/audio/monster_growl.wav", SoundType::Effect);

// Start ambient ocean loop
m_audio.playSound(m_soundOcean, 0.3f, true);
m_audio.playSound(m_soundWind, 0.15f, true);

// Start campfire 3D sound
m_audio.playSoundAt(m_soundFire, m_campfirePosition, 0.6f, 40.0f, true);
```

#### Step 1.5: Update Audio Listener in Game::update()

```cpp
void Game::update(){
    // ... existing code ...
    
    // Update audio listener position
    glm::vec3 camPos = m_camera->position();
    glm::vec3 forward = glm::normalize(/* camera forward vector */);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    m_audio.setListenerPosition(camPos, forward, up);
}
```

---

### **PHASE 2: Interaction System** (3-4 hours)

#### Step 2.1: Add to CMakeLists

Update `src/systems/CMakeLists.txt`:

```cmake
target_sources(engine PRIVATE
    CollisionSystem.cpp
    CollisionSystem.h
    InteractionSystem.cpp
    InteractionSystem.h
    MonsterAI.cpp
    MonsterAI.h
)
```

#### Step 2.2: Setup Interactables in Game::init()

Add new function `Game::setupGameplayElements()`:

```cpp
void Game::setupGameplayElements(){
    // Note near forest hut
    Interactable note;
    note.type = InteractableType::Note;
    note.position = m_forestHutPosition + glm::vec3(3.0f, 0.5f, 2.0f);
    note.radius = 2.5f;
    note.message = "Getting dark... go to the lighthouse before the monster comes. Light is your saviour.";
    note.onInteract = [this](){ onNotePickup(); };
    m_interactions.addInteractable(note);
    
    // Torch pickup near campfire
    Interactable torch;
    torch.type = InteractableType::TorchPickup;
    torch.position = m_campfirePosition + glm::vec3(1.5f, 0.3f, 0.0f);
    torch.radius = 2.0f;
    torch.message = "Press E to pick up torch stick";
    torch.onInteract = [this](){ onTorchPickup(); };
    m_interactions.addInteractable(torch);
    
    // Fire source for lighting torch
    Interactable fireSource;
    fireSource.type = InteractableType::FireSource;
    fireSource.position = m_campfirePosition;
    fireSource.radius = 3.0f;
    fireSource.message = "Press E to light torch";
    fireSource.onInteract = [this](){ onTorchLight(); };
    m_interactions.addInteractable(fireSource);
}

void Game::onNotePickup(){
    m_hasNote = true;
    m_audio.playSound(m_soundPickup, 0.7f, false);
    std::cout << "[Game] Note picked up!" << std::endl;
    // Remove note from world
    m_interactions.removeInteractable(0); // Assuming note is first interactable
}

void Game::onTorchPickup(){
    m_hasTorch = true;
    m_audio.playSound(m_soundPickup, 0.6f, false);
    std::cout << "[Game] Torch picked up!" << std::endl;
    m_interactions.removeInteractable(1);
}

void Game::onTorchLight(){
    if(m_hasTorch && !m_torchLit){
        m_torchLit = true;
        m_audio.playSound(/* torch ignite sound */, 0.8f, false);
        std::cout << "[Game] Torch is now lit!" << std::endl;
    }
}
```

#### Step 2.3: Check for Interactions in Game::update()

```cpp
void Game::checkInteractions(){
    glm::vec3 playerPos = m_characterController.position;
    m_nearestInteractableID = m_interactions.findNearestInteractable(playerPos, 3.0f);
    
    // Check for E key press
    if(m_nearestInteractableID >= 0 && glfwGetKey(/* window */, GLFW_KEY_E) == GLFW_PRESS){
        m_interactions.interact(m_nearestInteractableID);
    }
}
```

---

### **PHASE 3: Monster Patrol AI** (2-3 hours)

#### Step 3.1: Define Patrol Path in Game::init()

```cpp
// Setup monster patrol route (circle around island)
std::vector<PatrolPoint> monsterPath = {
    { glm::vec3(-100.0f, 10.0f, -120.0f), 2.0f },  // North corner
    { glm::vec3(120.0f, 10.0f, -100.0f), 1.5f },   // East corner
    { glm::vec3(100.0f, 10.0f, 150.0f), 2.0f },    // South corner
    { glm::vec3(-120.0f, 10.0f, 140.0f), 1.5f },   // West corner
};
m_monster.setPatrolPath(monsterPath);
m_monster.setMoveSpeed(4.0f);  // Slightly faster than player
```

#### Step 3.2: Load Monster Model

Add to `Game::init()`:

```cpp
// Load monster model (use character mesh as placeholder or load new model)
std::string monsterPath = resolveExistingPath({
    "assets/models/monster.fbx",
    "assets/models/creature.glb",
    "../assets/models/monster.fbx"
});

if(!monsterPath.empty()){
    m_monsterReady = loadStaticModel(monsterPath, m_monsterMesh);
    if(m_monsterReady){
        std::cout << "[Game] Monster model loaded" << std::endl;
    }
}
```

#### Step 3.3: Update Monster AI in Game::update()

```cpp
void Game::updateGameplay(float dt){
    // Update monster patrol
    m_monster.update(dt);
    
    // Check if monster detects player
    checkMonsterDetection();
    
    // Check if player reached lighthouse (safe zone)
    float distToLighthouse = glm::distance(m_characterController.position, m_lighthousePosition);
    if(distToLighthouse < 15.0f && m_torchLit){
        m_reachedLighthouse = true;
        std::cout << "[Game] YOU WIN! Reached the lighthouse safely!" << std::endl;
    }
}

void Game::checkMonsterDetection(){
    if(m_gameLost) return;
    
    // Monster only detects player if torch is NOT lit
    float detectionRadius = m_torchLit ? 3.0f : 12.0f;
    
    if(m_monster.detectsPlayer(m_characterController.position, detectionRadius)){
        m_gameLost = true;
        m_audio.playSound(m_soundMonster, 1.0f, false);
        std::cout << "[Game] GAME OVER! The monster caught you!" << std::endl;
        // Optional: reset game after 3 seconds
    }
}
```

#### Step 3.4: Render Monster in Game::render()

```cpp
// Render monster (in main render loop after other models)
if(m_monsterReady && m_characterShader && !m_monsterMesh.parts.empty()){
    glm::mat4 monsterModel = glm::mat4(1.0f);
    monsterModel = glm::translate(monsterModel, m_monster.getPosition());
    monsterModel = glm::rotate(monsterModel, glm::radians(m_monster.getYaw()), glm::vec3(0.0f, 1.0f, 0.0f));
    monsterModel = glm::scale(monsterModel, glm::vec3(2.0f)); // Scale as needed
    
    m_characterShader->bind();
    m_characterShader->setBool("uUseSkinning", false);
    m_characterShader->setMat4("uModel", monsterModel);
    // ... set other uniforms (lights, shadows, camera) ...
    
    for(const auto& part : m_monsterMesh.parts){
        glBindTexture(GL_TEXTURE_2D, part.albedoTex);
        glBindVertexArray(part.vao);
        glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}
```

---

### **PHASE 4: Rotating Lighthouse Beacon** (30 minutes)

#### Step 4.1: Update Beacon Rotation in Game::update()

```cpp
void Game::update(){
    // ... existing code ...
    
    // Rotate beacon (180 degrees per 10 seconds = 18 deg/sec)
    m_beaconRotation += 18.0f * Time::deltaTime();
    if(m_beaconRotation >= 360.0f) m_beaconRotation -= 360.0f;
}
```

#### Step 4.2: Apply Rotation to Lighthouse Render

```cpp
// In Game::render(), modify lighthouse rendering:
if(m_lighthouseReady && m_characterShader && !m_lighthouseMesh.parts.empty()){
    glm::mat4 lighthouseModel = glm::mat4(1.0f);
    lighthouseModel = glm::translate(lighthouseModel, m_lighthousePosition);
    
    // Apply beacon rotation to top part (if multi-part model)
    // If entire lighthouse rotates:
    lighthouseModel = glm::rotate(lighthouseModel, glm::radians(m_beaconRotation), glm::vec3(0.0f, 1.0f, 0.0f));
    
    lighthouseModel = glm::scale(lighthouseModel, glm::vec3(m_lighthouseScale));
    
    // ... rest of rendering code ...
}
```

#### Step 4.3: Add Spotlight Effect (Optional)

Create a cone spotlight shader that rotates with the beacon:

```cpp
// In fragment shader, add spotlight calculation
uniform vec3 uSpotlightPos;
uniform vec3 uSpotlightDir;
uniform float uSpotlightCutoff;  // cos(angle)

// In render code:
m_shader->setVec3("uSpotlightPos", m_lighthousePosition + glm::vec3(0.0f, 30.0f, 0.0f));
float rotRad = glm::radians(m_beaconRotation);
glm::vec3 spotDir(std::sin(rotRad), -0.3f, std::cos(rotRad));
m_shader->setVec3("uSpotlightDir", glm::normalize(spotDir));
m_shader->setFloat("uSpotlightCutoff", std::cos(glm::radians(30.0f)));
```

---

### **PHASE 5: UI & Feedback** (1-2 hours)

#### Step 5.1: Add UI Rendering Function

```cpp
void Game::renderUI(){
    #ifdef BUILD_IMGUI
    ImGui::Begin("Gameplay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    ImGui::SetWindowPos(ImVec2(10, 10));
    ImGui::SetWindowSize(ImVec2(300, 150));
    
    if(m_hasNote){
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Objective:");
        ImGui::TextWrapped("Reach the lighthouse with a lit torch!");
    }
    
    ImGui::Separator();
    ImGui::Text("Inventory:");
    if(m_hasTorch){
        if(m_torchLit){
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[Torch - LIT]");
        } else {
            ImGui::Text("[Torch - unlit]");
        }
    }
    
    if(m_nearestInteractableID >= 0){
        const Interactable* obj = m_interactions.getInteractable(m_nearestInteractableID);
        if(obj){
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[E] %s", obj->message.c_str());
        }
    }
    
    if(m_gameLost){
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "GAME OVER!");
        ImGui::Text("The monster caught you...");
        ImGui::Text("Press R to restart");
    }
    
    if(m_reachedLighthouse){
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "YOU WIN!");
        ImGui::Text("You reached safety!");
    }
    
    ImGui::End();
    #endif
}
```

---

## 🔧 Build Instructions

### Step 1: Rebuild CMake

```bash
cd /home/amri-fazlul/3D_world
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### Step 2: Compile

```bash
cmake --build build -j$(nproc)
```

### Step 3: Run

```bash
cd build
./src/lighthouse
```

---

## 🎮 Gameplay Flow

1. **Start**: Player spawns near forest hut
2. **Discover Note**: Find paper on ground → reads warning message
3. **Find Torch**: Locate stick near campfire
4. **Light Torch**: Interact with campfire flames
5. **Navigate**: Move toward lighthouse while avoiding monster
6. **Monster Patrol**: Creature follows waypoint circuit (audible footsteps/growls)
7. **Detection**: Monster sees player from 12m away (3m if torch lit)
8. **Win Condition**: Reach lighthouse with lit torch
9. **Lose Condition**: Monster catches player

---

## 📝 Documentation Points

For your design document, emphasize:

### Model Forms & Data Structures
- **Polygonal Models**: Lighthouse, hut, terrain (vertex buffer layout: pos3+norm3+uv2+tangent3)
- **Skeletal Animation**: Monster patrol (bone hierarchy, transformation matrices)
- **Particle System**: Fire (billboard quads, GPU instancing, lifecycle management)
- **Parametric Textures**: Terrain biome blending (weight functions), water animation (time-based UV scrolling)

### Systems Architecture
- **Audio System**: OpenAL spatial audio with distance attenuation (inverse-square law)
- **Interaction System**: Proximity-based triggers (radius checks, callback dispatch)
- **AI System**: Waypoint-based pathfinding with smooth rotation interpolation
- **Collision Detection**: AABB/sphere tests for obstacle avoidance

### Lighting Pipeline
- **Shadow Mapping**: Directional light + depth FBO (4096×4096, PCF filtering)
- **Point Lights**: Campfire flicker (sinusoidal intensity modulation)
- **Spotlight**: Rotating beacon (cone attenuation, fragment shader calculation)

---

## ⏱️ Time Estimate

| Phase | Task | Duration |
|-------|------|----------|
| 1 | Audio system integration | 2-3 hours |
| 2 | Interaction system | 3-4 hours |
| 3 | Monster AI patrol | 2-3 hours |
| 4 | Beacon rotation + spotlight | 1 hour |
| 5 | UI & polish | 1-2 hours |
| **Total** | | **9-13 hours** |

---

## 🎬 Video Showcase Script (5 minutes)

1. **Opening (30s)**: Fade in on stormy island, pan to lighthouse
2. **Discovery (60s)**: Player finds note, picks up torch
3. **Environment (60s)**: Show day/night toggle, fire particles, grass sway
4. **Tension (90s)**: Monster patrol, player sneaking, torch lighting
5. **Climax (45s)**: Sprint to lighthouse with monster chasing
6. **Win/Lose (30s)**: Show both endings
7. **Technical (45s)**: Overlay showing polygon counts, shader effects, audio zones

---

## ✅ Final Checklist

Before submission:
- [ ] All sounds playing correctly (positional audio working)
- [ ] Interactions trigger properly (note, torch, fire source)
- [ ] Monster patrols and detects player
- [ ] Beacon rotates visibly
- [ ] Win/lose conditions tested
- [ ] UI shows objectives and feedback
- [ ] Documentation covers all model forms and data structures
- [ ] Video captured and edited (5 minutes max)
- [ ] Build executable runs on clean machine
- [ ] All asset files included in submission

---

**Good luck! Your narrative design is excellent and will score very well on environmental storytelling and player agency criteria. The technical implementation is straightforward given your existing foundation.** 🚀
