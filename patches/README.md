# Série de patches do RDoom NX sobre o UZDoom 5.0.3

Base: UZDoom tag 5.0.3, commit `7910df7102033fd7754a317f3254f24101135926`.
Gerada com `git -C engine format-patch 7910df71..HEAD` (branch `rdoom-nx`).

Aplicar num clone limpo do UZDoom:

    git clone https://github.com/UZDoom/UZDoom.git && cd UZDoom
    git checkout 7910df7102033fd7754a317f3254f24101135926
    git am ../patches/*.patch

Os patches 0001–0012 são o port de Switch anterior do autor, 0013 é a
importação do trabalho de Vulkan em andamento e 0014 em diante são do RDoom
(ver docs/PORTING_AUDIT.md). O 0056 tira o OpenGL/GLES do build: o RDoom usa só Vulkan.
