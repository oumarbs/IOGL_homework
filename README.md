# IOGL Homework : fête foraine de nuit

Scène 3D en OpenGL 3.3.
Une petite fête foraine la nuit : un manège avec des lapins, un stand, une piste de bowling, des guirlandes
et un feu d'artifice. Au lancement la caméra fait une visite automatique de la scène (environ 55 s, en boucle).

**Vidéo (compilation + visite virtuelle) :** https://1drv.ms/f/c/18aa5be2da13fabc/IgBZqhG4nIV2Q7m_t2ntppWZAWGlF1ZSyFUTmQeUunLU6Bw?e=tsc6S3



- `main.cpp` : fenêtre, boucle de rendu, clavier/souris, points de passage de la visite
- `Scene.cpp/.h` : chargement des modèles, placement des objets, animations, lumières
- `Effects.cpp/.h` : shadow map, post-process (bloom), ciel, feu d'artifice
- `CameraPath.cpp/.h` : spline de Catmull-Rom pour la visite
- `Camera`, `Mesh`, `ShaderProgram`, `Texture2D` : classes du cours (un peu modifiées)
- `shaders/` : shaders GLSL
- `models/`, `textures/` : modèles OBJ et textures

## Compilation

Dépendances (Ubuntu / WSL) :

```
sudo apt install build-essential libglfw3-dev libglew-dev libglm-dev
```

```
make
make run
```

Sous Windows avec WSL : `run.bat` (ou Ctrl+Shift+B dans VSCode).

`make run` lance le programme avec `GALLIUM_DRIVER=d3d12` : sous WSL, sans ça, Mesa utilise le rendu
logiciel (llvmpipe) et c'est très lent.

## Commandes

| Touche | Action |
|---|---|
| C | visite automatique / caméra libre |
| W A S D, Z / X | se déplacer, monter / descendre (caméra libre) |
| Shift | aller plus vite |
| Souris / molette | regarder / zoom |
| F | lampe torche |
| N | passer au jour / à la nuit |
| T | cycle jour-nuit automatique |
| B / G / O | bloom / brouillard / ombres |
| P | pause des animations |
| F1 | fil de fer |
| Échap | quitter |

## Contenu de la scène

**Modèles** : OBJ du cours (tonneaux, caisses, quilles, lapins, lampadaires, ampoules, sol). Le loader accepte
maintenant les faces à plus de 3 sommets, les index négatifs et les fichiers sans normales. Le manège (socle,
colonne, toit, barres) est fait avec des cylindres et des cônes générés dans `Mesh.cpp`.

**Lumières** (Blinn-Phong, `shaders/scene.frag`) :
- une lumière directionnelle (soleil le jour, lune la nuit) avec ombres (shadow map + PCF)
- jusqu'à 8 point lights : lampadaires (un qui grésille), manège, stand, flash du feu d'artifice
- 2 spots : projecteur sur le manège et lampe torche

**Textures et matériaux** : texture diffuse + specular map (tonneaux), teinte et brillance par objet,
textures rayées générées dans le code (toit du manège, auvent).

**Caméra** : visite automatique sur une spline de Catmull-Rom, caméra FPS du cours en mode libre.

**Animations** : manège qui tourne, lapins qui montent et descendent, ampoules en chenillard, guirlandes qui
scintillent, boule de bowling qui renverse les quilles puis revient, cycle jour/nuit.

**Effets** : rendu HDR + bloom + tone mapping ACES, ciel procédural (soleil, lune, étoiles), brouillard,
feu d'artifice (particules), vignette, fondu au début et bandes noires pendant la visite.
