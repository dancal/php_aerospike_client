#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_interfaces.h" // needed for calling the internal serialize function
#include "Zend/zend_exceptions.h" // the serialize boilerplate's throwing exceptions, so I should too

#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"

#include "citrusleaf/cf_shash.h"
#include "citrusleaf/citrusleaf.h"
#include "php_citrusleaf.h"
#include "citrusleaf/cl_shm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define DEBUG 1


/**** Example php function. Testing for module is loaded ****/
PHP_FUNCTION(citrusleaf)
{
    RETURN_STRING("Citrusleaf lives!", 1);
}

/**** Internal object translation functions ****/
// You have to pass in an empty zval that you've created.
// we will not create your zval for you.
// this is to allow the efficiency of using &return_value, which is often
// what you want to do with this object

int
cl_object_unserialize_to_zval(cl_object *o, zval **z)
{

	php_unserialize_data_t var_hash;
	zval *var_zval;
	
	PHP_VAR_UNSERIALIZE_INIT(var_hash);
	// these calling conventions are ghetto. Pass in a char ** to the data and a char * to the end?
	// instead of just pointer and length? pluuuueze.
	if (1 != php_var_unserialize(z, (const unsigned char **) &(o->u.blob), o->u.blob + o->sz,
		&var_hash TSRMLS_CC) ) {
	
		zend_error(E_ERROR, "citrusleaf: could not unserialize server data");
		return(-1);
	
	}
	
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
	
	return(0);	
}

//
// This code was cribbed from 'shm_put_var', which serializes variables
// into shared memory with keys so they can be shared among multiple processes
//
int
zval_serialize_to_cl_object(zval *z, cl_object *o)
{
	smart_str var_str = {0};
	php_serialize_data_t var_hash;
	
	PHP_VAR_SERIALIZE_INIT(var_hash);
	php_var_serialize(&var_str, &z, &var_hash TSRMLS_CC);
	PHP_VAR_SERIALIZE_DESTROY(var_hash);
	
	// your byte buffer is now:
	//   var_str.c , var_str.len
	// I will do the nasty and make a data copy. Bleh.
	// can't use the wrapper funcs because they don't set 'free'
	char *buf = malloc(var_str.len);
	memcpy(buf, var_str.c, var_str.len);
	o->type = CL_PHP_BLOB;
	o->sz = var_str.len;
	o->u.blob = buf;
	o->free = buf;
	
	// and free the smart string
	smart_str_free(&var_str);
	
	// later, be able to just do this and do the smart_str_free later
   	// citrusleaf_object_init_blob(o, var_str.c, var_str.len);
   	
	return(0);
}


//
// What's going on here?
// Function takes a live zval and sets up a CL object that points to the data,
// with CL typing.
// What's with the 'zval_dtor'? This is an object that you'll have to destry
// after you're done with the cl_object, if it's not null. This happens in the
// case where we have to serialize the object. A data copy could be done, but it's
// snazzier to point the cl object directly into the zval that gets created by the
// serializer.
//
int
zval_to_citrusleaf_object(zval *z, cl_object *o)
{

	switch (Z_TYPE_P(z)) {
		case IS_NULL:
			citrusleaf_object_init_null(o);
			break;
		case IS_BOOL:
			citrusleaf_object_init_int(o, Z_BVAL_P(z));
			break;
		case IS_LONG:
			citrusleaf_object_init_int(o, Z_LVAL_P(z));
			break;
		case IS_STRING:
			citrusleaf_object_init_str2(o, Z_STRVAL_P(z),Z_STRLEN_P(z));
			// php_printf("<p> KEY %s </p", o->u.str);
			break;
			
		// everything that's more complicated gets the PHP serializer!
		case IS_DOUBLE:
		case IS_ARRAY:
		case IS_OBJECT:
		case IS_RESOURCE:
			
			return( zval_serialize_to_cl_object(z, o) );
			
		default:
			zend_error(E_NOTICE, "zval_to_object: could not convert %d\n",Z_TYPE_P(z));
			return(-1);
			
	}
	
	return(0);
}

/**** Definition of the CitrusleafResponse class ****/
zend_class_entry *php_citrusleaf_response_entry;
#define PHP_CITRUSLEAF_RESPONSE_NAME "CitrusleafResponse"

typedef struct {
	zend_object zo;
    int response_code;
    int generation;
    cl_bin *bin_vals;
    int n_bins;
    bool bins_emalloced; /*Flag to check if emalloc is used*/

} citrusleaf_response;

static zend_object_handlers citrusleaf_response_handlers;

static void citrusleaf_response_zend_dtor(void *object, zend_object_handle handle TSRMLS_DC)
{	
	zend_objects_destroy_object( object, handle TSRMLS_CC);
}

static void citrusleaf_response_zend_free(zend_object *object TSRMLS_DC)
{	
    // @TODO need to free the fields of the custom object
	efree(object);
}

zend_object_value citrusleaf_response_create(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value retval;
	citrusleaf_response *o;

	o = ecalloc(1, sizeof(citrusleaf_response));
	zend_object_std_init(&o->zo, ce TSRMLS_CC);

	retval.handle = zend_objects_store_put(o, citrusleaf_response_zend_dtor, 
			(zend_objects_free_object_storage_t) citrusleaf_response_zend_free, 0 TSRMLS_CC);
	
	retval.handlers = &citrusleaf_response_handlers;

	return (retval);
	
}

PHP_METHOD( CitrusleafResponse, __construct )
{
	return_value = getThis();
}

/* CitrisueafResponse.get_response_code */
PHP_METHOD(CitrusleafResponse, get_response_code)
{
	citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(getThis() TSRMLS_CC);

	RETURN_LONG(clr->response_code );	
}

/* CitrisueafResponse.get_generation */
PHP_METHOD(CitrusleafResponse, get_generation)
{
	citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(getThis() TSRMLS_CC);

	RETURN_LONG(clr->generation );	
}
/* CitrisueafResponse.get_bins */
PHP_METHOD(CitrusleafResponse, get_bins)
{
	citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(getThis() TSRMLS_CC);
    array_init(return_value);

    int i;
    cl_bin *bin_ptr = clr->bin_vals;

    for (i=0;i<clr->n_bins;i++) {
	    if ((bin_ptr[i].object.type == CL_STR) || (bin_ptr[i].object.type == CL_BLOB)) {
		    add_assoc_stringl(return_value,bin_ptr[i].bin_name, bin_ptr[i].object.u.str,bin_ptr[i].object.sz,1);
	    }
	    else if (bin_ptr[i].object.type == CL_INT) {
		    add_assoc_long(return_value,bin_ptr[i].bin_name, bin_ptr[i].object.u.i64);
	    }
	    else if (bin_ptr[i].object.type == CL_PHP_BLOB) {
            zval *obj;
            MAKE_STD_ZVAL(obj);
		    if (0 != cl_object_unserialize_to_zval(& (bin_ptr[i].object), &obj)) {
			    zend_error(E_ERROR, "citrusleaf get: failure unserializing");
			    RETURN_NULL();
		    }
		    add_assoc_zval(return_value,bin_ptr[i].bin_name, obj);
	    } else {
		    zend_error(E_NOTICE, "citrusleaf get: unrecognized type name %s type %d",bin_ptr[i].bin_name,bin_ptr[i].object.type);
        }
    }
    return;
}

PHP_METHOD(CitrusleafResponse, free_bins) {
	citrusleaf_response *clr = (citrusleaf_response *)zend_object_store_get_object(getThis() TSRMLS_CC);
	if (clr->bin_vals) {
		/* First free the space allocated for the values of the bin */
		citrusleaf_bins_free(clr->bin_vals, clr->n_bins);
		/* If bins_emalloced is set, efree the bin values else, free them */
		if (clr->bins_emalloced) {
			efree(clr->bin_vals);
		} else {
			free(clr->bin_vals);
		}
	}
	clr->bin_vals=NULL;
	RETURN_NULL();	
}
	
static zend_function_entry php_citrusleaf_response_functions[] = {
	PHP_ME(CitrusleafResponse, __construct, NULL /*arginfo*/, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(CitrusleafResponse, get_response_code, NULL /*arginfo*/, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(CitrusleafResponse, get_generation, NULL /*arginfo*/, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(CitrusleafResponse, get_bins, NULL /*arginfo*/, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(CitrusleafResponse, free_bins, NULL /*arginfo*/, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};

/**** Definition of CitrusleafClient class *****/
zend_class_entry *php_citrusleaf_client_entry;
#define PHP_CITRUSLEAF_CLIENT_NAME "CitrusleafClient"

PHP_METHOD(CitrusleafClient, helloWorld)
{
	RETURN_STRING("Citrusleaf says hello", 1);
}

// custom object backing CitrusleafClient
typedef struct {
	zend_object zo;
	cl_cluster *asc;
	char	namespace[32];
	char    set[32];
	char    iset[32];
	char    bin[32];
    int     timeout;
    int     writepolicy;
} citrusleaf_object;

static zend_object_handlers citrusleaf_object_handlers;

static void citrusleaf_object_zend_dtor(void *object, zend_object_handle handle TSRMLS_DC)
{	
	zend_objects_destroy_object( object, handle TSRMLS_CC);
}

static void citrusleaf_object_zend_free(zend_object *object TSRMLS_DC)
{
	citrusleaf_object *co = (citrusleaf_object *) object;

	zend_object_std_dtor( &co->zo TSRMLS_CC);

	// @TODO: release asc
	
	efree(object);
}

zend_object_value citrusleaf_object_create(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value retval;
	citrusleaf_object *o;
	
	o = ecalloc(1, sizeof(citrusleaf_object));

	o->asc = 0;
	zend_object_std_init(&o->zo, ce TSRMLS_CC);

	retval.handle = zend_objects_store_put(o, citrusleaf_object_zend_dtor, 
			(zend_objects_free_object_storage_t) citrusleaf_object_zend_free, 0 TSRMLS_CC);
	
	retval.handlers = &citrusleaf_object_handlers;

	return (retval);
	
}

/* CitrusleafClient.set_log_level */
PHP_METHOD(CitrusleafClient, set_log_level)
{
	char *loglevel;
	int loglevel_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &loglevel, &loglevel_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	if (0 == strncmp("error", loglevel, 5)) {
		cf_set_log_level(CF_ERROR);
		RETURN_LONG(CITRUSLEAF_RESULT_OK);
	} else if (0 == strncmp("warn", loglevel, 4)) {
		cf_set_log_level(CF_WARN);
		RETURN_LONG(CITRUSLEAF_RESULT_OK);
	} else if (0 == strncmp("info", loglevel, 4)) {
		cf_set_log_level(CF_INFO);
		RETURN_LONG(CITRUSLEAF_RESULT_OK);
	} else if (0 == strncmp("debug", loglevel, 5)) {
		cf_set_log_level(CF_DEBUG);
		RETURN_LONG(CITRUSLEAF_RESULT_OK);
	} else {
		RETURN_LONG(CITRUSLEAF_RESULT_PARAMETER_ERROR);
	}
}	

static int g_logfd = -1;

static void
cl_phpclient_log_cb(cf_log_level level, const char* fmt_no_newline, ...)
{
	char buff[512];
	size_t fmt_size = strlen(fmt_no_newline) + 2;
	char fmt[fmt_size];

	// timestamp
	time_t ltime;
	struct tm *mytm;

	ltime = time(NULL);
	mytm = localtime(&ltime);
	sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d(%d): ", 
		1900+mytm->tm_year, 1+mytm->tm_mon, mytm->tm_mday, 
		mytm->tm_hour, mytm->tm_min, mytm->tm_sec, getpid());

	// format the data
	strncpy(fmt, fmt_no_newline, fmt_size);
	fmt[fmt_size - 2] = '\n';

	va_list ap;
	va_start(ap, fmt_no_newline);
	vsnprintf(buff+strlen(buff), 512, fmt, ap);
	va_end(ap);

	buff[511] = '\0';
	if (0 <= g_logfd) {
		int n = write(g_logfd, buff, strlen(buff));
	} else {
		// this should not happen
		fprintf(stderr, "Cannot write log due to invalid file descriptor\n");
		fprintf(stderr, "%s", buff);
	}
}

int 
do_set_log_file(char *logfile) {
	char fulllogfile[512];
	strncpy(fulllogfile, logfile, 511);
	fulllogfile[500] = '\0';
	
	// add pid because there could be many processes
	// sprintf(fulllogfile+strlen(fulllogfile), "_%d", getpid());
	
	int fd = open(fulllogfile, O_RDWR|O_CREAT|O_APPEND, 0644);
	if (0 > fd) {
		if (errno != 0) {
			return errno;
		} else{
			return -1;
		}
	} else {
		cf_set_log_callback(cl_phpclient_log_cb);
		g_logfd = fd;
		return 0;
	}
}	
cl_result_codes_enum make_php_rv(cl_rv rv){
	cl_result_codes_enum php_rv = CITRUSLEAF_RESULT_UNKNOWN;
	

	//Check the result from citrusleaf server and map it to php understandable one
	switch(rv){
		case CITRUSLEAF_OK:
			php_rv=CITRUSLEAF_RESULT_OK;
			break;
		// Both the client-timeout and serverside-timeout will be reported as timeout
		case CITRUSLEAF_FAIL_SERVERSIDE_TIMEOUT:
		case CITRUSLEAF_FAIL_TIMEOUT:
			php_rv = CITRUSLEAF_RESULT_TIMEOUT;
			break;
		case CITRUSLEAF_FAIL_NOTFOUND:
			php_rv=CITRUSLEAF_RESULT_KEY_NOT_FOUND_ERROR;
			break;
		case CITRUSLEAF_FAIL_PARAMETER:
			php_rv=CITRUSLEAF_RESULT_PARAMETER_ERROR;
			break;
		case CITRUSLEAF_FAIL_GENERATION:
			php_rv=CITRUSLEAF_RESULT_GENERATION_ERROR;
			break;
		case  CITRUSLEAF_FAIL_KEY_EXISTS:
			php_rv=CITRUSLEAF_RESULT_KEY_FOUND_ERROR;
			break;
		case CITRUSLEAF_FAIL_BIN_EXISTS:
			php_rv=CITRUSLEAF_RESULT_BIN_FOUND_ERROR;
			break;
		case CITRUSLEAF_FAIL_CLIENT:
			php_rv=CITRUSLEAF_RESULT_FAIL_CLIENT;
			break;
		case CITRUSLEAF_FAIL_UNKNOWN:
			php_rv=CITRUSLEAF_RESULT_UNKNOWN;
			break;
    	case CITRUSLEAF_FAIL_ASYNCQ_FULL:
			php_rv=CITRUSLEAF_RESULT_FAIL_ASYNCQ_FULL;
			break;
		case CITRUSLEAF_FAIL_CLUSTER_KEY_MISMATCH:
			php_rv=CITRUSLEAF_RESULT_CLUSTER_KEY_MISMATCH;
			break;
		case CITRUSLEAF_FAIL_PARTITION_OUT_OF_SPACE:
			php_rv=CITRUSLEAF_RESULT_PARTITION_OUT_OF_SPACE;
			break;
		case CITRUSLEAF_FAIL_NOXDS:
			php_rv=CITRUSLEAF_RESULT_NO_XDS;
			break;
		case CITRUSLEAF_FAIL_UNAVAILABLE:
			php_rv=CITRUSLEAF_RESULT_SERVER_UNAVAILABLE;
			break;
		case CITRUSLEAF_FAIL_INCOMPATIBLE_TYPE:
			php_rv=CITRUSLEAF_RESULT_INCOMPATIBLE_TYPE;
			break;
		case CITRUSLEAF_FAIL_RECORD_TOO_BIG:
			php_rv=CITRUSLEAF_RESULT_RECORD_TOO_BIG;
			break;
		case  CITRUSLEAF_FAIL_KEY_BUSY:
			php_rv=CITRUSLEAF_RESULT_KEY_BUSY;
			break;
		default:
			php_rv=CITRUSLEAF_RESULT_UNKNOWN;	
	}
	return php_rv;
}
/* CitrusleafClient.set_log_file */
PHP_METHOD(CitrusleafClient, set_log_file)
{
	char *logfile;
	int logfile_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &logfile, &logfile_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	int rc_errno = do_set_log_file(logfile);
	if (0 == rc_errno) {
		RETURN_LONG(CITRUSLEAF_RESULT_OK);
	} else {
		RETURN_LONG(CITRUSLEAF_RESULT_PARAMETER_ERROR);
	}
}

PHP_METHOD(CitrusleafClient, close) {
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	citrusleaf_cluster_release_or_destroy(&clo->asc);
	//citrusleaf_cluster_destroy
}

/* CitrusleafClient.connect */
PHP_METHOD(CitrusleafClient, connect)
{
	zval *arr, **data;
	HashTable *arr_hash;
	HashPosition pointer;
	int array_count;
	char *connect_url;
	int   connect_url_len;

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	// defaults
        strcpy(clo->namespace, "test");
        strcpy(clo->bin,"binname");
        strcpy(clo->set, "");
        strcpy(clo->iset, "");
        clo->timeout=1000; // 1 second
        clo->writepolicy=CL_WRITE_RETRY;
	cl_cluster *asc;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC, "s", &connect_url, &connect_url_len) == SUCCESS) {
		/* If the string is separated by commas, tokenize the complete string into
		 * into smaller strings and call cluster get on them individually. If result is 
		 * not okay, try with the next string.
		 */
		char * urlx = strdup(connect_url);
		char delim[] = ",";
		char * url = NULL;
		url = strtok(urlx,delim);
		while(url!=NULL){
			asc = citrusleaf_cluster_get(url);
			if (asc) {
				clo->asc = asc;
				RETURN_LONG(CITRUSLEAF_RESULT_OK);
			}
			url = strtok(NULL,delim);
		}
		zend_error(E_NOTICE, "citrusleaf connect: invalid hosts\n");
		RETURN_LONG(CITRUSLEAF_RESULT_NO_HOSTS);
	} else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arr) == SUCCESS) {
		arr_hash = Z_ARRVAL_P(arr);
		for(zend_hash_internal_pointer_reset_ex(arr_hash, &pointer);
			zend_hash_get_current_data_ex(arr_hash, (void**) &data, &pointer) == SUCCESS;
			zend_hash_move_forward_ex(arr_hash, &pointer)) {
			if (Z_TYPE_PP(data) == IS_STRING) {
				asc = citrusleaf_cluster_get(Z_STRVAL_PP(data));
				if (asc) {
					clo->asc = asc;
					RETURN_LONG(CITRUSLEAF_RESULT_OK);
				} else {
					continue;
				}
			}
		}
		RETURN_LONG(CITRUSLEAF_RESULT_NO_HOSTS);
	} else {
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
	}
}	

/* CitrusleafClient.set_default_binname */
PHP_METHOD(CitrusleafClient, set_default_binname)
{
	char *ns;
	int   ns_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ns, &ns_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);

	strcpy(clo->bin,ns);
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	

/* CitrusleafClient.get_default_binname */
PHP_METHOD(CitrusleafClient, get_default_binname)
{
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETVAL_STRING( clo->bin , 1);

    return;
}	

/* CitrusleafClient.set_default_namespace */
PHP_METHOD(CitrusleafClient, set_default_namespace)
{
	char *ns;
	int   ns_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ns, &ns_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);

	strcpy(clo->namespace,ns);
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	


PHP_METHOD(CitrusleafClient, use_shm)
{
	long num_nodes;
	long key;
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &num_nodes, &key) == SUCCESS){
		if(CL_SHM_OK == citrusleaf_use_shm(num_nodes,key)){
			RETURN_TRUE;
		}
		else{
			RETURN_FALSE;
		}
	}
}

PHP_METHOD(CitrusleafClient, free_shm)
{
	if (CL_SHM_OK == citrusleaf_shm_free()){
		RETURN_TRUE;
	}
	else{
		RETURN_FALSE;
	}
}


/* CitrusleafClient.get_default_namespace */
PHP_METHOD(CitrusleafClient, get_default_namespace)
{
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETVAL_STRING( clo->namespace , 1);

    return;
}	

/* CitrusleafClient.set_default_timeout */
PHP_METHOD(CitrusleafClient, set_default_timeout)
{
	long  timeout;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &timeout) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	clo->timeout=timeout;
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	

/* CitrusleafClient.get_default_timeout */
PHP_METHOD(CitrusleafClient, get_default_timeout)
{
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETURN_LONG( clo->timeout);
}	

/* CitrusleafClient.set_default_writepolicy */
PHP_METHOD(CitrusleafClient, set_default_writepolicy)
{
	long  writepolicy;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &writepolicy) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	clo->writepolicy=writepolicy;
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	

/* CitrusleafClient.get_default_writepolicy */
PHP_METHOD(CitrusleafClient, get_default_writepolicy)
{
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETURN_LONG( clo->writepolicy);
}	

/* CitrusleafClient.set_default_set */
PHP_METHOD(CitrusleafClient, set_default_set)
{
	char *set;
	int   set_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &set, &set_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	strcpy(clo->set,set);
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	

/* CitrusleafClient.get_default_set */
PHP_METHOD(CitrusleafClient, get_default_set)
{
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETVAL_STRING( clo->set , 1);

    return;
}	

/* CitrusleafClient.set_default_iset */
PHP_METHOD(CitrusleafClient, set_default_iset)
{
	char *set;
	int   set_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &set, &set_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	strcpy(clo->iset,set);
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	

/* CitrusleafClient.set_default_bin */
PHP_METHOD(CitrusleafClient, set_default_bin)
{
	char *bin;
	int   bin_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &bin, &bin_len) == FAILURE) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	strcpy(clo->bin,bin);
	
    RETURN_LONG(CITRUSLEAF_RESULT_OK); 
}	
/* CitrusleafClient.get_default_bin */
PHP_METHOD(CitrusleafClient, get_default_bin)
{
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETVAL_STRING( clo->bin , 1);

    return;
}	


/* CitrusleafClient.delete */
PHP_METHOD(CitrusleafClient, delete)
{
	zval *key_zval;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key_zval) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf delete: invalid params");
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key_zval, &o_key)) {
		zend_error(E_NOTICE, "citrusleaf cannot convert object");
        RETURN_LONG(CITRUSLEAF_RESULT_UNKNOWN); 
	}

	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf delete: Null key");
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
	}

	cl_write_parameters cl_wp;
	cl_write_parameters_set_default(&cl_wp);

	cl_rv rv = citrusleaf_delete(clo->asc, clo->namespace, clo->set, &o_key, &cl_wp);

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf delete: error %d",rv);
	}
	RETURN_LONG(make_php_rv(rv));
	
}	

/* CitrusleafClient.info */
PHP_METHOD(CitrusleafClient, info)
{
        char *hostname, *names = NULL, *values = NULL;
        long port, hostname_len, name_len, timeout = 0;

        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sls|l", &hostname, &hostname_len, &port, &names, &name_len, &timeout) == FAILURE) {
                zend_error(E_NOTICE, "citrusleaf info: invalid params");
                RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
        }

        if (!(*names)) {
                zend_error(E_NOTICE, "citrusleaf info: names cannot be empty");
                RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
        }

        if (0 != citrusleaf_info(hostname, port, names, &values, timeout)) {
                zend_error(E_NOTICE, "citrusleaf info: Failed info call");
                RETURN_LONG(CITRUSLEAF_RESULT_UNKNOWN);
        }

        array_init(return_value);
        if (values) {
                cf_vector_define(lines_v, sizeof(void *), 0);
                str_split('\n',values,&lines_v);
                for (uint j=0;j<cf_vector_size(&lines_v);j++) {
                        char *line = cf_vector_pointer_get(&lines_v, j);
                        cf_vector_define(pair_v, sizeof(void *), 0);
                        str_split('\t',line, &pair_v);

                        if (cf_vector_size(&pair_v) == 2) {
                                char *name = cf_vector_pointer_get(&pair_v,0);
                                char *value = cf_vector_pointer_get(&pair_v,1);
                                add_assoc_string(return_value, name, value, 1);
                        }
                        cf_vector_destroy(&pair_v);
                }
                cf_vector_destroy(&lines_v);
        }
        return;
}

/* CitrusleafClient.exists */
PHP_METHOD(CitrusleafClient, exists)
{
	zval *key_zval, *bin_names = NULL, **data;
	HashTable *arr_hash;
	HashPosition pointer;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|a", &key_zval, &bin_names) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf exists() requires one or two parameters\n");
		object_init_ex(return_value, php_citrusleaf_response_entry);
		citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
		clr->response_code = CITRUSLEAF_RESULT_INVALID_API_PARAM;
		return;
	}

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key_zval, &o_key)) {
		zend_error(E_NOTICE, "citrusleaf exists: could not serialize key failure");
		object_init_ex(return_value, php_citrusleaf_response_entry);
		citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
		clr->response_code = CITRUSLEAF_RESULT_UNKNOWN;
		return;
	}

	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf exists: Null key");
		object_init_ex(return_value, php_citrusleaf_response_entry);
		citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
		clr->response_code = CITRUSLEAF_RESULT_INVALID_API_PARAM;
		return;
	}

	uint32_t generation;
	cl_bin *bins;
	int n_bins, i;
	cl_rv rv;
	/* In future, there might be a case when we would want to check if a particular bin exists or not.
 	 * For that scenario, we have used emalloc in the following code, which is commented for now
 	 * and can be ignored. 
 	 */
//	bool bins_emalloced = false;
	if (bin_names && (Z_TYPE_P(bin_names) == IS_ARRAY)) {
		// If array of bin_names is specified, fill the names in bins and call
		// citrusleaf_exists_key, but those bins are currently ignored.
		arr_hash = Z_ARRVAL_P(bin_names);
		n_bins = zend_hash_num_elements(arr_hash);
		bins = emalloc (n_bins * sizeof(cl_bin));
//		bins_emalloced = true;
		zend_hash_internal_pointer_reset_ex(arr_hash, &pointer);
		for (i=0; i<n_bins; i++) {
			zend_hash_get_current_data_ex(arr_hash, (void**) &data, &pointer);
			strcpy(bins[i].bin_name, Z_STRVAL_PP(data));
			bins[i].object.type = CL_NULL;
			zend_hash_move_forward_ex(arr_hash, &pointer);
		}
		// NB: Ignoring bins!!
		rv = citrusleaf_exists_key(clo->asc, clo->namespace, clo->set, &o_key, 0 /* bins */, 0 /* n_bins */, clo->timeout, &generation);
	} else {
		// NB: Ignoring bins!!
		rv = citrusleaf_exists_key(clo->asc, clo->namespace, clo->set, &o_key, 0 /* &bins */, 0 /* &n_bins */, clo->timeout, &generation);
	}

	object_init_ex(return_value, php_citrusleaf_response_entry);
	citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
	clr->n_bins = n_bins;
	clr->bin_vals = bins;
	clr->generation = generation;
	clr->response_code=make_php_rv(rv);
/*	Ignore for now 
 	if (bins_emalloced) {
		clr->bins_emalloced = true;
	} else {
		clr->bins_emalloced = false;
	}*/

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf exists: error %d",rv);
	}

	return;
}

/* CitrusleafClient.get */
PHP_METHOD(CitrusleafClient, get)
{
	zval *key_zval, *bin_names = NULL, **data;
	HashTable *arr_hash;
	HashPosition pointer;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|a", &key_zval, &bin_names) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf get() requires one or two parameter\n");
		object_init_ex(return_value, php_citrusleaf_response_entry);
		citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
		clr->response_code = CITRUSLEAF_RESULT_INVALID_API_PARAM;
		return;
	}	

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key_zval, &o_key)) {
		zend_error(E_NOTICE, "citrusleaf get: could not serialize key failure");
		object_init_ex(return_value, php_citrusleaf_response_entry);
		citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
		clr->response_code = CITRUSLEAF_RESULT_UNKNOWN;
		return;
	}

	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf get: Null key");
		object_init_ex(return_value, php_citrusleaf_response_entry);
		citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
		clr->response_code = CITRUSLEAF_RESULT_INVALID_API_PARAM;
		return;
	}

	uint32_t generation;
	cl_bin *bins = NULL;
	int n_bins = 0, i;
	bool bins_emalloced = false;
	cl_rv rv;
	if (bin_names && (Z_TYPE_P(bin_names) == IS_ARRAY)) {
		// If array of bin_names is specified, fill the names in bins and call
		// citrusleaf_get to get values corresponding to those particular bins
		arr_hash = Z_ARRVAL_P(bin_names);
		n_bins = zend_hash_num_elements(arr_hash);
		bins = emalloc (n_bins * sizeof(cl_bin));
		bins_emalloced = true;
		zend_hash_internal_pointer_reset_ex(arr_hash, &pointer);
		for (i=0; i<n_bins; i++) {
			zend_hash_get_current_data_ex(arr_hash, (void**) &data, &pointer);
			strcpy(bins[i].bin_name, Z_STRVAL_PP(data));
			bins[i].object.type = CL_NULL;
			zend_hash_move_forward_ex(arr_hash, &pointer);
		}
		rv = citrusleaf_get(clo->asc, clo->namespace, clo->set, &o_key, bins, n_bins, clo->timeout, &generation);
	} else {
		rv = citrusleaf_get_all(clo->asc, clo->namespace, clo->set, &o_key, &bins, &n_bins, clo->timeout, &generation);
	}

	object_init_ex(return_value, php_citrusleaf_response_entry);
	citrusleaf_response *clr = (citrusleaf_response *) zend_object_store_get_object(return_value TSRMLS_CC);
	clr->n_bins = n_bins;
	clr->bin_vals = bins;
	clr->generation = generation;
	if (bins_emalloced) {
		clr->bins_emalloced = true;
	} else {
		clr->bins_emalloced = false;
	}
	clr->response_code=make_php_rv(rv);    

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf get: error %d",rv);
	}

	return;
}	

PHP_METHOD(CitrusleafClient, put)
{
	zval *key;
	zval *val_zval;
	long write_unique_flag=CITRUSLEAF_WRITE_UNIQUE_NONE;
	long generation=-1;
	long ttlseconds=-1;

	// checking parameters
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|lll", &key, &val_zval,&write_unique_flag,&generation,&ttlseconds) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf put: (key,bin_val_array,write_unique_only,generation,ttlseconds)");
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	

	#ifdef DEBUG
	zend_error(E_NOTICE, "put: write_unique_flag %d", write_unique_flag);
	zend_error(E_NOTICE, "put: generation %ld", generation);
	zend_error(E_NOTICE, "put: ttlseconds %ld", ttlseconds);
	#endif

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);

	// convert key to citrusleaf object	
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key, &o_key)) {
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}

	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf put: Null key");
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
	}

	cl_bin *values;	
	int val_size=0;
	// convert single value cl_object
	values= emalloc(1*sizeof(cl_bin));
	val_size = 1;
	strcpy(values[0].bin_name, clo->bin);
	if (0 != zval_to_citrusleaf_object(val_zval, &values[0].object)) {
		efree(values);
		values = NULL;
		RETURN_LONG(CITRUSLEAF_RESULT_UNKNOWN); 
	}

	// set up the write parameter
	cl_write_parameters cl_wp;
	cl_write_parameters_set_default(&cl_wp);
	cl_wp.timeout_ms = clo->timeout;
	cl_wp.w_pol = clo->writepolicy;
	if (write_unique_flag == CITRUSLEAF_WRITE_IF_KEY_NOT_EXIST) {
		cl_wp.unique = true;
	} else if (write_unique_flag == CITRUSLEAF_WRITE_IF_BINS_NOT_EXIST) {
		cl_wp.unique_bin = true;
	}
	if (generation>-1) {
		cl_wp.use_generation = true;
		cl_wp.generation = generation;
	}
	if (ttlseconds>-1) {
		cl_wp.record_ttl = ttlseconds;
	}

	//zend_error(E_NOTICE, "citrusleaf put: val_size=%d,val_type=%d",val_size,values[0].object.type);
	cl_rv rv = citrusleaf_put(clo->asc, clo->namespace, clo->set, &o_key, values, val_size, &cl_wp);

	efree(values);
	values = NULL;

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf put: error %d",rv);
	}

	RETURN_LONG(make_php_rv(rv));
  
}	

/* put many bins for one record. input is array of binname,value pairs */
PHP_METHOD(CitrusleafClient, putmany)
{
	zval *key;
    zval *val_zval;

    long write_unique_flag=CITRUSLEAF_WRITE_UNIQUE_NONE;
    long generation=-1;
    long ttlseconds=-1;
	
    // checking parameters
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|lll", &key, &val_zval,&write_unique_flag,&generation,&ttlseconds) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf put: (key,bin_val_array,write_unique_only,generation,ttlseconds)");
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	
    
#ifdef DEBUG
    zend_error(E_NOTICE, "put: write_unique_flag %d", write_unique_flag);
    zend_error(E_NOTICE, "put: generation %ld", generation);
    zend_error(E_NOTICE, "put: ttlseconds %ld", ttlseconds);
#endif

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);

    // convert key to citrusleaf object	
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key, &o_key)) {
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}

	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf putmany: Null key");
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
	}

    cl_bin *values;	
    int val_size=0;
    if (Z_TYPE_P(val_zval)==IS_ARRAY) {
        // convert bin/value array to citrusleaf object array
        zval **data;
        HashTable *array_hash;
        HashPosition pointer;
        int array_count;

        array_hash = Z_ARRVAL_P(val_zval);
        array_count = zend_hash_num_elements(array_hash);
        values= emalloc(array_count*sizeof(cl_bin));
        val_size = array_count;

        int counter = 0;
        for(zend_hash_internal_pointer_reset_ex(array_hash, &pointer); zend_hash_get_current_data_ex(array_hash, (void**) &data, &pointer) == SUCCESS; zend_hash_move_forward_ex(array_hash, &pointer)) {

            zval temp;
            char *key;
            int key_len;
            long index;

            // getting the key
            if (zend_hash_get_current_key_ex(array_hash, &key, &key_len, &index, 0, &pointer) == HASH_KEY_IS_STRING) {
        	    strcpy(values[counter].bin_name, key);
            } else {
        	    sprintf(values[counter].bin_name, "%ld",index);
            }

            // getting the value
	        if (0 != zval_to_citrusleaf_object(*data, &(values[counter].object))) {
                efree(values);
                values = NULL;
            	RETURN_LONG(CITRUSLEAF_RESULT_UNKNOWN); 
	        }
            counter++;
        }
    } else {
		zend_error(E_NOTICE, "citrusleaf put: expect an array of binname,value pairs");
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
    }    	
	
    // set up the write parameter
	cl_write_parameters cl_wp;
	cl_write_parameters_set_default(&cl_wp);
	cl_wp.timeout_ms = clo->timeout;
    cl_wp.w_pol = clo->writepolicy;
    cl_wp.unique = write_unique_flag;
    if (generation>-1) {
        cl_wp.use_generation = true;
        cl_wp.generation = generation;
    }
    if (ttlseconds>-1) {
        cl_wp.record_ttl = ttlseconds;
    }
	
	//zend_error(E_NOTICE, "citrusleaf put: val_size=%d,val_type=%d",val_size,values[0].object.type);
	cl_rv rv = citrusleaf_put(clo->asc, clo->namespace, clo->set, &o_key, values, val_size, &cl_wp);
    
    efree(values);
    values = NULL;

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf putmany: error %d",rv);
	}

	RETURN_LONG(make_php_rv(rv));
}	

PHP_METHOD(CitrusleafClient, add)
{
	zval *key_zval, *val_zval;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &key_zval, &val_zval) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf add requires two object\n");
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}	
	if (Z_TYPE_P(val_zval) != IS_LONG) {
		zend_error(E_NOTICE, "citrusleaf add: second parameter must be integer\n");
    	RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM); 
	}		

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key_zval, &o_key)) {
        RETURN_LONG(CITRUSLEAF_RESULT_UNKNOWN);
	}
	
	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf add: Null key");
		RETURN_LONG(CITRUSLEAF_RESULT_INVALID_API_PARAM);
	}

	cl_operation values[1];
	strcpy(values[0].bin.bin_name, clo->bin);
	if (0 != zval_to_citrusleaf_object(val_zval, &values[0].bin.object)) {
        RETURN_LONG(CITRUSLEAF_RESULT_UNKNOWN);
	}
	values[0].op = CL_OP_INCR;
	
	cl_write_parameters cl_wp;
	cl_write_parameters_set_default(&cl_wp);
	cl_wp.timeout_ms = clo->timeout;
	int replace = 0; //default;
	int generation;
    cl_wp.w_pol = clo->writepolicy;
	
	cl_rv rv = citrusleaf_operate(clo->asc, clo->namespace, clo->set, &o_key, values, 1, &cl_wp,replace,&generation);

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf add: error %d",rv);
	}
	
	RETURN_LONG(make_php_rv(rv));
}	

PHP_METHOD(CitrusleafClient, subtract)
{
	zval *key_zval, *val_zval;

//	fprintf(stderr, "subtract\n");
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &key_zval, &val_zval) == FAILURE) {
		zend_error(E_NOTICE, "citrusleaf subtract requires two object\n");
		RETURN_FALSE;
	}	
	if (Z_TYPE_P(val_zval) != IS_LONG) {
		zend_error(E_NOTICE, "citrusleaf subtract: second parameter must be integer\n");
		// dtor these vals?
		RETURN_FALSE;
	}		

//	fprintf(stderr, "subtract: val1 type %d val2 type %d\n",Z_TYPE_P(key_zval),Z_TYPE_P(val_zval));

	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	cl_object o_key;
	if (0 != zval_to_citrusleaf_object(key_zval, &o_key)) {
		RETURN_FALSE;
	}
	
	if (o_key.type == CL_NULL) {
		zend_error(E_NOTICE, "citrusleaf subtract: Null key");
		RETURN_FALSE;
	}

	cl_bin values[1];
	strcpy(values[0].bin_name, clo->bin);
	if (0 != zval_to_citrusleaf_object(val_zval, &values[0].object)) {
		RETURN_FALSE;
	}
	
	cl_write_parameters cl_wp;
	cl_write_parameters_set_default(&cl_wp);
	// cl_wp.timeout_ms = ???
	
	cl_rv rv = citrusleaf_put(clo->asc, clo->namespace, clo->set, &o_key, values, 1, &cl_wp);

	if (rv != CITRUSLEAF_OK) {
		zend_error(E_NOTICE, "citrusleaf subtract: error %d",rv);
	}

	RETURN_LONG(make_php_rv(rv));
	// it would seem I have to destruct these, but doing so gives a duplicate free?
	// zval_dtor(key_zval);
	// zval_dtor(val_zval);
	
	RETURN_TRUE;
}	


PHP_METHOD(CitrusleafClient, query)
{
//	fprintf(stderr, "add: val1 type %d val2 type %d\n",Z_TYPE_P(key_zval),Z_TYPE_P(val_zval));

    zval *arr, **data;
    HashTable *arr_hash;
    HashPosition pointer;
    int array_count;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arr) == FAILURE) {
		php_printf("<p> ERROR query [1]</p");
		zend_error(E_NOTICE, "<p>citrusleaf query requires an array passed in as parameter</p>");
        RETURN_FALSE;
    }

	if (Z_TYPE_P(arr) != IS_ARRAY) {
		zend_error(E_NOTICE, "<p> citrusleaf query: first parameter must be array</p>");
		RETURN_FALSE;
	}		

    arr_hash = Z_ARRVAL_P(arr);
    array_count = zend_hash_num_elements(arr_hash);

    php_printf("<p>The array passed contains %d elements</p> ", array_count);

	if (array_count != 14) {
		php_printf("<p> ERROR query [2]</p");
		zend_error(E_NOTICE, "<p>citrusleaf query requires an array of size 14 passed in as parameter</p>");
        RETURN_FALSE;
	}

	// php_printf("<p> query [3]</p");
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	sconfig c;
	memset(&c, 0, sizeof(c));

	c.asc = clo->asc;
	c.ns = clo->namespace;
	c.set = clo->set;
	c.verbose = false;

	int i = 0;
    for(zend_hash_internal_pointer_reset_ex(arr_hash, &pointer); zend_hash_get_current_data_ex(arr_hash, (void**) &data, &pointer) == SUCCESS; zend_hash_move_forward_ex(arr_hash, &pointer)) {

        if ((Z_TYPE_PP(data) != IS_STRING) && (i != 12)) {
			php_printf("<p> ERROR query [3] postion %d</p", i);
			zend_error(E_NOTICE, "<p>citrusleaf query expects string at position %d </p>", i);
        	RETURN_FALSE;
        }
		

		switch (i) {
			case 0: c.account_id_attr = Z_STRVAL_PP(data); break;
			case 1: c.account_id = Z_STRVAL_PP(data); break;
			case 2: 
        		if ((strcmp (Z_STRVAL_PP(data), "range_hour") != 0)) {
					php_printf("<p> ERROR query [4]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query expects groupby at position %d </p>", i);
        			RETURN_FALSE;
        		}
				break;
			case 3: c.date_hour_attr = Z_STRVAL_PP(data); break;
			case 4: c.begin_date = Z_STRVAL_PP(data); break;
			case 5: c.end_date = Z_STRVAL_PP(data); break;
			case 6: 
        		if ((strcmp (Z_STRVAL_PP(data), "sum") != 0)) {
					php_printf("<p> ERROR query [5]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query expects sum at position %d </p>", i);
        			RETURN_FALSE;
        		}
				break;
			case 7: c.count_attr = Z_STRVAL_PP(data); break;
			case 8: 
        		if ((strcmp (Z_STRVAL_PP(data), "count_distinct") != 0)) {
					php_printf("<p> ERROR query [6]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query expects count_distinct at position %d </p>", i);
        			RETURN_FALSE;
        		}
				break;
			case 9: c.distinct_attr = Z_STRVAL_PP(data); break;
			case 10: 
        		if ((strcmp (Z_STRVAL_PP(data), "groupby") != 0)) {
					php_printf("<p> ERROR query [7]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query expects groupby at position %d </p>", i);
        			RETURN_FALSE;
        		}
				break;
			case 11: c.group_attr = Z_STRVAL_PP(data); break;
			case 12: 
        		if (Z_TYPE_PP(data) != IS_BOOL) {
					php_printf("<p> ERROR query [8]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query expects boolean at position %d </p>", i);
        			RETURN_FALSE;
        		}
				c.index = Z_LVAL_PP(data) ? true : false;
				break;
			case 13: c.iset = Z_STRVAL_PP(data); break;
			default:
					php_printf("<p> ERROR query [9]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query should never reach position %d </p>", i);
        			RETURN_FALSE;
		}
		i++;
	}
	
	// php_printf("<p> query [5]</p");

	int rv = group_query(&c);
	
	// php_printf("<p> query [6]</p");
	// it would seem I have to destruct these, but doing so gives a duplicate free?
	// zval_dtor(key_zval);
	// zval_dtor(val_zval);

	if (rv == CITRUSLEAF_OK) {
		// php_printf("<p> query [7]</p");
		RETURN_TRUE;
	}
	else {
		php_printf("<p> ERROR query [10]</p");
		RETURN_FALSE;
	}
}	

PHP_METHOD(CitrusleafClient, put_index)
{
//	fprintf(stderr, "add: val1 type %d val2 type %d\n",Z_TYPE_P(key_zval),Z_TYPE_P(val_zval));

    zval *arr, **data;
    HashTable *arr_hash;
    HashPosition pointer;
    int array_count;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arr) == FAILURE) {
		php_printf("<p> ERROR query [1]</p");
		zend_error(E_NOTICE, "<p>citrusleaf put_index requires an array passed in as parameter</p>");
        RETURN_FALSE;
    }

	if (Z_TYPE_P(arr) != IS_ARRAY) {
		zend_error(E_NOTICE, "citrusleaf put_index: first parameter must be array\n");
		RETURN_FALSE;
	}		

    arr_hash = Z_ARRVAL_P(arr);
    array_count = zend_hash_num_elements(arr_hash);

    php_printf("<p>The array passed contains %d elements</p> ", array_count);

	if (array_count != 13) {
		php_printf("<p> ERROR query [2]</p");
		zend_error(E_NOTICE, "<p>citrusleaf query requires an array of size 13 passed in as parameter</p>");
        RETURN_FALSE;
	}

	// php_printf("<p> query [3]</p");
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	iconfig c;
	memset(&c, 0, sizeof(c));

	c.asc = clo->asc;
	c.ns = clo->namespace;
	c.set = clo->set;
	c.iset = clo->iset;
	c.index = true;
	c.verbose = false;
	c.timeout = 5000; // five second timeout for inserts. Need to configure somewhere else

	int i = 0;
    for(zend_hash_internal_pointer_reset_ex(arr_hash, &pointer); zend_hash_get_current_data_ex(arr_hash, (void**) &data, &pointer) == SUCCESS; zend_hash_move_forward_ex(arr_hash, &pointer)) {

        if ((Z_TYPE_PP(data) != IS_STRING) && (i != 12)) {
			php_printf("<p> ERROR query [3] postion %d</p", i);
			zend_error(E_NOTICE, "<p>citrusleaf query expects string at position %d </p>", i);
        	RETURN_FALSE;
        }
		

		switch (i) {
			case 0: c.account_id = Z_STRVAL_PP(data); break;
			case 1: c.domain_id = Z_STRVAL_PP(data); break;
			case 2: c.campaign_id = Z_STRVAL_PP(data); break;
			case 3: c.original_url_id = Z_STRVAL_PP(data); break;
			case 4: c.share_type = Z_STRVAL_PP(data); break;
			case 5: c.create_type = Z_STRVAL_PP(data); break;
			case 6: c.sharer_id = Z_STRVAL_PP(data); break;
			case 7: c.parent_id = Z_STRVAL_PP(data); break;
			case 8: c.user_id = Z_STRVAL_PP(data); break;
			case 9: c.notes = Z_STRVAL_PP(data); break;
			case 10: c.redirection_id = Z_STRVAL_PP(data); break;
			case 11: c.hour = Z_STRVAL_PP(data); break;
			case 12: c.clicks = Z_STRVAL_PP(data); break;
			default:
					php_printf("<p> ERROR query [9]</p");
					zend_error(E_NOTICE, "<p>citrusleaf query should never reach position %d </p>", i);
        			RETURN_FALSE;
		}
		i++;
	}
	
	// php_printf("<p> query [5]</p");

	int rv = put_index(&c);
	
	// php_printf("<p> query [6]</p");
	// it would seem I have to destruct these, but doing so gives a duplicate free?
	// zval_dtor(key_zval);
	// zval_dtor(val_zval);

	if (rv == CITRUSLEAF_OK) {
		// php_printf("<p> query [7]</p");
		RETURN_TRUE;
	}
	else {
		php_printf("<p> ERROR query [10]</p");
		RETURN_FALSE;
	}
}

PHP_METHOD(CitrusleafClient, update_clicks)
{

    zval *val_ival, *arr, **data;
    HashTable *arr_hash;
    HashPosition pointer;
    int array_count;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "az", &arr, &val_ival) == FAILURE) {
		php_printf("<p> ERROR query [1]</p");
		zend_error(E_NOTICE, "<p>citrusleaf query requires an array passed in as parameter</p>");
        RETURN_FALSE;
    }

	if (Z_TYPE_P(arr) != IS_ARRAY) {
		zend_error(E_NOTICE, "citrusleaf update_clicks: first parameter must be array\n");
		RETURN_FALSE;
	}		

	if (Z_TYPE_P(val_ival) != IS_LONG) {
		zend_error(E_NOTICE, "citrusleaf update_clicks: second parameter must be integer\n");
		RETURN_FALSE;
	}		
    arr_hash = Z_ARRVAL_P(arr);
    array_count = zend_hash_num_elements(arr_hash);

    php_printf("<p>The array passed contains %d elements</p> ", array_count);

	if (array_count != 3) {
		php_printf("<p> ERROR query [2]</p");
		zend_error(E_NOTICE, "<p>citrusleaf update_clicks requires an primary key with 3 fields passed in as parameters</p>");
        RETURN_FALSE;
	}

	// php_printf("<p> query [3]</p");
	citrusleaf_object *clo = (citrusleaf_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	uconfig c;
	memset(&c, 0, sizeof(c));

	c.asc = clo->asc;
	c.ns = clo->namespace;
	c.set = clo->set;
	c.verbose = false;
	c.timeout = 5000; // five second timeout for inserts. Need to configure somewhere else
	c.clicks = Z_LVAL_P(val_ival);
	c.clicks_attr = clo->bin;

	int i = 0;
    for(zend_hash_internal_pointer_reset_ex(arr_hash, &pointer); zend_hash_get_current_data_ex(arr_hash, (void**) &data, &pointer) == SUCCESS; zend_hash_move_forward_ex(arr_hash, &pointer)) {

        if (Z_TYPE_PP(data) != IS_STRING) {
			php_printf("<p> ERROR update_clicks [3] postion %d</p", i);
			zend_error(E_NOTICE, "<p>citrusleaf update_clicks expects string at position %d </p>", i);
        	RETURN_FALSE;
        }
		

		switch (i) {
			case 0: c.account_id = Z_STRVAL_PP(data); break;
			case 1: c.hour = Z_STRVAL_PP(data); break;
			case 2: c.redirection_id = Z_STRVAL_PP(data); break;
			default:
					php_printf("<p> ERROR query [9]</p");
					zend_error(E_NOTICE, "<p>citrusleaf update_clicks should never reach position %d </p>", i);
        			RETURN_FALSE;
		}
		i++;
	}
	
	// php_printf("<p> query [5]</p");

	int rv = update_clicks(&c);
	
	// php_printf("<p> query [6]</p");
	// it would seem I have to destruct these, but doing so gives a duplicate free?
	// zval_dtor(key_zval);
	// zval_dtor(val_zval);

	if (rv == CITRUSLEAF_OK) {
		// php_printf("<p> query [7]</p");
		RETURN_TRUE;
	}
	else {
		php_printf("<p> ERROR query [10]</p");
		RETURN_FALSE;
	}
}	

PHP_METHOD( CitrusleafClient, __construct )
{
#ifdef LOGDEBUG
	cf_set_log_level(CF_DEBUG);
	char *default_logfile = "/tmp/citrusleaf_phpclient.log";
	int rc_errno = do_set_log_file(default_logfile);
	if (0 == rc_errno) {
		fprintf(stderr, "Default log file: %s\n", default_logfile);
	} else {
		fprintf(stderr, "Error opening default log file: %s  errno=%d\n", default_logfile, rc_errno);
	}
#endif

	return_value = getThis();
}

	ZEND_BEGIN_ARG_INFO_EX(php_citrusleaf_two_arg, 0, 0, 2)
	ZEND_END_ARG_INFO()

	ZEND_BEGIN_ARG_INFO_EX(php_citrusleaf_one_arg, 0, 0, 1)
	ZEND_END_ARG_INFO()

static zend_function_entry php_citrusleaf_client_functions[] = {
	PHP_ME(CitrusleafClient, __construct, NULL /*arginfo*/, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(CitrusleafClient, helloWorld, NULL/*arginfo */, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_log_level, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_log_file, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, connect, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, close, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, use_shm, php_citrusleaf_two_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, free_shm, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_default_namespace, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, get_default_namespace, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_default_bin, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, get_default_bin, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_default_set, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, get_default_set, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_default_timeout, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, get_default_timeout, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_default_writepolicy, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, get_default_writepolicy, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, set_default_iset, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, exists, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, get, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, put, php_citrusleaf_two_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, putmany, php_citrusleaf_two_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, add, php_citrusleaf_two_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, subtract, php_citrusleaf_two_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, delete, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, query, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, info, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, put_index, php_citrusleaf_one_arg, ZEND_ACC_PUBLIC)
	PHP_ME(CitrusleafClient, update_clicks, php_citrusleaf_two_arg, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

/**** Register CitrusleafClient & CitrusleafResponse classes *****/
PHP_MINIT_FUNCTION(citrusleaf)
{
    /* Register CitrusleafClient class */
	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, PHP_CITRUSLEAF_CLIENT_NAME, php_citrusleaf_client_functions);
	ce.create_object = citrusleaf_object_create;
	php_citrusleaf_client_entry = zend_register_internal_class(&ce TSRMLS_CC);
	
	if (!php_citrusleaf_client_entry) {
		zend_error(E_ERROR, "Failed to register citrusleaf class");
		return FAILURE;
	}	
	memcpy(&citrusleaf_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	

	/* Register the CitrusleafResponse class */
	INIT_CLASS_ENTRY(ce, PHP_CITRUSLEAF_RESPONSE_NAME, php_citrusleaf_response_functions);
	ce.create_object = citrusleaf_response_create;
	php_citrusleaf_response_entry = zend_register_internal_class(&ce TSRMLS_CC);
	
	if (!php_citrusleaf_response_entry) {
		zend_error(E_ERROR, "Failed to register response class");
		return FAILURE;
	}	
	memcpy(&citrusleaf_response_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));


	/* init the process level globals in the citrusleaf code */
	citrusleaf_init();
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(citrusleaf)
{
	citrusleaf_shutdown();
	return SUCCESS;
}

	
	
// For our own sanity we need to have some kind of framework in place to know the 
// number of people we've been talking to, who is chasing who, etc etc, as we've discussed. 

static zend_function_entry citrusleaf_functions[] = {
    PHP_FE(citrusleaf, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry citrusleaf_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_CITRUSLEAF_EXTNAME,
    citrusleaf_functions, /* functions */
    PHP_MINIT(citrusleaf), /* minit */
    PHP_MSHUTDOWN(citrusleaf), /* mshutdown */
    NULL, /* rinit */
    NULL, /* rshutdown */
    NULL, /* minfo */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_CITRUSLEAF_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_CITRUSLEAF
ZEND_GET_MODULE(citrusleaf)
#endif


