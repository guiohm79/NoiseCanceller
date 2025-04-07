#include "NoiseInverter.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// Fonction helper pour afficher le menu
void afficherMenu() {
    std::cout << "\n=== NoiseInverter Menu ===\n";
    std::cout << "1. Lister les périphériques audio\n";
    std::cout << "2. Démarrer le traitement\n";
    std::cout << "3. Calibrer\n";
    std::cout << "4. Modifier les paramètres\n";
    std::cout << "5. Arrêter\n";
    std::cout << "0. Quitter\n";
    std::cout << "Votre choix: ";
}

int main() {
    std::cout << "=== NoiseInverter - Système d'annulation de bruit ===\n";
    std::cout << "Initialisation...\n";
    
    // Créer l'instance de NoiseInverter
    NoiseInverter inverter;
    
    // Variables pour stocker l'état et les sélections
    int choix = -1;
    int deviceEntree = -1;
    int deviceSortie = -1;
    bool running = false;
    
    // Boucle principale du programme
    while (choix != 0) {
        afficherMenu();
        std::cin >> choix;
        
        switch (choix) {
            case 1: {
                // Lister les périphériques audio
                std::vector<NoiseInverter::AudioDevice> devices = inverter.listDevices();
                
                std::cout << "\n=== Périphériques d'entrée ===\n";
                for (const auto& device : devices) {
                    if (device.isInput) {
                        std::cout << device.id << ": " << device.name;
                        if (device.isDefault) std::cout << " (Défaut)";
                        std::cout << std::endl;
                    }
                }
                
                std::cout << "\n=== Périphériques de sortie ===\n";
                for (const auto& device : devices) {
                    if (device.isOutput) {
                        std::cout << device.id << ": " << device.name;
                        if (device.isDefault) std::cout << " (Défaut)";
                        std::cout << std::endl;
                    }
                }
                
                break;
            }
            
            case 2: {
                // Démarrer le traitement
                if (running) {
                    std::cout << "Le traitement est déjà en cours.\n";
                    break;
                }
                
                // Sélectionner les périphériques si pas encore fait
                if (deviceEntree == -1 || deviceSortie == -1) {
                    std::vector<NoiseInverter::AudioDevice> devices = inverter.listDevices();
                    
                    // Trouver les périphériques par défaut
                    for (const auto& device : devices) {
                        if (device.isInput && device.isDefault) {
                            deviceEntree = device.id;
                        }
                        if (device.isOutput && device.isDefault) {
                            deviceSortie = device.id;
                        }
                    }
                    
                    std::cout << "Périphérique d'entrée (défaut=" << deviceEntree << "): ";
                    int temp;
                    std::cin >> temp;
                    if (temp > 0) deviceEntree = temp;
                    
                    std::cout << "Périphérique de sortie (défaut=" << deviceSortie << "): ";
                    std::cin >> temp;
                    if (temp > 0) deviceSortie = temp;
                }
                
                std::cout << "Démarrage du traitement...\n";
                if (inverter.start(deviceEntree, deviceSortie)) {
                    running = true;
                    std::cout << "Traitement démarré avec succès.\n";
                } else {
                    std::cout << "Échec du démarrage du traitement.\n";
                }
                
                break;
            }
            
            case 3: {
                // Calibrer
                if (!running) {
                    std::cout << "Veuillez d'abord démarrer le traitement.\n";
                    break;
                }
                
                std::cout << "Calibration en cours...\n";
                auto [delay, gain] = inverter.calibrate();
                std::cout << "Calibration terminée : délai = " << delay << " ms, gain = " << gain << "\n";
                
                break;
            }
            
            case 4: {
                // Modifier les paramètres
                if (!running) {
                    std::cout << "Veuillez d'abord démarrer le traitement.\n";
                    break;
                }
                
                float delay, gain, lowFreq, highFreq;
                int filterType;
                
                std::cout << "Nouveau délai (ms, -1 pour laisser inchangé): ";
                std::cin >> delay;
                
                std::cout << "Nouveau gain (0-1, -1 pour laisser inchangé): ";
                std::cin >> gain;
                
                std::cout << "Nouvelle fréquence basse (Hz, -1 pour laisser inchangé): ";
                std::cin >> lowFreq;
                
                std::cout << "Nouvelle fréquence haute (Hz, -1 pour laisser inchangé): ";
                std::cin >> highFreq;
                
                std::cout << "Type de filtre (0=Passe-bande, 1=Passe-bas, 2=Passe-haut, -1 pour laisser inchangé): ";
                std::cin >> filterType;
                
                NoiseInverter::FilterType type = inverter.getCurrentFilterType(); // Corrigé ici
                if (filterType == 0) type = NoiseInverter::BANDPASS;
                else if (filterType == 1) type = NoiseInverter::LOWPASS;
                else if (filterType == 2) type = NoiseInverter::HIGHPASS;
                
                inverter.setParameters(delay, gain, lowFreq, highFreq, type);
                std::cout << "Paramètres mis à jour.\n";
                
                break;
            }
            
            case 5: {
                // Arrêter le traitement
                if (!running) {
                    std::cout << "Le traitement n'est pas en cours.\n";
                    break;
                }
                
                std::cout << "Arrêt du traitement...\n";
                inverter.stop();
                running = false;
                std::cout << "Traitement arrêté.\n";
                
                break;
            }
            
            case 0:
                // Quitter
                if (running) {
                    std::cout << "Arrêt du traitement...\n";
                    inverter.stop();
                }
                std::cout << "Au revoir!\n";
                break;
                
            default:
                std::cout << "Choix invalide. Veuillez réessayer.\n";
                break;
        }
        
        // Montrer les informations de charge CPU si actif
        if (running) {
            std::cout << "Latence: " << inverter.getLatency() << " ms\n";
        }
    }
    
    return 0;
}