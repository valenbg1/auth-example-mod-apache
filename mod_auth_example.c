/*
 * Módulo de ejemplo para Apache httpd 2.4
 *
 * Autentica un usuario contra un fichero de logins posibles.
 * Basado en http://httpd.apache.org/docs/2.4/developer/modguide.html
 *
 *
 * Valen Blanco	<http://github.com/valenbg1>.
 */

// Directivas para incluir funciones y símbolos de Apache.
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"
#include "util_script.h"

// Tamaño para los arrays de char.
#define BUF_SIZE 256

// Comprueba si una c string está vacía.
#define STREMPTY(string) (string[0] == '\0')

// Definición del tipo bool para mayor legibilidad del código.
typedef char bool;

// Estructura que contiene la configuración del módulo.
typedef struct
{
	char context[BUF_SIZE]; // Argumento que pasa Apache al crear una configuración.
	char logins_path[BUF_SIZE]; // Path hacia el fichero de logins.
	char logs_path[BUF_SIZE]; // Path hacia el fichero de logs.
	bool flush; // Permite o no visualizar el fichero de logins completo (p.e. al acceder a localhost/auth/flush).
} auth_ex_cfg;

// Declaración de prototipos.
static int auth_example_handler(request_rec *r);

static void register_hooks(apr_pool_t *p);

const char *set_logins_path(cmd_parms *cmd, void *cfg, const char *arg);

const char *set_logs_path(cmd_parms *cmd, void *cfg, const char *arg);

const char *set_flush(cmd_parms *cmd, void *cfg, const char *arg);

void *create_dir_conf(apr_pool_t *pool, char *context);

void *merge_dir_conf(apr_pool_t *pool, void *BASE, void *ADD);
// Fin declaración de prototipos.

/*
 * Directivas de configuración del módulo para Apache, que son
 * los argumentos posibles para nuestro módulo.
 */
static const command_rec directives[] =
{
	/*
	 * Con esta macro declaramos una directiva con nombre 'AuthExampleLoginsPath',
	 * que acepta 1 parámetro, la función que la maneja es 'set_logins_path', a NULL
	 * la función opcional que guarda la configuración, y el contexto en el que se aceptará
	 * esta directiva, en este caso 'ACCESS_CONF', permitimos este argumento en archivos '.conf'
	 * dentro de bloques tipo <Directory> o <Location>. El último argumento es una breve descripción
	 * de la directiva.
	 */
	AP_INIT_TAKE1("AuthExampleLoginsPath", set_logins_path, NULL, ACCESS_CONF,
		"Sets the logins file path"),

	AP_INIT_TAKE1("AuthExampleLogsPath", set_logs_path, NULL, ACCESS_CONF,
		"Sets the logs file path"),
	AP_INIT_TAKE1("AuthExampleFlush", set_flush, NULL, ACCESS_CONF,
		"Allows the 'flush' mode [allow|deny]"),
	{ NULL }
};

// Declaración del módulo y funciones para engachar con Apache.
module AP_MODULE_DECLARE_DATA auth_example_module =
{
    STANDARD20_MODULE_STUFF,
    create_dir_conf, // Función que creará una nueva estructura 'auth_ex_cfg'.
    merge_dir_conf, // Función para mezclar dos estructuras 'auth_ex_cfg'.
    NULL, // Función que crea una nueva estructura 'auth_ex_cfg' (para servidores distintos).
    NULL, // Función para mezclar dos estructuras 'auth_ex_cfg' (para servidores distintos).
    directives, // Tabla de directivas para nuestro módulo.
    register_hooks // Función para registrar nuestras funciones con Apache.
};

/*
 * Handler de nuestro módulo, que es el que se encargará de manejar toda la autenticación
 * del usuario y devolver resultados a la consulta HTTP.
 */
static int auth_example_handler(request_rec *r)
{
	/*
	 * Como Apache irá preguntando a cada módulo si tiene un handler llamado
	 * 'auth_example-handler', rechazamos todas las peticiones en las que el nombre
	 * del handler sea distinto.
	 */
    if (!r->handler || strcmp(r->handler, "auth_example-handler"))
        return DECLINED;

    /*
     * Pedimos a Apache que nos dé la estructura de configuración (que es dependiente del contexto, p.e.
     * bloques tipo <Directory> o <Location> en el archivo '.conf' del módulo).
     */
    auth_ex_cfg *config = (auth_ex_cfg*) ap_get_module_config(r->per_dir_config, &auth_example_module);

    /*
     * Ahora vamos a comprobar si podemos ejecutar un 'stat' sobre los ficheros de logins y logs
     * respectivamente, usando la APR, que es una API uniforme e independiente de la plataforma para
     * funciones comunes de los SOs. Si los paths no apuntan a un fichero o apuntan
     * a un directorio, devolvemos un 'HTTP_NOT_FOUND' (404). Si no se ha podido ejecutar el 'stat',
     * probablemente no tenemos permiso para acceder a los ficheros, devolvemos un 'HTTP_FORBIDDEN'
     * (403). Usamos el pool de memoria que nos proporciona Apache.
     */

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


    apr_file_t *f_logins, *f_logs;
	apr_table_t *GET;
	const char *user, *passwd;
	bool flush_mode, user_found = FALSE;

	/*
	 * Pedimos a Apache que nos dé la tabla de argumentos de la consulta HTTP tipo GET en
	 * la estructura correspondiente. Luego usamos las funciones definidas que nos permiten
	 * parsear la tabla para recoger los argumentos 'user' y 'passwd' respectivamente.
	 */

    ap_args_to_table(r, &GET);
    user = apr_table_get(GET, "user");
    passwd = apr_table_get(GET, "passwd");

    /*
     * Estamos en modo flush si 'config->flush' es TRUE y no nos proporcionan
     * usuario o contraseña. En caso contrario, si falta alguno de los dos argumentos
     * devolvemos un 'HTTP_NETWORK_AUTHENTICATION_REQUIRED' (511).
     */

    flush_mode = config->flush && (!user || !passwd);

    if (!flush_mode && (!user || !passwd))
    	return HTTP_NETWORK_AUTHENTICATION_REQUIRED;

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


    /*
     * Ahora procedemos a abrir los ficheros de logins (en modo lectura buffered) y de logs (en modo escritura
     * append), con los permisos por defecto y usando el pool de memoria que nos proporciona Apache.
     * Una mejor solución sería tener estos ficheros cargados previamente en memoria (p.e. en una
     * configuración inicial del módulo) para poder consultarlos más rápidamente en cada petición.
     */

    if ((apr_file_open(&f_logins, config->logins_path, APR_FOPEN_READ | APR_FOPEN_BUFFERED, APR_OS_DEFAULT, r->pool)
    		== APR_SUCCESS) &&
    		(apr_file_open(&f_logs, config->logs_path, APR_FOPEN_WRITE | APR_FOPEN_APPEND, APR_OS_DEFAULT, r->pool)
				== APR_SUCCESS))
    {
    	char buffer[BUF_SIZE];

    	/*
    	 * 'apr_file_gets' lee una línea de fichero cada vez, y la mete en 'buffer'. Evidentemente
    	 * recorrer cada línea del fichero en busca de la que corresponda con usuario/contraseña
    	 * no es la mejor de las opciones.
    	 */
    	while (apr_file_gets(buffer, BUF_SIZE, f_logins) == APR_SUCCESS)
    	{
    		if (flush_mode)
    		{
    			// Si estamos en modo flush, imprimimos la línea leída y continuamos con el bucle.
    			ap_rprintf(r, "%s<br>", buffer);
    		}
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
										"tu informacion secreta es: ", user);

						if (f_info)
							ap_rprintf(r, "<b><span style=\"color:blue\">'%s'</span></b>.", f_info);

						break;
					}
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

    if (flush_mode)
    	ap_rputs("</code></p>", r);
    else if (!user_found)
    	ap_rprintf(r, "<span style=\"color:red\">El usuario <b>%s</b> no existe!</span>",
					user);

    return OK;
}

/*
 * Función para registrar nuestras funciones con Apache.
 */
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

/*
 * Función que creará una nueva estructura 'auth_ex_cfg'. Ponemos valores por defecto
 * para cada campo de nuestra estructura.
 */
void *create_dir_conf(apr_pool_t *pool, char *context)
{
	if (!context)
		context = "Undefined context";

	/*
	 * Pedimos memoria para alojar una nueva estructura 'auth_ex_cfg'
	 * en el pool de memoria que nos deja Apache para nuestro módulo
	 * (referenciado con el argumento 'pool'). En este caso además se inicializa
	 * el espacio reservado con 0's.
	 */
	auth_ex_cfg *cfg = apr_pcalloc(pool, sizeof(auth_ex_cfg));

	if (cfg)
	{
		// Valores por defecto para la configuración.

		strcpy(cfg->context, context);
		strcpy(cfg->logins_path, "/etc/apache2/mod_auth_example-logins");
		strcpy(cfg->logs_path, "/etc/apache2/mod_auth_example-logs");
		cfg->flush = FALSE;
	}

	return cfg;
}

/*
 * Función para mezclar dos estructuras 'auth_ex_cfg'. Sirve para que directorios
 * o localizaciones puedan heredar la configuración de su padre. Por ejemplo,
 * si tenemos una localización 'localhost/auth' y otra 'localhost/auth/flush', al crear la configuración
 * de 'localhost/auth/flush' Apache creará primero la configuración de 'localhost/auth' (leyendo del fichero
 * .conf), después la de 'localhost/auth/flush', y nos pasará punteros a ambas estructuras de configuración
 * en los argumentos 'BASE' y 'ADD' respectivamente. Es nuestra responsabilidad definir cómo se integran
 * estas dos configuraciones.
 */
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

/*
 * Controla la asignación de un valor al campo 'logins_path' de nuestra
 * estructura de configuración 'auth_ex_cfg'. Es decir, qué pasa cuando
 * en un fichero de configuración tenemos una directiva 'AuthExampleLoginsPath'
 */
const char *set_logins_path(cmd_parms *cmd, void *cfg, const char *arg)
{
	// Estructura de configuración creada previamente.
	auth_ex_cfg *config = (auth_ex_cfg*) cfg;

	if (config)
	{
		/*
		 * '*arg' sería el argumento del parámetro 'AuthExampleLoginsPath' que
		 * se encuentra en el fichero .conf
		 * (p.e. AuthExampleLoginsPath "/etc/apache2/mod_auth_example-logins").
		 */
		strcpy(config->logins_path, arg);
	}

	return NULL;
}

/*
 * Controla la asignación de un valor al campo 'logs_path' de nuestra
 * estructura de configuración 'auth_ex_cfg'. Es decir, qué pasa cuando
 * en un fichero de configuración tenemos una directiva 'AuthExampleLogsPath'
 */
const char *set_logs_path(cmd_parms *cmd, void *cfg, const char *arg)
{
	// Estructura de configuración creada previamente.
	auth_ex_cfg *config = (auth_ex_cfg*) cfg;

	if (config)
	{
		/*
		 * '*arg' sería el argumento del parámetro 'AuthExampleLogsPath' que
		 * se encuentra en el fichero .conf
		 * (p.e. AuthExampleLogsPath "/etc/apache2/mod_auth_example-logs").
		 */
		strcpy(config->logs_path, arg);
	}

	return NULL;
}

/*
 * Controla la asignación de un valor al campo 'flush' de nuestra
 * estructura de configuración 'auth_ex_cfg'. Es decir, qué pasa cuando
 * en un fichero de configuración tenemos una directiva 'AuthExampleFlush'
 */
const char *set_flush(cmd_parms *cmd, void *cfg, const char *arg)
{
	// Estructura de configuración creada previamente.
	auth_ex_cfg *config = (auth_ex_cfg*) cfg;

	if (config)
	{
		/*
		 * '*arg' sería el argumento del parámetro 'AuthExampleFlush' que
		 * se encuentra en el fichero .conf (p.e. AuthExampleFlush allow).
		 */
		config->flush = !strcasecmp(arg, "allow") ? TRUE : FALSE;
	}

	return NULL;
}
