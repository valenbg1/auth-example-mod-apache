# Módulo Apache *auth_example*

Módulo de ejemplo de login básico para Apache httpd 2.4

Autentica un usuario contra un fichero de logins posibles.

Las pruebas se han realizado usando un SO [Debian](http://www.debian.org/distrib/) 8.4.0 amd64 y [Apache httpd](http://httpd.apache.org/) 2.4.10.

### Referencias
* [Developing modules for the Apache HTTP Server 2.4](http://httpd.apache.org/docs/2.4/developer/modguide.html).
* [Apache2 Documentation](http://ci.apache.org/projects/httpd/trunk/doxygen/index.html).
* [Apache Portable Runtime Documentation](http://apr.apache.org/docs/apr/2.0/index.html).
* [Apache HTTP Server Version 2.4 Documentation](http://httpd.apache.org/docs/2.4/en/).

Valen Blanco ([GitHub](http://github.com/valenbg1)/[LinkedIn](http://www.linkedin.com/in/valenbg1)).

## Introducción

### Apache httpd
Para instalar Apache en nuestra máquina, así como los paquetes *developer*, podemos hacerlo desde la terminal:

`apt-get install apache2 apache2-bin apache2-data apache2-dbg apache2-dev apache2-doc apache2-utils libapr1 libapr1-dev libaprutil1 libaprutil1-dev`

#### apxs

##### Crear plantilla de ejemplo para desarrollar un módulo

Mediante la herramienta [*apxs*](http://httpd.apache.org/docs/current/programs/apxs.html) que nos proporciona Apache, podemos crear una plantilla de ejemplo que contiene un *Makefile* y un fichero fuente para comenzar a desarrollar un módulo, mediante la terminal:

`apxs -g -n auth_example`

##### Compilar, instalar y activar nuestro módulo en el servidor

En la terminal, en el directorio donde estemos trabajando:

`apxs -cia mod_auth_example.c`
* `-c`: compila el módulo.
* `-i`: instala el módulo en la carpeta de Apache.
* `-a`: activa el módulo en el archivo *.conf* de Apache (realmente sólo es necesario la primera vez).

Después habrá que reiniciar el servidor: `apachectl restart`.

### Eclipse IDE

#### Instalar Eclipse

Para editar el código he usado Eclipse C/C++, que puede conseguirse en este [link](http://www.eclipse.org/downloads/) o instalando a través de la terminal:

`apt-get install eclipse eclipse-cdt`

#### Crear un nuevo proyecto para nuestro módulo

Habrá que crear un nuevo proyecto a partir del *Makefile* plantilla que ya tenemos. En Eclipse:
> File -> New -> Project... -> C/C++ -> Makefile Project with Existing Code

* **Project Name**: p.e. Apache auth_example module.
* **Existing Code Location**: la carpeta donde esté el código fuente.
* **Languages**: seleccionar sólo C.
* **Toolchain for Indexer Settings**: yo estoy usando *Linux GCC*.

Una vez creado el proyecto, hay que incluir dos *paths* con las cabeceras de Apache y [APR](http://apr.apache.org/) en el proyecto. En Eclipse:
> Botón derecho sobre el proyecto -> Properties -> C/C++ General -> Paths and Symbols -> Includes -> GNU C -> Add...

Y añadimos las carpetas con las cabeceras, en mi caso:
* `/usr/include/apache2`
* `/usr/include/apr-1.0`

Añadidas las cabeceras, sólo queda refrescar el *C index* del proyecto. En Eclipse:
> Botón derecho sobre el proyecto -> Index -> Rebuild y Freshen All Files

## Crear un módulo para Apache

Voy a explicar paso a paso el archivo [mod_auth_example.c](http://github.com/valenbg1/auth-example-mod-apache/blob/master/mod_auth_example.c).

### Declarar el módulo

Comenzamos con la declaración del módulo, que definirá como engancha nuestro módulo con Apache:
<a name=auth_example_module></a>
```c
module AP_MODULE_DECLARE_DATA auth_example_module =
{
    STANDARD20_MODULE_STUFF,
    create_dir_conf,
    merge_dir_conf,
    NULL, // Función que crea una nueva configuración para servidores distintos.
    NULL, // Función para mezclar dos configuraciones para servidores distintos.
    directives,
    register_hooks
};
```

Declaramos un nuevo módulo que se llamará [*auth_example_module*](#auth_example_module):
* [**create_dir_conf**](#create_dir_conf): función de nuestro módulo que creará una nueva configuración por directorio (las configuraciones se explican luego).
* [**merge_dir_conf**](#merge_dir_conf): función de nuestro módulo que mezcla dos configuraciones distintas.
* [**directives**](#directivas): tabla de directivas, que son los argumentos que luego podemos utilizar para configurarlo en el archivo *.conf* de nuestro módulo (explicadas luego).
* **register_hooks**: función para registrar nuestras funciones de enganche con el servidor.

### Configuración de nuestro módulo

La configuración de nuestro módulo se va a guardar en una estructura que definiremos nosotros:
<a name=auth_ex_cfg></a>
```c
#define BUF_SIZE 256

// Definición del tipo 'bool' para mayor legibilidad del código.
typedef char bool;

typedef struct
{
	char context[BUF_SIZE]; // Argumento que pasa Apache al crear una configuración.
	char logins_path[BUF_SIZE]; // Path hacia un fichero de logins.
	char logs_path[BUF_SIZE]; // Path hacia un fichero de logs.
	bool flush; // Permite o no visualizar el fichero de logins completo (p.e. al acceder a localhost/auth/flush).
} auth_ex_cfg;
```

Para manejar esta estructura, se definen dos funciones que controlan la creación y la mezcla de configuraciones respectivamente:
* <a name=create_dir_conf></a>**create_dir_conf**:
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
* <a name=merge_dir_conf></a>**merge_dir_conf**: función para mezclar dos estructuras [*auth_ex_cfg*](#auth_ex_cfg). Sirve para que directorios o localizaciones puedan heredar la configuración de su padre. Por ejemplo, si tenemos una localización *localhost/auth* y otra *localhost/auth/flush*, al crear la configuración de *localhost/auth/flush* Apache creará primero la configuración de *localhost/auth* (leyendo del fichero *.conf*), después la de *localhost/auth/flush*, y nos pasará punteros a ambas estructuras de configuración en los argumentos *BASE* y *ADD* respectivamente. Es nuestra responsabilidad definir cómo se integran estas dos configuraciones.
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

### Directivas

Son los argumentos que luego podemos indicar en el archivo *.conf* de nuestro módulo para configurarlo. Es un array de estructuras [*command_rec*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__APACHE__CORE__CONFIG.html#ga79f84e70f072880482a3fd004ae48710):
<a name=directives></a>
```c
static const command_rec directives[] =
{
	AP_INIT_TAKE1("AuthExampleLoginsPath", set_logins_path, NULL, ACCESS_CONF,
			"Sets the logins file path"),
	AP_INIT_TAKE1("AuthExampleLogsPath", set_logs_path, NULL, ACCESS_CONF,
			"Sets the logs file path"),
	AP_INIT_TAKE1("AuthExampleFlush", set_flush, NULL, ACCESS_CONF,
			"Allows the 'flush' mode [allow|deny]"),
	{ NULL }
};
```

[AP_INIT_TAKE1](http://ci.apache.org/projects/httpd/trunk/doxygen/group__APACHE__CORE__CONFIG.html#ga07c7d22ae17805e61204463326cf9c34)( | "AuthExampleLoginsPath", | [set_logins_path](#set_logins_path), | NULL, | [ACCESS_CONF](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga09a3c6983aa6dc02f1d1f49aea0ff4ee), | "Sets the logins file path")
:---: | :---: | :---: | :---: | :---: | :---:
Macro que declara una directiva que acepta 1 parámetro | Nombre de la directiva | Función que maneja la directiva | Función (opcional) que guarda la configuración | Contexto en el que se acepta la directiva | Breve descripción de la directiva

Los contextos posibles en los que se acepta la directiva son:
* [**RSRC_CONF**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga2c51f4c7392fa5af1afe797470dc16e3): Archivos *.conf* fuera de tags *\<Directory\>* o *\<Location\>*.
* [**ACCESS_CONF**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga09a3c6983aa6dc02f1d1f49aea0ff4ee): Archivos *.conf* dentro de tags *\<Directory\>* o *\<Location\>*.
* [**OR_OPTIONS**](https://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga498a222873d29356a1ab9bd3f936b270): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride Options*.
* [**OR_FILEINFO**](https://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga3f1f59e707b2f247220fe64d06cb557d): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride FileInfo*.
* [**OR_AUTHCFG**](https://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#gad707d4eac4b22c5bc225b784ecb6c90e): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride AuthConfig*.
* [**OR_INDEXES**](https://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga1470585899bbca9fd583e49f17336e1b): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride Indexes*.
* [**OR_ALL**](https://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga1cf7da2bf7d8b3caaea4cb6a9abf02b5): En cualquier sitio en archivos *.conf* y *.htaccess*.

#### Funciones para manejar las directivas

En el array de directivas anterior hemos definido que las funciones de manejo serían [*set_logins_path*](#set_logins_path), *set_logs_path* y *set_flush*. Estas funciones controlan lo que pasa cuando en el fichero de configuración *.conf* del módulo tenemos alguna de las directivas que hemos indicado ([*AuthExampleLoginsPath*](#directives), [*AuthExampleLogsPath*](#directives) y [*AuthExampleFlush*](#directives)). Como ejemplo vamos a ver [*set_logins_path*](#set_logins_path) (las dos restantes pueden verse en el archivo [mod_auth_example.c](http://github.com/valenbg1/auth-example-mod-apache/blob/master/mod_auth_example.c)):
<a name=set_logins_path></a>
```c
const char *set_logins_path(cmd_parms *cmd, void *cfg, const char *arg)
{
	auth_ex_cfg *config = (auth_ex_cfg*) cfg;

	if (config)
	{
		/*
		 * '*arg' sería el argumento de la directiva 'AuthExampleLoginsPath' que
		 * se encuentra en el fichero '.conf'
		 * (p.e. 'AuthExampleLoginsPath "/etc/apache2/mod_auth_example-logins"').
		 */
		strcpy(config->logins_path, arg);
	}

	return NULL;
}
```

*set_logs_path* y *set_flush* son similares.
