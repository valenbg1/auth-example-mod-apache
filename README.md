# Módulo Apache httpd auth_example

Módulo de ejemplo de login básico para Apache httpd 2.4

Autentica un usuario contra un fichero de logins posibles.

Las pruebas se han realizado usando un SO [Debian](http://www.debian.org/distrib/) 8.4.0 amd64 y [Apache httpd](http://httpd.apache.org/) 2.4.10

### Referencias

* [Developing modules for the Apache HTTP Server 2.4](http://httpd.apache.org/docs/2.4/developer/modguide.html)
* [Apache2 Documentation](http://ci.apache.org/projects/httpd/trunk/doxygen/index.html)
* [Apache Portable Runtime Documentation](http://apr.apache.org/docs/apr/2.0/index.html)
* [Apache HTTP Server Version 2.4 Documentation](http://httpd.apache.org/docs/2.4/en/)

Valen Blanco ([GitHub](http://github.com/valenbg1)/[LinkedIn](http://www.linkedin.com/in/valenbg1)).

## Índice

* [Introducción](#introducción)
  * [Apache httpd](#apache-httpd)
    * [apxs](#apxs)
      * [Crear plantilla de ejemplo para desarrollar un módulo](#crear-plantilla-de-ejemplo-para-desarrollar-un-módulo)
      * [Compilar, instalar y activar nuestro módulo en el servidor](#compilar-instalar-y-activar-nuestro-módulo-en-el-servidor)
  * [Eclipse IDE](#eclipse-ide)
    * [Instalar Eclipse](#instalar-eclipse)
    * [Crear un nuevo proyecto para nuestro módulo](#crear-un-nuevo-proyecto-para-nuestro-módulo)
* [Crear un módulo para Apache](#crear-un-módulo-para-apache)
  * [Declarar el módulo](#declarar-el-módulo)
  * [Configuración de nuestro módulo](#configuración-de-nuestro-módulo)
  * [Directivas](#directivas)
    * [Funciones para manejar las directivas](#funciones-para-manejar-las-directivas)
  * [Registrar nuestras funciones con el servidor](#registrar-nuestras-funciones-con-el-servidor)
  * [Crear nuestro propio handler](#crear-nuestro-propio-handler)
* [Archivo *.conf* de configuración del módulo](#archivo-conf-de-configuración-del-módulo)
* [Referencias](#referencias)

## Introducción

### Apache httpd
Para instalar Apache en nuestra máquina, así como los paquetes *developer*, podemos hacerlo desde la terminal:

`apt-get install build-essential apache2 apache2-bin apache2-data apache2-dbg apache2-dev apache2-doc apache2-utils libapr1 libapr1-dev libaprutil1 libaprutil1-dev`

#### apxs

##### Crear plantilla de ejemplo para desarrollar un módulo

Mediante la herramienta [*apxs*](http://httpd.apache.org/docs/current/programs/apxs.html) que nos proporciona Apache, podemos crear una plantilla de ejemplo que contiene un *Makefile* y un fichero fuente para comenzar a desarrollar un módulo, mediante la terminal:

`apxs -g -n auth_example`

##### Compilar, instalar y activar nuestro módulo en el servidor

En la terminal, en el directorio donde estemos trabajando:

`apxs -cia mod_auth_example.c`
* `-c`: compila el módulo.
* `-i`: instala el módulo en la carpeta de Apache.
* `-a`: activa el módulo en la configuración de Apache (realmente sólo es necesario la primera vez).

Después habrá que reiniciar el servidor: `apachectl restart`.

### Eclipse IDE

#### Instalar Eclipse

Para editar el código he usado Eclipse C/C++, que puede conseguirse en este [link](http://www.eclipse.org/downloads/) o instalando a través de la terminal:

`apt-get install eclipse eclipse-cdt`

#### Crear un nuevo proyecto para nuestro módulo

Habrá que crear un nuevo proyecto a partir del *Makefile* plantilla que ya tenemos. En Eclipse:
> File -> New -> Project... -> C/C++ -> Makefile Project with Existing Code

* **Project Name**: p.e. *Apache auth_example module*.
* **Existing Code Location**: la carpeta donde esté el código fuente.
* **Languages**: seleccionar sólo C.
* **Toolchain for Indexer Settings**: yo estoy usando *Linux GCC*.

Una vez creado el proyecto, hay que incluir dos paths con las cabeceras de Apache y [APR](http://apr.apache.org/) en el proyecto. En Eclipse:
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

Declaramos un nuevo módulo que se llamará *auth_example_module*:
* [**create_dir_conf**](#create_dir_conf): función de nuestro módulo que creará una nueva configuración por directorio (las configuraciones se explican luego).
* [**merge_dir_conf**](#merge_dir_conf): función de nuestro módulo que mezcla dos configuraciones distintas.
* [**directives**](#directives): tabla de directivas, que son los argumentos que luego podemos utilizar para configurarlo en el [archivo *.conf* de nuestro módulo](#archivo-conf-de-configuración-del-módulo) (explicadas luego).
* [**register_hooks**](#register_hooks): función para registrar nuestras funciones de enganche con el servidor.

### Configuración de nuestro módulo

La configuración de nuestro módulo se va a guardar en una estructura que definiremos nosotros:
<a name=auth_ex_cfg></a>
```c
#define BUF_SIZE 256

// Definición del tipo bool para mayor legibilidad del código.
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

Son los argumentos que luego podemos indicar en el [archivo *.conf* de nuestro módulo](#archivo-conf-de-configuración-del-módulo) para configurarlo. Es un array de estructuras [*command_rec*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__APACHE__CORE__CONFIG.html#ga79f84e70f072880482a3fd004ae48710):
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
* [**RSRC_CONF**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga2c51f4c7392fa5af1afe797470dc16e3): Archivos *.conf* fuera de tags \<Directory\> o \<Location\>.
* [**ACCESS_CONF**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga09a3c6983aa6dc02f1d1f49aea0ff4ee): Archivos *.conf* dentro de tags \<Directory\> o \<Location\>.
* [**OR_OPTIONS**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga498a222873d29356a1ab9bd3f936b270): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride Options*.
* [**OR_FILEINFO**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga3f1f59e707b2f247220fe64d06cb557d): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride FileInfo*.
* [**OR_AUTHCFG**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#gad707d4eac4b22c5bc225b784ecb6c90e): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride AuthConfig*.
* [**OR_INDEXES**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga1470585899bbca9fd583e49f17336e1b): Archivos *.conf* y *.htaccess* cuando se indica *AllowOverride Indexes*.
* [**OR_ALL**](http://ci.apache.org/projects/httpd/trunk/doxygen/group__ConfigDirectives.html#ga1cf7da2bf7d8b3caaea4cb6a9abf02b5): En cualquier sitio en archivos *.conf* y *.htaccess*.

#### Funciones para manejar las directivas

En el array de directivas anterior hemos definido que las funciones de manejo serían *set_logins_path*, *set_logs_path* y *set_flush*. Estas funciones controlan lo que pasa cuando en el [fichero de configuración *.conf* del módulo](#archivo-conf-de-configuración-del-módulo) tenemos alguna de las directivas que hemos indicado (*AuthExampleLoginsPath*, *AuthExampleLogsPath* y *AuthExampleFlush*). Como ejemplo vamos a ver *set_logins_path* (las dos restantes pueden verse en el archivo [mod_auth_example.c](http://github.com/valenbg1/auth-example-mod-apache/blob/master/mod_auth_example.c)):
<a name=set_logins_path></a>
```c
const char *set_logins_path(cmd_parms *cmd, void *cfg, const char *arg)
{
    // Estructura de configuración creada previamente.
    auth_ex_cfg *config = (auth_ex_cfg*) cfg;

    if (config)
    {
        /*
         * '*arg' sería el argumento de la directiva 'AuthExampleLoginsPath' que
         * se encuentra en el fichero .conf
         * (p.e. AuthExampleLoginsPath "/etc/apache2/mod_auth_example-logins").
         */
        strcpy(config->logins_path, arg);
    }

    return NULL;
}
```

*set_logs_path* y *set_flush* son similares.

### Registrar nuestras funciones con el servidor

En la [declaración del módulo](#auth_example_module), indicamos una función que vamos a utilizar para registrar nuestras funciones con Apache:
<a name=register_hooks></a>
```c
static void register_hooks(apr_pool_t *p)
{
    /*
     * En este caso, registramos un nuevo handler cuya función es
     * 'auth_example_handler'. Con 'APR_HOOK_LAST' le indicamos a
     * Apache que nuestro handler debe ser llamado de los últimos en
     * la cola (p.e. después de mod_rewrite, módulo que reescribe urls
     * al estilo /parent/child a /parent?child=1).
     */
    ap_hook_handler(auth_example_handler, NULL, NULL, APR_HOOK_LAST);
}
```

### Crear nuestro propio handler

El handler es la parte más pesada del módulo, que se encargará de manejar toda la autenticación del usuario y devolver resultados a la consulta HTTP. Vamos a ver la función *auth_example_handler* paso a paso:

```c
static int auth_example_handler(request_rec *r)
{
    /*
     * Como Apache irá preguntando a cada módulo si tiene un handler llamado
     * 'auth_example-handler', rechazamos todas las peticiones en las que el nombre
     * del handler sea distinto.
     */
    if (!r->handler || strcmp(r->handler, "auth_example-handler"))
        return DECLINED;
```
Apache nos pasa un puntero a una estructura de datos [*request_rec*](http://ci.apache.org/projects/httpd/trunk/doxygen/structrequest__rec.html) que contiene la información sobre la consulta que se ha realizado a nuestro servidor. En el campo [*handler*](http://ci.apache.org/projects/httpd/trunk/doxygen/structrequest__rec.html#a4fb5cec8fe63f73648e96a3af0dff91c) de la estructura se guarda el nombre del handler que debería atender la petición. Por lo tanto, si no es el nombre de nuestro handler (*auth_example-handler*), devolvemos [*DECLINED*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__APACHE__CORE__DAEMON.html#ga9eba11ca86461a3ae319311d64682dda).

```c
    auth_ex_cfg *config = (auth_ex_cfg*) ap_get_module_config(r->per_dir_config, &auth_example_module);
```
Pedimos a Apache que nos dé la estructura de configuración para esta petición (que dependerá del contexto, p.e. bloques tipo \<Directory\> o \<Location\> en el [archivo *.conf* del módulo](#archivo-conf-de-configuración-del-módulo)).

```c
    apr_finfo_t f_logins_info, f_logs_info;

    if ((apr_stat(&f_logins_info, config->logins_path, APR_FINFO_MIN, r->pool) == APR_SUCCESS) &&
        (apr_stat(&f_logs_info, config->logs_path, APR_FINFO_MIN, r->pool) == APR_SUCCESS))
    {
        if ((f_logins_info.filetype == APR_NOFILE) || (f_logins_info.filetype == APR_DIR) ||
            (f_logs_info.filetype == APR_NOFILE) || (f_logs_info.filetype == APR_DIR))
            return HTTP_NOT_FOUND;
    }
    else
        return HTTP_FORBIDDEN;
```
Ahora vamos a comprobar si podemos ejecutar un *stat* sobre los ficheros de logins y logs respectivamente, usando la [APR](http://apr.apache.org/), que es una API uniforme e independiente de la plataforma para funciones comunes de los sistemas operativos. Si los paths no apuntan a un fichero o apuntan a un directorio, devolvemos un [*HTTP_NOT_FOUND*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__HTTP__Status.html#gabd505b5244bd18ae61c581484b4bc5a0). Si no se ha podido ejecutar el *stat*, probablemente no tenemo permiso para acceder a los ficheros, devolvemos un [*HTTP_FORBIDDEN*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__HTTP__Status.html#ga92646f876056a1e5013e0050496dc04d). Usamos el pool de memoria que nos proporciona Apache.

```c
    apr_file_t *f_logins, *f_logs;
    apr_table_t *GET;
    const char *user, *passwd;
    bool flush_mode, user_found = FALSE;

    ap_args_to_table(r, &GET);
    user = apr_table_get(GET, "user");
    passwd = apr_table_get(GET, "passwd");

    /*
     * Estamos en modo flush si 'config->flush' ha sido activado y no nos proporcionan
     * usuario o contraseña. En caso contrario, si falta alguno de los dos argumentos
     * devolvemos un 'HTTP_NETWORK_AUTHENTICATION_REQUIRED' (511).
     */
     
    flush_mode = config->flush && (!user || !passwd);

    if (!flush_mode && (!user || !passwd))
        return HTTP_NETWORK_AUTHENTICATION_REQUIRED;
```
Pedimos a Apache que nos dé la tabla de argumentos de la consulta HTTP tipo GET en la estructura correspondiente. Luego usamos las funciones definidas ([*ap_args_to_table*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__APACHE__CORE__SCRIPT.html#gaed25877b529623a4d8f99f819ba1b7bd), [*apr_table_get*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__apr__tables.html#ga4db13e3915c6b9a3142b175d4c15d915)) que nos permiten parsear la tabla para recoger los argumentos *user* y *passwd* respectivamente.

```c
    // Configura el tipo de contenido que se está enviando de vuelta.
    ap_set_content_type(r, "text/html");

    /* 
     * 'ap_rprintf' permite devolver contenido a una request (referenciado por 'r')
     * al estilo printf. Devolvemos la configuración de nuestro módulo.
     */
    ap_rprintf(r, "<h2><u>Bienvenido al sistema superseguro de autenticacion</h2>"
                    "<b>Mi configuracion es:</u>"
                    "<ul><li>Context:</b> '%s'.</li>"
                    "<li><b>Logins file path:</b> '%s'.</li>"
                    "<li><b>Logs file path:</b> '%s'.</li>"
                    "<li><b>Flush:</b> '%d'.</li></ul><br>",
                    config->context, config->logins_path, config->logs_path, config->flush);

    if (flush_mode)
    {
        // 'ap_rputs' funciona igual que 'ap_rprintf' pero sin argumentos.
        ap_rputs("<p>Modo <i>flush</i>!<br>"
                    "Vamos a ver todos los datos secretos abajo >:)</p><p><code>", r);
    }
    else
        ap_rprintf(r, "<p>Vamos a comprobar las credenciales para <b>%s</b>.</p>", user);
```
Vamos a devolver la configuración actual de nuestro módulo a la petición HTTP.

```c
    if ((apr_file_open(&f_logins, config->logins_path, APR_FOPEN_READ | APR_FOPEN_BUFFERED, APR_OS_DEFAULT, r->pool)
        == APR_SUCCESS) &&
        (apr_file_open(&f_logs, config->logs_path, APR_FOPEN_WRITE | APR_FOPEN_APPEND, APR_OS_DEFAULT, r->pool)
            == APR_SUCCESS))
    {
```
Ahora procedemos a abrir los ficheros de logins (en modo lectura buffered) y de logs (en modo escritura append), con los permisos por defecto y usando el pool de memoria que nos proporciona Apache. Una mejor solución sería tener estos ficheros cargados previamente en memoria (p.e. en una configuración inicial del módulo) para poder consultarlos más rápidamente en cada petición.

```c
        char buffer[BUF_SIZE];

        /*
         * Evidentemente recorrer cada línea del fichero en busca de la que corresponda con
         * usuario/contraseña no es la mejor de las opciones.
         */
        while (apr_file_gets(buffer, BUF_SIZE, f_logins) == APR_SUCCESS)
        {
```
Vamos leyendo el fichero de logins línea por línea, [*apr_file_gets*](http://apr.apache.org/docs/apr/2.0/group__apr__file__io.html#gaf9513b769c10b09e5f37d6d0b86bdce9) lee una línea de fichero cada vez, y la mete en *buffer*.

```c
            if (flush_mode)
            {
                ap_rprintf(r, "%s<br>", buffer);
            }
```
Si estamos en modo flush, imprimimos la línea leída y continuamos con el bucle.

```c
            else
            {
                /*
                 * Declaramos un buffer auxiliar (para usar 'strtok') y las variables
                 * para referenciar el usuario, la contraseña y la información
                 * leída del fichero de logins.
                 */
                char aux_buf[BUF_SIZE], *f_user, *f_passwd, *f_info;
                strcpy(aux_buf, buffer);
	
                f_user = strtok(aux_buf, ":");
	
                // Si el usuario que nos dan coincide.
                if (f_user && !strcmp(user, f_user))
                {
                    user_found = TRUE;
                    f_passwd = strtok(NULL, ":");
	
                    // Si la contraseña coincide.
                    if (f_passwd && !strcmp(passwd, f_passwd))
                    {
                        f_info = strtok(NULL, ";");
	
                        // Imprimimos la información del usuario leída del fichero de logins.
                        ap_rprintf(r, "Bienvenido <b>%s</b>, autenticacion correcta, "
                                        "tu informacion secreta es: ",
                                        user);
	
                        if (f_info)
                            ap_rprintf(r, "<b><span style=\"color:blue\">'%s'</span></b>.", f_info);
	
                        break;
                    }
```
Comprobamos usuario / contraseña contra la línea leída de fichero, si coinciden, imprimimos la información del usuario que también está guardada en el fichero de logins.

```c
                    // Si la contraseña no coincide.
                    else
                    {
                        /*
                         * Imprimimos un mensaje de error y guardamos la información del login
                         * incorrecto en el archivo de logs.
                         */
	
                        ap_rputs("<span style=\"color:red\"><b>Contrasena incorrecta!</b> "
                                    "Este incidente sera reportado al admin :)</span>", r);
	
                        sprintf(aux_buf, "Password incorrect login= %s:%s;\r",
                            user, passwd);
	
                        apr_size_t b_written = strlen(aux_buf);
                        apr_file_write(f_logs, aux_buf, &b_written);
	
                        break;
                    }
                }
            }
        }

        apr_file_close(f_logins);
        apr_file_close(f_logs);
    }
```
Si el usuario coincide pero la contraseña no, imprimimos un mensaje de error y guardamos el intento de login incorrecto en el fichero de logs.

```c
    if (flush_mode)
        ap_rputs("</code></p>", r);
    else if (!user_found)
        ap_rprintf(r, "<span style=\"color:red\">El usuario <b>%s</b> no existe!</span>",
                user);

    return OK;
}
```
Por último, si al recorrer el fichero de logins no hemos encontrado el usuario que nos han indicado, indicamos que no existe. Devolvemos [*OK*](http://ci.apache.org/projects/httpd/trunk/doxygen/group__APACHE__CORE__DAEMON.html#gaba51915c87d64af47fb1cc59348961c9).

## Archivo *.conf* de configuración del módulo

Al usar [apxs](#compilar-instalar-y-activar-nuestro-módulo-en-el-servidor) para instalar nuestro módulo en Apache, crea un archivo *auth_example.load* en la carpeta *mods-available* del directorio de instalación de Apache (en mi caso `/etc/apache2`) que contiene lo siguiente:

`LoadModule auth_example_module /usr/lib/apache2/modules/mod_auth_example.so`

También habrá creado un enlace simbólico a este fichero dentro del directorio *mods-enabled*. Al iniciarse, Apache leerá este fichero y cargará nuestro módulo que se encuentra en la ruta indicada.

Solamente queda especificar la configuración de nuestro módulo en su respectivo archivo *.conf*, en la terminal:

`nano /etc/apache2/mods-available/auth_example.conf`

Y escribir la configuración que deseemos, por ejemplo:
```
<IfModule auth_example_module>
    <Location "/auth">
        SetHandler auth_example-handler
        AuthExampleLoginsPath "/etc/apache2/mod_auth_example-logins"
        AuthExampleLogsPath "/etc/apache2/mod_auth_example-logs"
        AuthExampleFlush deny
    </Location>

    <Location "/auth/flush">
        AuthExampleFlush allow
    </Location>

    <Location "/auth/flush/deny">
        AuthExampleFlush deny
    </Location>
</IfModule>
```
Con esta configuración, le estamos indicando a Apache que el handler de la localización */auth* es *auth_example-handler* (nuestro handler) y configurando las [directivas](#directivas) de nuestro módulo con las rutas hacia los ficheros de logins y logs y la directiva para permitir o no el modo flush. Como fue explicado en la función [*merge_dir_conf*](#merge_dir_conf), las localizaciones son capaces de heredar naturalmente la configuración de su padre.
Para que Apache cargue el archivo *.conf* de nuestro módulo al arrancar, tendremos también que crear un enlace simbólico a este fichero en la carpeta *mods-enabled*:

`ln -s /etc/apache2/mods-available/auth_example.conf /etc/apache2/mods-enabled`

Finalmente, faltaría crear los ficheros de logins y logs con los permisos adecuados:
```
touch /etc/apache2/mod_auth_example-logins
touch /etc/apache2/mod_auth_example-logs

chgrp www-data /etc/apache2/mod_auth_example-logins
chgrp www-data /etc/apache2/mod_auth_example-logs

chmod g+rwx /etc/apache2/mod_auth_example-logins
chmod g+rwx /etc/apache2/mod_auth_example-logs
```
Y rellenar el fichero de logins con algunos datos, por ejemplo:
```
valen:mypasswd:This is my secret message.;
nelav:passwdmy:My secret message is this.;
prueba:apasswd:Mi prueba.;
```

Antes de probar si la configuración funciona, ¡hay que reiniciar Apache (`apachectl restart`)! (:
