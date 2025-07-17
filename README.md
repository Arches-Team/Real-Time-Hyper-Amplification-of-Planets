# Real-Time Hyper-Amplification of Planets

This repository contains the implementation of the paper:

**Real-Time Hyper-Amplification of Planets**  
Yann Cortial, Eric Guérin, Adrien Peytavie, Eric Galin  
Published in *The Visual Computer*  
[Read the paper](https://hal.science/hal-02967067v1/document)

---

## 🛠 Installation

1. **Unzip GLM Library**  
   Unzip `lib/glm-0.9.9.2.zip`.  
   This should create the directory `lib/glm-0.9.9.2/` within the solution directory.

2. **Install Qt 5.15 LTS**  
   Download and install **Qt 5.15 LTS** with `msvc2019_64` compiler support.

3. **Set up Environment Variable**  
   Define a Windows environment variable `QTDIR` pointing to the Qt binaries:  

   QTDIR = C:\DEV\lib\Qt\Qt5.15\5.15.0\msvc2019_64


4. **Configure Visual Studio 2019**  
- Run Visual Studio 2019.  
- Install the **Qt Visual Studio Tools** extension from within Visual Studio.  
- Restart Visual Studio.  
- In the `Extensions` menu, go to `Qt VS Tools` → `Qt Options`.  
- Add a new Qt version entry (e.g., `msvc2019_64_qt5.15`) and set the path to your Qt installation (e.g., `C:\DEV\lib\Qt\Qt5.15\5.15.0\msvc2019_64`).

5. **Load the Solution**  
- Restart Visual Studio and load the `AppPlanetSubdiv` solution.  
- In Solution Explorer, right-click on the solution and select `Change Solution Qt's Version`.  
- Choose the Qt version entry you created in step 4.

The project should now be ready to build and run.

---

## 📂 Data

The repository includes application data located in the `data/` folder.  
This directory contains sample datasets required for testing and rendering planetary terrains.  

---

## 🎮 Usage

### Keyboard Commands

#### 🌍 Terrain Generation
| Key | Action                                                       |
|-----|--------------------------------------------------------------|
| T   | Freeze/unfreeze tessellation update                          |
| W   | Toggle generation of drainage patterns (ravines)             |
| V   | Toggle generation of river and valley profiles               |
| B   | Toggle blending of river profiles with raw terrain           |
| E   | Toggle generation of lakes (non-endorheic)                   |
| R   | [Deprecated] Toggle subdivision-based relief generation      |

#### 🖼 Rendering
| Key   | Action                                |
|-------|---------------------------------------|
| F5    | Toggle HQ shading / debug shading     |
| +     | Toggle FullHD mode                    |

#### 🎨 False Colors (Debug Shading)
| Key   | Action                                             |
|-------|----------------------------------------------------|
| F6    | Toggle wireframe/filled triangles                  |
| D     | Show drainage patterns in false colors             |
| P     | Show plateaux in false colors                      |
| A     | Show crust age in false colors (younger = colored) |
| Q     | Show valley profiles blend distance                |
| F     | Show normalized river flow direction               |
| G     | [Deprecated] Show ghost vertices                   |

#### 📸 Export
| Key   | Action                                                      |
|-------|-------------------------------------------------------------|
| C     | Take a screenshot                                           |
| F9    | Start/stop saving camera frames to disk (live mode)         |
| F1    | Start a new camera path segment and add first control point |
| F2    | Record a new control point for current camera path segment  |
| F4    | Export video frames for current camera path                 |
| K     | Save camera path to disk (default location)                 |
| L     | Load camera path from disk (default location)               |

#### 🔄 Miscellaneous
| Key   | Action            |
|-------|-------------------|
| F12   | Reload shaders    |

---

## 📄 Citation

If you use this code, please cite:


@article{10.1007/s00371-020-01923-4,
	author = {Cortial, Yann and Peytavie, Adrien and Galin, \'{E}ric and Gu\'{e}rin, \'{E}ric},
	title = {Real-time hyper-amplification of planets},
	year = {2020},
	issue_date = {Oct 2020},
	publisher = {Springer-Verlag},
	address = {Berlin, Heidelberg},
	volume = {36},
	number = {10–12},
	issn = {0178-2789},
	url = {https://doi.org/10.1007/s00371-020-01923-4},
	doi = {10.1007/s00371-020-01923-4},
	abstract = {We propose an original method for generating planets with a high level of detail in real time. Our approach relies on a procedural hyper-amplification algorithm: a controlled subdivision process that faithfully reproduces landforms and hydrosphere features at different scales. Starting from low-resolution user-defined control maps providing information about the elevation, the presence of large-scale water bodies and landforms types, we apply subdivision rules to obtain a high-resolution hydrologically consistent planet model. We first generate large-scale river networks connected to inner seas and oceans and then synthesize the detailed hydrographic landscapes, including river tributaries and lakes, mountain ranges, valleys, plateaus, deserts and hills systems. Our GPU implementation allows to interactively explore planets that are produced by tectonic simulations, generated procedurally or authored by artists.},
	journal = {Vis. Comput.},
	month = oct,
	pages = {2273–2284},
	numpages = {12},
	keywords = {Procedural generation, Adaptive subdivision, Amplification, Planets, Terrain modeling}
}


---

## 🔗 Resources

- 📖 [Read the paper](https://hal.science/hal-02967067v1/document)  
- 🖥 Qt Visual Studio Tools: [https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools2019](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools2019)

