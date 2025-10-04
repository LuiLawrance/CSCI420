CSCI-420 Assignment #1: Height Fields Using Shaders

Operating System: Windows 11 Pro
IDE: Visual Studio Community 2022

-------------------------
Assignment Core Functionality:

* Pressing "1" -> renders using POINTS
* Pressing "2" -> renders using LINES/WIREFRAME
* Pressing "3" -> renders using TRIANGLES
* Pressing "4" -> renders using SMOOTHENING on vertices of the TRIANGLES

* Pressing "x" -> takes a screenshot and saves it in the "Images" folder
* Pressing "s" -> initiates a 300 frame animation and saves each frame in the "Images/Animations" folder
* Pressing "ESC" -> Exits the application

* Dragging mouse while holding "Left Mouse"           -> rotates the heightmap about the x and y axes
* Dragging mouse while holding "Left Mouse" + SHIFT   -> scales the heightmap on the x and y axes
* Dragging mouse while holding "Left Mouse" + "t"     -> translates the heightmap on the x and y axes
* Dragging mouse while holding "Middle Mouse"         -> rotates the heightmap about the z-axis
* Dragging mouse while holding "Middle Mouse" + SHIFT -> scales the heightmap on the z-axis
* Dragging mouse while holding "Middle Mouse" + "t"   -> translates the heightmap on the z-axis

-------------------------
Assignment Extra Considerations:

* Pressing "5" -> renders WIREFRAME on top of TRIANGLES
* Pressing "a" -> initiates an animation (consisting of rotations, scaling, translations, and mode transitions)
* Pressing "r" -> toggles a continuous rotation of the heightmap about the y-axis

* You can additionally generate a heightmap with color, taken from a secondary image of the same size as the first argument
	- Ex: ./hw1 heightmap/Heightmap.jpg heightmap/HeightmapColor.jpg

* The code can automatically detect whether the first argument is a colored image as and generate the heightmap accordingly
	- Ex. ./hw1 heightmap/HeightmapColor.jpg

