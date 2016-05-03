# Apache auth_example module

Módulo de ejemplo de login básico para Apache httpd 2.4

Autentica un usuario contra un fichero de logins posibles.
Basado en *http://httpd.apache.org/docs/2.4/developer/modguide.html*.

Este tutorial se ha realizado usando un SO [Debian](http://www.debian.org/distrib/) 8.4.0 amd64 y [Apache httpd](http://httpd.apache.org/) 2.4.10.

Valen Blanco (*http://github.com/valenbg1*).

## Introducción

### Instalar Apache httpd
Para instalar Apache en nuestra máquina, así como los paquetes *developer*, podemos hacerlo en la terminal:

`apt-get install apache2 apache2-bin apache2-data apache2-dbg apache2-dev apache2-doc apache2-utils libapr1 libapr1-dev libaprutil1 libaprutil1-dev`

### apxs

#### Crear *template* de ejemplo para desarrollar un módulo de Apache

Mediante la herramienta *apxs* que nos proporciona Apache, podemos crear un *template* de ejemplo que contiene un *Makefile* y un fichero fuente de ejemplo para comenzar a desarrollar un módulo, mediante la terminal:

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

Para editar el código he usado Eclipse C/C++, que puede conseguirse en este [link](http://www.eclipse.org/downloads/) o instalando a través de la terminal:

`apt-get install eclipse eclipse-cdt`

#### Crear un nuevo proyecto para nuestro módulo

Habrá que crear un nuevo proyecto a partir del *Makefile* que ya tenemos (que fue creado con `apxs -g`). En Eclipse: File -> New -> Project... -> C/C++ -> Makefile Project with Existing Code:
* **Project Name**: p.e. Apache auth_example module.
* **Existing Code Location**: la carpeta donde esté el código fuente.
* **Languages**: seleccionar sólo C.
* **Toolchain for Indexer Settings**: yo estoy usando *Linux GCC*.

Una vez creado el proyecto, hay que incluir dos *paths* con las cabeceras de Apache y APR en el proyecto. En Eclipse: botón derecho sobre el proyecto -> Properties -> C/C++ General -> Paths and Symbols -> Includes -> GNU C -> Add... y añadimos las carpetas con las cabeceras, en mi caso:
* `/usr/include/apache2`
* `/usr/include/apr-1.0`

Añadidas las cabeceras, sólo queda refrescar el *C index* del proyecto. En Eclipse: botón derecho sobre el proyecto -> Index -> Rebuild y Freshen All Files.

## Crear un módulo para Apache

Voy a explicar paso a paso el archivo [mod_auth_example.c](http://github.com/valenbg1/auth-example-mod-apache/blob/master/mod_auth_example.c).

### Declarar el módulo

Comenzamos con la declaración del módulo, que definirá como engancha nuestro módulo con Apache:
```c
module AP_MODULE_DECLARE_DATA auth_example_module =
{
    STANDARD20_MODULE_STUFF,
    create_dir_conf,
    merge_dir_conf,
    NULL, // Función que creará una nueva configuración para servidores distintos.
    NULL, // Función para mezclar dos configuraciones para servidores distintos.
    directives,
    register_hooks
};
```

Declaramos un nuevo módulo que se llamará *auth_example_module*:
* **create_dir_conf**: función de nuestro módulo que creará una nueva configuración por directorio.
* **merge_dir_conf**: función de nuestro módulo que mezcla dos configuraciones distintas.
* **directives**: tabla de directivas para nuestro módulo, que son los argumentos que luego podemos utilizar para configurarlo en el archivo *.conf* de nuestro módulo.
* **register_hooks**: función para registrar nuestras funciones de enganche con Apache.

### Configuración de nuestro módulo

La configuración de nuestro módulo se va a guardar en una estructura que definiremos nosotros:
```c
#define BUF_SIZE 256

typedef struct
{
	char context[BUF_SIZE]; // Argumento que pasa Apache al crear una configuración.
	char logins_path[BUF_SIZE]; // Path hacia un fichero de logins.
	char logs_path[BUF_SIZE]; // Path hacia un fichero de logs.
	bool flush; // Permite o no visualizar el fichero de logins completo (p.e. en localhost/auth/flush).
} auth_ex_cfg;
```

Para manejar esta estructura, hemos definido dos funciones que controlan la creación y la mezcla de configuraciones respectivamente:
* **create_dir_conf**:
```c
void *create_dir_conf(apr_pool_t *pool, char *context)
{
	if (!context)
		context = "Undefined context";

	/*
	 * Pedimos memoria para alojar una nueva estructura 'auth_ex_cfg'
	 * en el pool de memoria que nos deja Apache para nuestro módulo
	 * (referenciado con 'pool'). En este caso además se inicializa
	 * el espacio reservado con 0's.
	 */
	auth_ex_cfg *cfg = apr_pcalloc(pool, sizeof(auth_ex_cfg));

	if (cfg)
	{
		// Ponemos unos valores por defecto para la configuración.

		strcpy(cfg->context, context);
		strcpy(cfg->logins_path, "/etc/apache2/mod_auth_example-logins");
		strcpy(cfg->logs_path, "/etc/apache2/mod_auth_example-logs");
		cfg->flush = FALSE;
	}

	return cfg;
}
```
* **merge_dir_conf**: función para mezclar dos estructuras *auth_ex_cfg*. Sirve para que directorios o localizaciones puedan heredar la configuración de su padre. Por ejemplo, si tenemos una localización *localhost/auth* y otra *localhost/auth/flush*, al crear la configuración de *localhost/auth/flush* Apache creará primero la configuración de *localhost/auth* (leyendo del fichero *.conf*), después la de *localhost/auth/flush*, y nos pasará punteros a ambas estructuras de configuración en los argumentos *BASE* y *ADD* respectivamente. Es nuestra responsabilidad definir cómo se integran estas dos configuraciones.
```c
// Comprueba si una c string está vacía.
#define STREMPTY(string) (string[0] == '\0')

void *merge_dir_conf(apr_pool_t *pool, void *BASE, void *ADD)
{
	auth_ex_cfg *base = (auth_ex_cfg*) BASE;
	auth_ex_cfg *add = (auth_ex_cfg*) ADD;
	auth_ex_cfg *conf = (auth_ex_cfg*) create_dir_conf(pool, "Merged configuration");

	/*
	 * En nuestro caso, si no está definido algún path para nuestra localización,
	 * heredamos la configuración de la localización padre.
	 */
    strcpy(conf->logins_path, STREMPTY(add->logins_path) ? base->logins_path : add->logins_path);
    strcpy(conf->logs_path, STREMPTY(add->logs_path) ? base->logs_path : add->logs_path);

    /*
     * El valor del campo 'flush' no se hereda, tendrá que ser definido para cada
     * localización explícitamente o se usará el valor por defecto (FALSE).
     */
    conf->flush = add->flush;

    return conf;
}
```
