# MBK De-noiser PRO  (v2.0)

Réducteur de bruit spectral **VST3** (Windows & macOS), construit avec [JUCE](https://juce.com).

## Fonctions

- **Analyseur de spectre en temps réel** : entrée, sortie et seuil de bruit (la zone grisée = hors bande traitée)
- **Reduction / Sensitivity / Smoothing** : l'essentiel pour régler en quelques secondes
- **Band Low / Band High** : limite le traitement à une plage de fréquences
- **Mode Auto** : le bruit de fond est suivi automatiquement
- **Mode Learned + LEARN NOISE** : appuyez, jouez uniquement le bruit, rappuyez. Le profil est **sauvegardé avec le projet**
- **NOISE ONLY** : écoute ce qui est retiré, pour vérifier que rien d'utile n'est perdu
- **8 presets** (voix, podcast, souffle, ronflement, musique...), **bypass**, mix, gain de sortie
- Interface **redimensionnable**, infobulles sur chaque commande, double-clic = valeur par défaut
- Algorithme : STFT 2048 / recouvrement 75 %, filtre de Wiener à décision dirigée (peu de « bruit musical »)

Latence : 2048 échantillons (déclarée à l'hôte, compensation automatique).

## Compiler en local

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MBKDenoiserPro_VST3
```

## Publier une version (installeurs automatiques)

Créez une *Release* avec un tag `v2.0.0` : GitHub Actions compile et ajoute

| Fichier | Contenu |
|---|---|
| `MBK-De-noiser-PRO-2.0.0-Windows-Setup.exe` | Installeur → `C:\Program Files\Common Files\VST3\` |
| `MBK-De-noiser-PRO-2.0.0-macOS.pkg` | Installeur → `/Library/Audio/Plug-Ins/VST3/` |

> Les installeurs ne sont pas signés (SmartScreen / Gatekeeper afficheront un avertissement).
> Pour une diffusion grand public : certificat de signature de code (Windows) et Apple Developer ID + notarisation (macOS).
