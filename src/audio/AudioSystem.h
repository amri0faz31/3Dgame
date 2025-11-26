#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

// Forward declare OpenAL types to avoid header dependency
typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
typedef unsigned int ALuint;

enum class SoundType {
    Ambient,      // Looping background (ocean, wind)
    Effect,       // One-shot effects (pickup, footstep)
    Positional    // 3D spatial sound (fire crackling)
};

struct SoundSource {
    ALuint source = 0;
    ALuint buffer = 0;
    SoundType type = SoundType::Effect;
    bool isPlaying = false;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float maxDistance = 50.0f;
    bool looping = false;
};

class AudioSystem {
public:
    bool init();
    void shutdown();
    
    // Load sound from WAV file, returns sound ID
    int loadSound(const std::string& path, SoundType type = SoundType::Effect);
    
    // Play sound (returns source ID for tracking)
    int playSound(int soundID, float volume = 1.0f, bool loop = false);
    
    // Play 3D positional sound
    int playSoundAt(int soundID, const glm::vec3& position, float volume = 1.0f, 
                    float maxDistance = 50.0f, bool loop = false);
    
    // Update listener position (camera/player)
    void setListenerPosition(const glm::vec3& position, const glm::vec3& forward, 
                            const glm::vec3& up);
    
    // Update sound source position (for moving sounds)
    void updateSoundPosition(int sourceID, const glm::vec3& position);
    
    // Stop specific sound
    void stopSound(int sourceID);
    
    // Stop all sounds
    void stopAll();
    
    // Set master volume (0.0 to 1.0)
    void setMasterVolume(float volume);

private:
    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;
    std::vector<SoundSource> m_sources;
    float m_masterVolume = 1.0f;
    
    bool loadWAV(const std::string& path, ALuint& outBuffer);
};
