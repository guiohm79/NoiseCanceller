#ifndef NOISE_INVERTER_H
#define NOISE_INVERTER_H

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <string>

// Essayez différents chemins d'inclusion pour RtAudio
#if defined(_WIN32) || defined(_WIN64)
    // Essayez d'abord le chemin standard
    #if __has_include(<RtAudio.h>)
        #include <RtAudio.h>
    // Ensuite essayez le chemin avec rtaudio/
    #elif __has_include(<rtaudio/RtAudio.h>)
        #include <rtaudio/RtAudio.h>
    // Si rien ne fonctionne, utilisez un chemin relatif (à adapter)
    #else
        #include "RtAudio.h"
    #endif
#else
    // Pour Linux/macOS
    #if __has_include(<rtaudio/RtAudio.h>)
        #include <rtaudio/RtAudio.h>
    #else
        #include <RtAudio.h>
    #endif
#endif

// Structure pour stocker les données visualisées
struct VisualizationData {
    std::vector<float> inputSignal;
    std::vector<float> outputSignal;
    std::mutex mutex;
};

class NoiseInverter {
public:
    NoiseInverter();
    ~NoiseInverter();

    // Énumération pour le type de filtre
    enum FilterType {
        BANDPASS,
        LOWPASS,
        HIGHPASS
    };

    // Liste des périphériques audio
    struct AudioDevice {
        int id;
        std::string name;
        bool isInput;
        bool isOutput;
        bool isDefault;
        int maxChannels;
        std::vector<unsigned int> sampleRates;
    };

    // Méthodes principales
    std::vector<AudioDevice> listDevices();
    bool start(int inputDevice, int outputDevice);
    void stop();
    void setParameters(float delayMs = -1.0f, float gain = -1.0f, 
                      float lowFreq = -1.0f, float highFreq = -1.0f, 
                      FilterType filterType = BANDPASS);
    std::pair<float, float> calibrate();
    
    // Getters
    float getCpuLoad() const { return cpuLoad; }
    float getLatency() const { return measuredLatency; }
    bool isRunning() const { return running; }
    
    // Récupère les données pour visualisation
    void getVisualizationData(std::vector<float>& inputSignal, std::vector<float>& outputSignal);
    
    // Permet de définir une fonction de callback pour la mise à jour de l'interface
    void setUpdateCallback(std::function<void()> callback) { updateCallback = callback; }

private:
    // Paramètres audio
    unsigned int sampleRate = 96000;
    unsigned int bufferFrames = 16;  // Très petit pour minimiser la latence
    unsigned int nChannels = 2;      // Stéréo
    
    // Paramètres du traitement
    float delayMs = 2.0f;
    float gain = 0.95f;
    float lowFreq = 50.0f;
    float highFreq = 4000.0f;
    FilterType currentFilterType = BANDPASS;
    
    // Buffer circulaire pour le délai
    std::vector<float> delayBuffer;
    size_t delayBufferPos = 0;
    size_t delayBufferSize;
    
    // Données pour visualisation
    VisualizationData vizData;
    size_t vizBufferSize = 1000;
    
    // Instance RtAudio
    RtAudio audio;
    
    // État
    std::atomic<bool> running{false};
    std::atomic<float> cpuLoad{0.0f};
    std::atomic<float> measuredLatency{0.0f};
    
    // Coefficients du filtre IIR (spécifique à Firewire Focusrite)
    float b[3] = {0.25f, 0.0f, -0.25f};  // Numérateur
    float a[3] = {1.0f, -1.5f, 0.5f};    // Dénominateur
    float z1[2] = {0.0f, 0.0f};          // États du filtre
    
    // Callback pour mettre à jour l'interface
    std::function<void()> updateCallback;
    
    // Callback pour le traitement audio
    static int audioCallback(void* outputBuffer, void* inputBuffer,
                            unsigned int nFrames, double streamTime,
                            RtAudioStreamStatus status, void* userData);
    
    // Traitement audio interne
    int processAudio(float* outputBuffer, float* inputBuffer, unsigned int nFrames);
    
    // Calcule les coefficients du filtre
    void calculateFilterCoefficients();
    
    // Application du filtre
    float applyFilter(float input);
    
    // Thread de surveillance de la charge CPU
    void cpuMonitorThread();
};

#endif // NOISE_INVERTER_H