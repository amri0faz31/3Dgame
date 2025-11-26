#include "AudioSystem.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <iostream>
#include <fstream>
#include <cstring>

// Simple WAV file parser (PCM format only)
struct WAVHeader {
    char riff[4];
    uint32_t size;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t format;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

bool AudioSystem::init(){
    m_device = alcOpenDevice(nullptr);
    if(!m_device){
        std::cerr << "[Audio] Failed to open OpenAL device" << std::endl;
        return false;
    }
    
    m_context = alcCreateContext(m_device, nullptr);
    if(!m_context){
        std::cerr << "[Audio] Failed to create OpenAL context" << std::endl;
        alcCloseDevice(m_device);
        m_device = nullptr;
        return false;
    }
    
    alcMakeContextCurrent(m_context);
    
    // Set distance model for 3D sound attenuation
    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    
    std::cout << "[Audio] OpenAL initialized: " 
              << alGetString(AL_RENDERER) << std::endl;
    return true;
}

void AudioSystem::shutdown(){
    stopAll();
    
    for(auto& src : m_sources){
        if(src.source) alDeleteSources(1, &src.source);
        if(src.buffer) alDeleteBuffers(1, &src.buffer);
    }
    m_sources.clear();
    
    if(m_context){
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_context);
        m_context = nullptr;
    }
    if(m_device){
        alcCloseDevice(m_device);
        m_device = nullptr;
    }
    std::cout << "[Audio] Shutdown complete" << std::endl;
}

bool AudioSystem::loadWAV(const std::string& path, ALuint& outBuffer){
    std::ifstream file(path, std::ios::binary);
    if(!file){
        std::cerr << "[Audio] Failed to open: " << path << std::endl;
        return false;
    }
    
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    
    if(std::strncmp(header.riff, "RIFF", 4) != 0 ||
       std::strncmp(header.wave, "WAVE", 4) != 0){
        std::cerr << "[Audio] Invalid WAV file: " << path << std::endl;
        return false;
    }
    
    // Skip to data chunk
    char chunkID[4];
    uint32_t chunkSize;
    while(file.read(chunkID, 4)){
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if(std::strncmp(chunkID, "data", 4) == 0){
            break;
        }
        file.seekg(chunkSize, std::ios::cur);
    }
    
    std::vector<char> data(chunkSize);
    file.read(data.data(), chunkSize);
    
    ALenum format = 0;
    if(header.channels == 1 && header.bitsPerSample == 8)
        format = AL_FORMAT_MONO8;
    else if(header.channels == 1 && header.bitsPerSample == 16)
        format = AL_FORMAT_MONO16;
    else if(header.channels == 2 && header.bitsPerSample == 8)
        format = AL_FORMAT_STEREO8;
    else if(header.channels == 2 && header.bitsPerSample == 16)
        format = AL_FORMAT_STEREO16;
    else{
        std::cerr << "[Audio] Unsupported WAV format: " << path << std::endl;
        return false;
    }
    
    alGenBuffers(1, &outBuffer);
    alBufferData(outBuffer, format, data.data(), chunkSize, header.sampleRate);
    
    if(alGetError() != AL_NO_ERROR){
        std::cerr << "[Audio] Failed to buffer audio data: " << path << std::endl;
        return false;
    }
    
    std::cout << "[Audio] Loaded: " << path 
              << " (" << header.channels << "ch, " 
              << header.sampleRate << "Hz)" << std::endl;
    return true;
}

int AudioSystem::loadSound(const std::string& path, SoundType type){
    SoundSource src;
    src.type = type;
    
    if(!loadWAV(path, src.buffer)){
        return -1;
    }
    
    m_sources.push_back(src);
    return static_cast<int>(m_sources.size() - 1);
}

int AudioSystem::playSound(int soundID, float volume, bool loop){
    if(soundID < 0 || soundID >= static_cast<int>(m_sources.size()))
        return -1;
    
    auto& src = m_sources[soundID];
    
    if(!src.source){
        alGenSources(1, &src.source);
        alSourcei(src.source, AL_BUFFER, src.buffer);
    }
    
    alSourcef(src.source, AL_GAIN, volume * m_masterVolume);
    alSourcei(src.source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcei(src.source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(src.source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    
    alSourcePlay(src.source);
    src.isPlaying = true;
    src.looping = loop;
    
    return soundID;
}

int AudioSystem::playSoundAt(int soundID, const glm::vec3& position, 
                              float volume, float maxDistance, bool loop){
    if(soundID < 0 || soundID >= static_cast<int>(m_sources.size()))
        return -1;
    
    auto& src = m_sources[soundID];
    
    if(!src.source){
        alGenSources(1, &src.source);
        alSourcei(src.source, AL_BUFFER, src.buffer);
    }
    
    src.position = position;
    src.volume = volume;
    src.maxDistance = maxDistance;
    
    alSourcef(src.source, AL_GAIN, volume * m_masterVolume);
    alSourcei(src.source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcei(src.source, AL_SOURCE_RELATIVE, AL_FALSE);
    alSource3f(src.source, AL_POSITION, position.x, position.y, position.z);
    alSourcef(src.source, AL_REFERENCE_DISTANCE, 5.0f);
    alSourcef(src.source, AL_MAX_DISTANCE, maxDistance);
    
    alSourcePlay(src.source);
    src.isPlaying = true;
    src.looping = loop;
    
    return soundID;
}

void AudioSystem::setListenerPosition(const glm::vec3& position, 
                                      const glm::vec3& forward, 
                                      const glm::vec3& up){
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    float orientation[6] = {
        forward.x, forward.y, forward.z,
        up.x, up.y, up.z
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioSystem::updateSoundPosition(int sourceID, const glm::vec3& position){
    if(sourceID < 0 || sourceID >= static_cast<int>(m_sources.size()))
        return;
    
    auto& src = m_sources[sourceID];
    if(src.source){
        src.position = position;
        alSource3f(src.source, AL_POSITION, position.x, position.y, position.z);
    }
}

void AudioSystem::stopSound(int sourceID){
    if(sourceID < 0 || sourceID >= static_cast<int>(m_sources.size()))
        return;
    
    auto& src = m_sources[sourceID];
    if(src.source && src.isPlaying){
        alSourceStop(src.source);
        src.isPlaying = false;
    }
}

void AudioSystem::stopAll(){
    for(auto& src : m_sources){
        if(src.source && src.isPlaying){
            alSourceStop(src.source);
            src.isPlaying = false;
        }
    }
}

void AudioSystem::setMasterVolume(float volume){
    m_masterVolume = glm::clamp(volume, 0.0f, 1.0f);
    alListenerf(AL_GAIN, m_masterVolume);
}
