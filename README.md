# Apache auth_example module
Módulo de ejemplo de login básico para Apache httpd 2.4

Autentica un usuario contra un fichero de logins posibles.
Basado en *http://httpd.apache.org/docs/2.4/developer/modguide.html*.

Este tutorial se ha realizado usando un SO [Debian](http://www.debian.org/distrib/) 8.4.0 amd64 y [Apache httpd](http://httpd.apache.org/) 2.4.10.

Valen Blanco (*http://github.com/valenbg1*).

## Intro

### Instalar Apache httpd
Para instalar Apache en nuestra máquina, así como los paquetes *developer*, podemos hacerlo en la terminal:

`apt-get install apache2 apache2-bin apache2-data apache2-dbg apache2-dev apache2-doc apache2-utils libapr1 libapr1-dev libaprutil1 libaprutil1-dev`

### apxs

#### Crear *template* de ejemplo para desarrollar un módulo de Apache

Mediante la herramienta *apxs* que nos proporciona Apache, podemos crear un *template* de ejemplo que contiene un *Makefile* y un fichero fuente de ejemplo para comenzar a desarrollar un módulo:

`apxs -g -n auth_example`

#### Compilar, instalar y activar nuestro módulo en Apache

En la terminal, en el directorio donde estemos trabajando:

`apxs -cia mod_auth_example.c`
* `-c`: compila el módulo.
* `-i`: instala el módulo en la carpeta de Apache.
* `-a`: activa el módulo en el archivo *.conf* de Apache (realmente sólo es necesario la primera vez).

Después habrá que reiniciar el servidor Apache: `apachectl restart`.

### Eclipse IDE

#### Instalar Eclipse

Para editar el código yo he usado Eclipse C/C++, que puede conseguirse en este [link](http://www.eclipse.org/downloads/) o instalando a través del terminal:

`apt-get install eclipse eclipse-cdt`

#### Crear un nuevo proyecto para nuestro módulo

Habrá que crear un nuevo proyecto a partir del *Makefile* que ya tenemos (que fue creado con `apxs -g`). En Eclipse:
File -> New -> Project... -> C/C++ -> Makefile Project with Existing Code:
* Project Name: el queráis, p.e. Apache auth_example module.
* Existing Code Location: la carpeta donde tengáis el código.
* Languages: seleccionar sólo C.
* Toolchain for Indexer Settings: yo estoy usando *Linux GCC*.

Una vez creado el proyecto, hay que incluir dos *paths* extra con las cabeceras de Apache y APR. En Eclipse: botón derecho sobre el proyecto -> Properties -> C/C++ General -> Paths and Symbols -> Includes -> GNU C -> Add... y añadimos las carpetas con las cabeceras, en mi caso:
* `/usr/include/apache2`
* `/usr/include/apr-1.0`

Añadidas las cabeceras, sólo queda refrescar el *C index* del proyecto. En Eclipse: botón derecho sobre el proyecto -> Index -> Rebuild y Freshen All Files.
