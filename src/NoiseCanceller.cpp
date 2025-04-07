#include "NoiseInverter.h"
#include <chrono>
#include <algorithm>
#include <cstring>

// Constructeur
NoiseInverter::NoiseInverter() {
    // Initialiser le tampon de délai (50ms max)
    delayBufferSize = static_cast<size_t>(sampleRate * 0.050);
    delayBuffer.resize(delayBufferSize, 0.0f);
    
    // Initialiser les buffers de visualisation
    vizData.inputSignal.resize(vizBufferSize, 0.0f);
    vizData.outputSignal.resize(vizBufferSize, 0.0f);
    
    // Calculer les coefficients du filtre
    calculateFilterCoefficients();
    
    std::cout << "NoiseInverter initialisé - Optimisé pour Focusrite Firewire" << std::endl;
    std::cout << "Taux d'échantillonnage: " << sampleRate << " Hz" << std::endl;
    std::cout << "Taille du tampon: " << bufferFrames << " échantillons" << std::endl;
}

// Destructeur
NoiseInverter::~NoiseInverter() {
    stop();
}

// Liste les périphériques audio disponibles
std::vector<NoiseInverter::AudioDevice> NoiseInverter::listDevices() {
    std::vector<AudioDevice> deviceList;
    
    // Obtenir les informations sur les périphériques
    unsigned int deviceCount = audio.getDeviceCount();
    std::cout << "Périphériques audio disponibles: " << deviceCount << std::endl;
    
    for (unsigned int i = 0; i < deviceCount; i++) {
        RtAudio::DeviceInfo info = audio.getDeviceInfo(i);
        
        if (info.probed) {
            // Créer une entrée pour le périphérique d'entrée
            if (info.inputChannels > 0) {
                AudioDevice device;
                device.id = i;
                device.name = info.name;
                device.isInput = true;
                device.isOutput = false;
                device.isDefault = info.isDefaultInput;
                device.maxChannels = info.inputChannels;
                device.sampleRates = info.sampleRates;
                
                deviceList.push_back(device);
                
                std::cout << "Périphérique " << i << ": " << info.name << " (Entrée)" << std::endl;
            }
            
            // Créer une entrée pour le périphérique de sortie
            if (info.outputChannels > 0) {
                AudioDevice device;
                device.id = i + 1000;  // +1000 pour différencier entrée/sortie
                device.name = info.name;
                device.isInput = false;
                device.isOutput = true;
                device.isDefault = info.isDefaultOutput;
                device.maxChannels = info.outputChannels;
                device.sampleRates = info.sampleRates;
                
                deviceList.push_back(device);
                
                std::cout << "Périphérique " << i << ": " << info.name << " (Sortie)" << std::endl;
            }
            
            // Afficher les informations détaillées
            std::cout << "  Entrées: " << info.inputChannels 
                      << ", Sorties: " << info.outputChannels << std::endl;
            
            std::cout << "  Fréquences supportées: ";
            for (size_t j = 0; j < std::min(size_t(5), info.sampleRates.size()); j++) {
                std::cout << info.sampleRates[j] << " ";
            }
            if (info.sampleRates.size() > 5) {
                std::cout << "...";
            }
            std::cout << std::endl;
            
            // Vérifier si c'est un périphérique Focusrite
            if (info.name.find("Focusrite") != std::string::npos ||
                info.name.find("Saffire") != std::string::npos) {
                std::cout << "  *** Périphérique Focusrite détecté! ***" << std::endl;
            }
        }
    }
    
    return deviceList;
}

// Démarre le traitement audio
bool NoiseInverter::start(int inputDevice, int outputDevice) {
    if (running) {
        return true;
    }
    
    try {
        // Ajuster les indices
        if (inputDevice >= 1000) {
            inputDevice -= 1000;
        }
        
        if (outputDevice >= 1000) {
            outputDevice -= 1000;
        }
        
        RtAudio::StreamParameters inParams;
        inParams.deviceId = inputDevice;
        inParams.nChannels = 1;  // Mono pour l'entrée
        inParams.firstChannel = 0;
        
        RtAudio::StreamParameters outParams;
        outParams.deviceId = outputDevice;
        outParams.nChannels = 2;  // Stéréo pour la sortie
        outParams.firstChannel = 0;
        
        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_MINIMIZE_LATENCY | RTAUDIO_SCHEDULE_REALTIME;
        options.numberOfBuffers = 2;  // Minimiser les buffers
        options.priority = 85;  // Haute priorité
        options.streamName = "NoiseInverter";
        
        // Ouvrir le stream
        std::cout << "Ouverture du stream audio..." << std::endl;
        std::cout << "  Entrée: " << inputDevice << ", Sortie: " << outputDevice << std::endl;
        std::cout << "  Taux d'échantillonnage: " << sampleRate << " Hz" << std::endl;
        std::cout << "  Taille du tampon: " << bufferFrames << " échantillons" << std::endl;
        
        audio.openStream(&outParams, &inParams, RTAUDIO_FLOAT32,
                       sampleRate, &bufferFrames, &audioCallback,
                       this, &options);
        
        // Démarrer le stream
        audio.startStream();
        
        // Mesurer la latence
        RtAudio::StreamInfo info = audio.getStreamInfo();
        measuredLatency = (info.inputLatency + info.outputLatency) * 1000.0f;
        
        std::cout << "Stream audio démarré avec succès!" << std::endl;
        std::cout << "Taille du tampon effective: " << bufferFrames << " échantillons" << std::endl;
        std::cout << "Latence d'entrée: " << info.inputLatency * 1000.0f << " ms" << std::endl;
        std::cout << "Latence de sortie: " << info.outputLatency * 1000.0f << " ms" << std::endl;
        std::cout << "Latence totale: " << measuredLatency << " ms" << std::endl;
        
        running = true;
        
        // Démarrer un thread pour surveiller la charge CPU
        std::thread monitorThread(&NoiseInverter::cpuMonitorThread, this);
        monitorThread.detach();
        
        return true;
    }
    catch (RtAudioError& e) {
        std::cerr << "Erreur: " << e.getMessage() << std::endl;
        return false;
    }
}

// Arrête le traitement audio
void NoiseInverter::stop() {
    if (running) {
        try {
            running = false;
            if (audio.isStreamRunning()) {
                audio.stopStream();
            }
            if (audio.isStreamOpen()) {
                audio.closeStream();
            }
            
            std::cout << "Stream audio arrêté" << std::endl;
        }
        catch (RtAudioError& e) {
            std::cerr << "Erreur lors de l'arrêt: " << e.getMessage() << std::endl;
        }
    }
}

// Définit les paramètres du traitement
void NoiseInverter::setParameters(float delayMs, float gain, 
                                float lowFreq, float highFreq,
                                FilterType filterType) {
    
    bool needFilterUpdate = false;
    
    if (delayMs >= 0.0f) {
        this->delayMs = delayMs;
    }
    
    if (gain >= 0.0f) {
        this->gain = gain;
    }
    
    if (lowFreq >= 0.0f) {
        this->lowFreq = lowFreq;
        needFilterUpdate = true;
    }
    
    if (highFreq >= 0.0f) {
        this->highFreq = highFreq;
        needFilterUpdate = true;
    }
    
    if (filterType != currentFilterType) {
        currentFilterType = filterType;
        needFilterUpdate = true;
    }
    
    if (needFilterUpdate) {
        calculateFilterCoefficients();
    }
}

// Calibration automatique
std::pair<float, float> NoiseInverter::calibrate() {
    if (!running) {
        return {delayMs, gain};
    }
    
    std::cout << "Calibration en cours..." << std::endl;
    
    // Utiliser la latence mesurée pour estimer le délai
    // On utilise un délai légèrement plus court pour la Focusrite
    delayMs = std::max(1.0f, measuredLatency * 0.5f);  
    
    // Gain adapté à la Focusrite
    gain = 0.92f;
    
    std::cout << "Calibration terminée: délai = " << delayMs << " ms, gain = " << gain << std::endl;
    
    return {delayMs, gain};
}

// Récupère les données pour visualisation
void NoiseInverter::getVisualizationData(std::vector<float>& inputSignal, std::vector<float>& outputSignal) {
    std::lock_guard<std::mutex> lock(vizData.mutex);
    
    inputSignal.resize(vizData.inputSignal.size());
    outputSignal.resize(vizData.outputSignal.size());
    
    std::copy(vizData.inputSignal.begin(), vizData.inputSignal.end(), inputSignal.begin());
    std::copy(vizData.outputSignal.begin(), vizData.outputSignal.end(), outputSignal.begin());
}

// Callback audio statique
int NoiseInverter::audioCallback(void* outputBuffer, void* inputBuffer,
                                unsigned int nFrames, double streamTime,
                                RtAudioStreamStatus status, void* userData) {
    
    NoiseInverter* self = static_cast<NoiseInverter*>(userData);
    return self->processAudio(static_cast<float*>(outputBuffer), 
                             static_cast<float*>(inputBuffer),
                             nFrames);
}

// Traitement audio interne
int NoiseInverter::processAudio(float* outputBuffer, float* inputBuffer, unsigned int nFrames) {
    if (status) {
        std::cerr << "Statut du stream: " << status << std::endl;
    }
    
    // Calcul du délai en échantillons
    int delaySamples = static_cast<int>(delayMs * sampleRate / 1000.0f);
    
    // Bloquer le mutex pour la mise à jour des données visualisées
    {
        std::lock_guard<std::mutex> lock(vizData.mutex);
        
        for (unsigned int i = 0; i < nFrames; i++) {
            // Obtenir l'échantillon d'entrée
            float input = inputBuffer[i];
            
            // Appliquer le filtre
            float filtered = applyFilter(input);
            
            // Inverser le signal filtré
            float inverted = -filtered * gain;
            
            // Stocker dans le tampon de délai
            delayBuffer[delayBufferPos] = inverted;
            
            // Lire l'échantillon retardé
            size_t readPos = (delayBufferPos + delayBufferSize - delaySamples) % delayBufferSize;
            float delayed = delayBuffer[readPos];
            
            // Avancer la position d'écriture
            delayBufferPos = (delayBufferPos + 1) % delayBufferSize;
            
            // Ajouter le signal d'origine et le signal inversé retardé
            float output = input + delayed;
            
            // Limiter la sortie pour éviter l'écrêtage
            output = std::max(-1.0f, std::min(1.0f, output));
            
            // Copier sur les deux canaux (stéréo)
            outputBuffer[i*2] = output;
            outputBuffer[i*2 + 1] = output;
            
            // Mettre à jour les données de visualisation (à une fréquence moins élevée)
            if (i % 2 == 0) {  // Ne mettre à jour qu'un échantillon sur deux
                size_t vizPos = (i / 2) % vizBufferSize;
                vizData.inputSignal[vizPos] = input;
                vizData.outputSignal[vizPos] = output;
            }
        }
    }
    
    // Appeler le callback de mise à jour de l'interface si défini
    if (updateCallback) {
        updateCallback();
    }
    
    return 0;
}

// Calcule les coefficients du filtre selon le type sélectionné
void NoiseInverter::calculateFilterCoefficients() {
    // Fréquences normalisées
    float omega1 = 2.0f * M_PI * lowFreq / sampleRate;
    float omega2 = 2.0f * M_PI * highFreq / sampleRate;
    
    switch (currentFilterType) {
        case BANDPASS:
            // Filtre passe-bande simple du second ordre
            b[0] = 0.25f;
            b[1] = 0.0f;
            b[2] = -0.25f;
            a[0] = 1.0f;
            a[1] = -1.5f;
            a[2] = 0.5f;
            break;
            
        case LOWPASS:
            // Filtre passe-bas simple
            {
                float alpha = std::exp(-omega2);
                b[0] = 1.0f - alpha;
                b[1] = 0.0f;
                b[2] = 0.0f;
                a[0] = 1.0f;
                a[1] = -alpha;
                a[2] = 0.0f;
            }
            break;
            
        case HIGHPASS:
            // Filtre passe-haut simple
            {
                float alpha = std::exp(-omega1);
                b[0] = (1.0f + alpha) / 2.0f;
                b[1] = -(1.0f + alpha) / 2.0f;
                b[2] = 0.0f;
                a[0] = 1.0f;
                a[1] = -alpha;
                a[2] = 0.0f;
            }
            break;
    }
    
    // Réinitialiser les états du filtre
    z1[0] = 0.0f;
    z1[1] = 0.0f;
}

// Applique le filtre IIR
float NoiseInverter::applyFilter(float input) {
    // Équation aux différences du filtre IIR
    float output = b[0] * input + z1[0];
    z1[0] = b[1] * input - a[1] * output + z1[1];
    z1[1] = b[2] * input - a[2] * output;
    
    return output;
}

// Thread de surveillance de la charge CPU
void NoiseInverter::cpuMonitorThread() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        // Attendre 1 seconde
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        try {
            // Mettre à jour la charge CPU si le stream est actif
            if (audio.isStreamRunning()) {
                cpuLoad = audio.getStreamCpuLoad() * 100.0f;
            }
            
            // Vérifier si nous avons toujours une bonne latence
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastTime).count();
            
            // Si plus d'une seconde s'est écoulée, afficher les infos CPU
            if (elapsed > 1000) {
                std::cout << "CPU: " << cpuLoad << "%, Latence: " << measuredLatency << "ms" << std::endl;
                lastTime = currentTime;
            }
        }
        catch (std::exception& e) {
            std::cerr << "Erreur dans le thread de surveillance: " << e.what() << std::endl;
        }
    }
}