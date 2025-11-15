# Proyecto Final - Computación Gráfica Avanzada

```
Semestre 2026-1
Facultad de Ingeniería UNAM
Moreno Chalico Edgar Ulises
```

## Ajustes de compilación

Estos ajustes se pueden configurar de la siguiente manera:

```bash
mkdir build
cd build
cmake .. -DENABLE_LOG=1 -DENABLE_GL_DEBUG=1 -DUSE_DEBUG_ASSETS=1
```

* `ENABLE_LOG`: Habilita algunos logs adicionales para mostrar información relevante en tiempo de ejecución.
* `ENABLE_GL_DEBUG`: Habilita el modo de depuración de OpenGL.
* `USE_DEBUG_ASSETS`: Utiliza los assets directamente sin tener que reconfigurar el proyecto.
* `FETCH_EXTERNAL_ASSIMP`: Descarga, compila y utiliza assimp v5.4.3. Útil para hacer debug de el importado de modelos o cualquier cosa que requiera los símbolos de assimp.
* `FIX_ASSIMP_MEMCALL`: Corrige un error de compilación de assimp, habilitar solo si sucede el error.

## Notas adicionales

Para mejor rendimiento en la versión final, es necesario compilar la versión de Release.