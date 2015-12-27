/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2008 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Jim Winstead <jimw@php.net>                                  |
   +----------------------------------------------------------------------+
 */

/* $Id: dbase.c,v 1.91 2008/01/24 10:27:59 pollita Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "fopen_wrappers.h"
#include "php_globals.h"

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if DBASE
#include "php_dbase.h"
#include "dbf.h"
#if defined(THREAD_SAFE)
DWORD DbaseTls;
static int numthreads=0;
void *dbase_mutex;

typedef struct dbase_global_struct{
	int le_dbhead;
}dbase_global_struct;

#define DBase_GLOBAL(a) dbase_globals->a

#define DBase_TLS_VARS \
	dbase_global_struct *dbase_globals; \
	dbase_globals=TlsGetValue(DbaseTls); 

#else
static int le_dbhead;
#define DBase_GLOBAL(a) a
#define DBase_TLS_VARS
#endif

#include <fcntl.h>
#include <errno.h>


static void _close_dbase(zend_resource *rsrc TSRMLS_DC)
{
	dbhead_t *dbhead = (dbhead_t *)rsrc->ptr;

	close(dbhead->db_fd);
	free_dbf_head(dbhead);
}


PHP_MINIT_FUNCTION(dbase)
{
#if defined(THREAD_SAFE)
	dbase_global_struct *dbase_globals;
#ifdef COMPILE_DL_DBASE
	CREATE_MUTEX(dbase_mutex, "DBase_TLS");
	SET_MUTEX(dbase_mutex);
	numthreads++;
	if (numthreads==1){
	if ((DbaseTls=TlsAlloc())==0xFFFFFFFF){
		FREE_MUTEX(dbase_mutex);
		return 0;
	}}
	FREE_MUTEX(dbase_mutex);
#endif
	dbase_globals = (dbase_global_struct *) LocalAlloc(LPTR, sizeof(dbase_global_struct)); 
	TlsSetValue(DbaseTls, (void *) dbase_globals);
#endif
	DBase_GLOBAL(le_dbhead) =
		zend_register_list_destructors_ex(_close_dbase, NULL, "dbase", module_number);
	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(dbase)
{
#if defined(THREAD_SAFE)
	dbase_global_struct *dbase_globals;
	dbase_globals = TlsGetValue(DbaseTls); 
	if (dbase_globals != 0) 
		LocalFree((HLOCAL) dbase_globals); 
#ifdef COMPILE_DL_DBASE
	SET_MUTEX(dbase_mutex);
	numthreads--;
	if (!numthreads){
	if (!TlsFree(DbaseTls)){
		FREE_MUTEX(dbase_mutex);
		return 0;
	}}
	FREE_MUTEX(dbase_mutex);
#endif
#endif
	return SUCCESS;
}

/* {{{ proto int dbase_open(string name, int mode)
   Opens a dBase-format database file */
PHP_FUNCTION(dbase_open)
{
	zval *dbf_name, *options;
	dbhead_t *dbh;
	zval *handle;
	DBase_TLS_VARS;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &dbf_name, &options) == FAILURE) {	
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(dbf_name);
	convert_to_long_ex(options);

	if (!Z_STRLEN_P(dbf_name)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The filename cannot be empty.");
		RETURN_FALSE;
	}

	if (Z_LVAL_P(options) == 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot open %s in write-only mode", Z_STRVAL_P(dbf_name));
		RETURN_FALSE;
	} else if (Z_LVAL_P(options) < 0 || Z_LVAL_P(options) > 3) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid access mode %ld", Z_LVAL_P(options));
		RETURN_FALSE;
	}

	if (php_check_open_basedir(Z_STRVAL_P(dbf_name) TSRMLS_CC)) {
		RETURN_FALSE;
	}

	dbh = dbf_open(Z_STRVAL_P(dbf_name), Z_LVAL_P(options) TSRMLS_CC);
	if (dbh == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to open database %s", Z_STRVAL_P(dbf_name));
		RETURN_FALSE;
	}

	handle = zend_list_insert(dbh, DBase_GLOBAL(le_dbhead) ZEND_LIST_CC);
	RETURN_LONG(Z_RES_HANDLE_P(handle));
}
/* }}} */

/* {{{ proto bool dbase_close(int identifier)
   Closes an open dBase-format database file */
PHP_FUNCTION(dbase_close)
{
	zend_long dbh_id;
	zval *dbh;
	int dbh_type;
	DBase_TLS_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &dbh_id) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}
	
	zend_list_delete(Z_RES_P(dbh));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int dbase_numrecords(int identifier)
   Returns the number of records in the database */
PHP_FUNCTION(dbase_numrecords)
{
	zend_long dbh_id;
	zval *dbh;
	int dbh_type;
	DBase_TLS_VARS;
	dbhead_t *dbht;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &dbh_id) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}

	dbht = Z_RES_P(dbh)->ptr;
	RETURN_LONG(dbht->db_records);

}
/* }}} */

/* {{{ proto int dbase_numfields(int identifier)
   Returns the number of fields (columns) in the database */
PHP_FUNCTION(dbase_numfields)
{
	zend_long dbh_id;
	zval *dbh;
	int dbh_type;
	DBase_TLS_VARS;
	dbhead_t *dbht;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &dbh_id) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}

	dbht = Z_RES_P(dbh)->ptr;
	RETURN_LONG(dbht->db_nfields);
}
/* }}} */

/* {{{ proto bool dbase_pack(int identifier)
   Packs the database (deletes records marked for deletion) */
PHP_FUNCTION(dbase_pack)
{
	zend_long dbh_id;
	zval *dbh;
	int dbh_type;
	DBase_TLS_VARS;
	dbhead_t *dbht;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &dbh_id) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}

	dbht = Z_RES_P(dbh)->ptr;
	pack_dbf(dbht);
	put_dbf_info(dbht);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool dbase_add_record(int identifier, array data)
   Adds a record to the database */
PHP_FUNCTION(dbase_add_record)
{
	zval *fields, *field, *dbh;
	zend_long dbh_id;
	int dbh_type;
	dbhead_t *dbht;

	int num_fields;
	dbfield_t *dbf, *cur_f;
	char *cp, *t_cp;
	int i;
	DBase_TLS_VARS;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &dbh_id, &fields) == FAILURE) {	
		WRONG_PARAM_COUNT;
	}
	

	if (Z_TYPE_P(fields) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument two must be of type 'Array'");
		RETURN_FALSE;
	}

	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}
	dbht = Z_RES_P(dbh)->ptr;

	num_fields = zend_hash_num_elements(Z_ARRVAL_P(fields));

	if (num_fields != dbht->db_nfields) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong number of fields specified");
		RETURN_FALSE;
	}

	cp = t_cp = (char *)emalloc(dbht->db_rlen + 1);
	*t_cp++ = VALID_RECORD;

	dbf = dbht->db_fields;
	for (i = 0, cur_f = dbf; cur_f < &dbf[num_fields]; i++, cur_f++) {
		zval tmp;
		if ((field = zend_hash_index_find(Z_ARRVAL_P(fields), i)) == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unexpected error");
			efree(cp);
			RETURN_FALSE;
		}

		tmp = *field;
		zval_copy_ctor(&tmp);
		convert_to_string(&tmp);
		snprintf(t_cp, cur_f->db_flen+1, cur_f->db_format, Z_STRVAL(tmp));
		zval_dtor(&tmp); 
		t_cp += cur_f->db_flen;
	}

	dbht->db_records++;
	if (put_dbf_record(dbht, dbht->db_records, cp) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to put record at %ld", dbht->db_records);
		efree(cp);
		RETURN_FALSE;
	}

	put_dbf_info(dbht);
	efree(cp);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool dbase_replace_record(int identifier, array data, int recnum)
   Replaces a record to the database */
PHP_FUNCTION(dbase_replace_record)
{
	zval *fields, *field, *recnum, *dbh;
	int dbh_type;
	zend_long dbh_id;

	int num_fields;
	dbfield_t *dbf, *cur_f;
	dbhead_t *dbht;
	char *cp, *t_cp;
	int i;
	DBase_TLS_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lzz", &dbh_id, &fields, &recnum) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(recnum);

	if (Z_TYPE_P(fields) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument two must be of type 'Array'");
		RETURN_FALSE;
	}

	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}
	dbht = Z_RES_P(dbh)->ptr;

	num_fields = zend_hash_num_elements(Z_ARRVAL_P(fields));

	if (num_fields != dbht->db_nfields) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong number of fields specified");
		RETURN_FALSE;
	}

	cp = t_cp = (char *)emalloc(dbht->db_rlen + 1);
	*t_cp++ = VALID_RECORD;

	dbf = dbht->db_fields;
	for (i = 0, cur_f = dbf; cur_f < &dbf[num_fields]; i++, cur_f++) {
		if ((field = zend_hash_index_find(Z_ARRVAL_P(fields), i)) == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unexpected error");
			efree(cp);
			RETURN_FALSE;
		}
		convert_to_string_ex(field);
		snprintf(t_cp, cur_f->db_flen+1, cur_f->db_format, Z_STRVAL_P(field)); 
		t_cp += cur_f->db_flen;
	}

	if (put_dbf_record(dbht, Z_LVAL_P(recnum), cp) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to put record at %ld", dbht->db_records);
		efree(cp);
		RETURN_FALSE;
	}

    put_dbf_info(dbht);
	efree(cp);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool dbase_delete_record(int identifier, int record)
   Marks a record to be deleted */
PHP_FUNCTION(dbase_delete_record)
{
	zval *record, *dbh;
	zend_long dbh_id;
	dbhead_t *dbht;
	int dbh_type;
	DBase_TLS_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &dbh_id, &record) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(record);

	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}
	dbht = Z_RES_P(dbh)->ptr;

	if (del_dbf_record(dbht, Z_LVAL_P(record)) < 0) {
		if (Z_LVAL_P(record) > dbht->db_records) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "record %ld out of bounds", Z_LVAL_P(record));
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to delete record %ld", Z_LVAL_P(record));
		}
		RETURN_FALSE;
	}

        put_dbf_info(dbht);
	RETURN_TRUE;
}
/* }}} */

/* {{{ php_dbase_get_record
 */  
static void php_dbase_get_record(INTERNAL_FUNCTION_PARAMETERS, int assoc)
{
	zval *record, *dbh;
	dbhead_t *dbht;
	zend_long dbh_id;
	int dbh_type;
	dbfield_t *dbf, *cur_f;
	char *data, *fnp, *str_value;
	size_t cursize = 0;
	long overflow_test;
	int errno_save;
	DBase_TLS_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &dbh_id, &record) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(record);

	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}
	dbht = Z_RES_P(dbh)->ptr;

	if ((data = get_dbf_record(dbht, Z_LVAL_P(record))) == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Tried to read bad record %ld", Z_LVAL_P(record));
		RETURN_FALSE;
	}

	dbf = dbht->db_fields;

	array_init(return_value);

	fnp = NULL;
	for (cur_f = dbf; cur_f < &dbf[dbht->db_nfields]; cur_f++) {
		/* get the value */
		str_value = (char *)emalloc(cur_f->db_flen + 1);

		if(cursize <= (unsigned)cur_f->db_flen) {
			cursize = cur_f->db_flen + 1;
			fnp = erealloc(fnp, cursize);
		}
		snprintf(str_value, cursize, cur_f->db_format, get_field_val(data, cur_f, fnp));

		/* now convert it to the right php internal type */
		switch (cur_f->db_type) {
			case 'C':
			case 'D':
				if (!assoc) {
					add_next_index_string(return_value, str_value);
				} else {
					add_assoc_string(return_value, cur_f->db_fname, str_value);
				}
				break;
			case 'I':	/* FALLS THROUGH */
			case 'N':
				if (cur_f->db_fdc == 0) {
					/* Large integers in dbase can be larger than long */
					errno_save = errno;
					overflow_test = strtol(str_value, NULL, 10);
					if (errno == ERANGE) {
					    /* If the integer is too large, keep it as string */
						if (!assoc) {
						    add_next_index_string(return_value, str_value);
						} else {
						    add_assoc_string(return_value, cur_f->db_fname, str_value);
						}
					} else {
						if (!assoc) {
						    add_next_index_long(return_value, overflow_test);
						} else {
						    add_assoc_long(return_value, cur_f->db_fname, overflow_test);
						}
					}
					errno = errno_save;
				} else {
					if (!assoc) {
						add_next_index_double(return_value, atof(str_value));
					} else {
						add_assoc_double(return_value, cur_f->db_fname, atof(str_value));
					}
				}
				break;
			case 'F':
				if (!assoc) {
					add_next_index_double(return_value, atof(str_value));
				} else {
					add_assoc_double(return_value, cur_f->db_fname, atof(str_value));
				}
				break;
			case 'L':	/* we used to FALL THROUGH, but now we check for T/Y and F/N
						   and insert 1 or 0, respectively.  db_fdc is the number of
						   decimals, which we don't care about.      3/14/2001 LEW */
				if ((*str_value == 'T') || (*str_value == 'Y')) {
					if (!assoc) {
						add_next_index_long(return_value, strtol("1", NULL, 10));
					} else {
						add_assoc_long(return_value, cur_f->db_fname,strtol("1", NULL, 10));
					}
				} else {
					if ((*str_value == 'F') || (*str_value == 'N')) {
						if (!assoc) {
							add_next_index_long(return_value, strtol("0", NULL, 10));
						} else {
							add_assoc_long(return_value, cur_f->db_fname,strtol("0", NULL, 10));
						}
					} else {
						if (!assoc) {
							add_next_index_long(return_value, strtol(" ", NULL, 10));
						} else {
							add_assoc_long(return_value, cur_f->db_fname,strtol(" ", NULL, 10));
						}
					}
				}
				break;
			case 'M':
				/* this is a memo field. don't know how to deal with this yet */
				break;
			default:
				/* should deal with this in some way */
				break;
		}
		efree(str_value);
	}

	efree(fnp);

	/* mark whether this record was deleted */
	if (data[0] == '*') {
		add_assoc_long(return_value, "deleted", 1);
	} else {
		add_assoc_long(return_value, "deleted", 0);
	}

	free(data);
}
/* }}} */
 
/* {{{ proto array dbase_get_record(int identifier, int record)
   Returns an array representing a record from the database */
PHP_FUNCTION(dbase_get_record)
{
	php_dbase_get_record(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* From Martin Kuba <makub@aida.inet.cz> */
/* {{{ proto array dbase_get_record_with_names(int identifier, int record)
   Returns an associative array representing a record from the database */
PHP_FUNCTION(dbase_get_record_with_names)
{
	php_dbase_get_record(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto bool dbase_create(string filename, array fields)
   Creates a new dBase-format database file */
PHP_FUNCTION(dbase_create)
{
	zval *filename, *fields, *field, *value, *handle;
	int fd;
	dbhead_t *dbh;

	int num_fields;
	dbfield_t *dbf, *cur_f;
	int i, rlen;
	DBase_TLS_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &filename, &fields) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(filename);

	if (Z_TYPE_P(fields) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expected array as second parameter");
		RETURN_FALSE;
	}

	if (php_check_open_basedir(Z_STRVAL_P(filename) TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if ((fd = VCWD_OPEN_MODE(Z_STRVAL_P(filename), O_BINARY|O_RDWR|O_CREAT, 0644)) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to create database (%d): %s", errno, strerror(errno));
		RETURN_FALSE;
	}

	num_fields = zend_hash_num_elements(Z_ARRVAL_P(fields));

	if (num_fields <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to create database without fields");
		RETURN_FALSE;
	}

	/* have to use regular malloc() because this gets free()d by
	   code in the dbase library */
	dbh = (dbhead_t *)malloc(sizeof(dbhead_t));
	dbf = (dbfield_t *)malloc(sizeof(dbfield_t) * num_fields);
	if (!dbh || !dbf) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to allocate memory for header info");
		RETURN_FALSE;
	}
	
	/* initialize the header structure */
	dbh->db_fields = dbf;
	dbh->db_fd = fd;
	dbh->db_dbt = DBH_TYPE_NORMAL;
	strcpy(dbh->db_date, "19930818");
	dbh->db_records = 0;
	dbh->db_nfields = num_fields;
	dbh->db_hlen = sizeof(struct dbf_dhead) + 1 + num_fields * sizeof(struct dbf_dfield);

	rlen = 1;
	/**
	 * Patch by greg@darkphoton.com
	 **/
	/* make sure that the db_format entries for all fields are set to NULL to ensure we
       don't seg fault if there's an error and we need to call free_dbf_head() before all
       fields have been defined. */
	for (i = 0, cur_f = dbf; i < num_fields; i++, cur_f++) {
		cur_f->db_format = NULL;
	}
	/**
	 * end patch
	 */


	for (i = 0, cur_f = dbf; i < num_fields; i++, cur_f++) {
		/* look up the first field */
		if ((field = zend_hash_index_find(Z_ARRVAL_P(fields), i)) == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to find field %d", i);
			free_dbf_head(dbh);
			RETURN_FALSE;
		}

		/* field name */
		
		if (Z_TYPE_P(field) != IS_ARRAY || (value = zend_hash_index_find(Z_ARRVAL_P(field), 0)) == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "expected field name as first element of list in field %d", i);
			free_dbf_head(dbh);
			RETURN_FALSE;
		}
		convert_to_string_ex(value);
		if (Z_STRLEN_P(value) > 10 || Z_STRLEN_P(value) == 0) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid field name '%s' (must be non-empty and less than or equal to 10 characters)", Z_STRVAL_P(value));
			free_dbf_head(dbh);
			RETURN_FALSE;
		}
		copy_crimp(cur_f->db_fname, Z_STRVAL_P(value), Z_STRLEN_P(value));

		/* field type */
		if ((value = zend_hash_index_find(Z_ARRVAL_P(field), 1)) == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "expected field type as second element of list in field %d", i);
			RETURN_FALSE;
		}
		convert_to_string_ex(value);
		cur_f->db_type = toupper(*Z_STRVAL_P(value));

		cur_f->db_fdc = 0;

		/* verify the field length */
		switch (cur_f->db_type) {
		case 'L':
			cur_f->db_flen = 1;
			break;
		case 'M':
			cur_f->db_flen = 10;
			dbh->db_dbt = DBH_TYPE_MEMO;
			/* should create the memo file here, probably */
			break;
		case 'D':
			cur_f->db_flen = 8;
			break;
		case 'F':
			cur_f->db_flen = 20;
			break;
		case 'N':
		case 'C':
			/* field length */
			if ((value = zend_hash_index_find(Z_ARRVAL_P(field), 2)) == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "expected field length as third element of list in field %d", i);
				free_dbf_head(dbh);
				RETURN_FALSE;
			}
			convert_to_long_ex(value);
			cur_f->db_flen = Z_LVAL_P(value);

			if (cur_f->db_type == 'N') {
				if ((value = zend_hash_index_find(Z_ARRVAL_P(field), 3)) == NULL) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "expected field precision as fourth element of list in field %d", i);
					free_dbf_head(dbh);
					RETURN_FALSE;
				}
				convert_to_long_ex(value);
				cur_f->db_fdc = Z_LVAL_P(value);
			}
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unknown field type '%c'", cur_f->db_type);
			free_dbf_head(dbh);
			RETURN_FALSE;
		}
		cur_f->db_foffset = rlen;
		rlen += cur_f->db_flen;
	
		cur_f->db_format = get_dbf_f_fmt(cur_f);
	}

	dbh->db_rlen = rlen;
	put_dbf_info(dbh);

	handle = zend_list_insert(dbh, DBase_GLOBAL(le_dbhead) ZEND_LIST_CC);
	RETURN_LONG(Z_RES_HANDLE_P(handle));
}
/* }}} */

/* {{{ arginfo compatibility */
#if (PHP_MAJOR_VERSION >= 6 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3))
# define PHP_DBASE_ARGINFO
#else
# define PHP_DBASE_ARGINFO static
#endif

/* {{{ arginfo */
PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_open, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_close, 0)
	ZEND_ARG_INFO(0, identifier)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_numrecords, 0)
	ZEND_ARG_INFO(0, identifier)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_numfields, 0)
	ZEND_ARG_INFO(0, identifier)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_pack, 0)
	ZEND_ARG_INFO(0, identifier)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_add_record, 0)
	ZEND_ARG_INFO(0, identifier)
	ZEND_ARG_ARRAY_INFO(0, data, 0)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_replace_record, 0)
	ZEND_ARG_INFO(0, identifier)
	ZEND_ARG_ARRAY_INFO(0, data, 0)
	ZEND_ARG_INFO(0, recnum)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_delete_record, 0)
	ZEND_ARG_INFO(0, identifier)
	ZEND_ARG_INFO(0, record)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_get_record, 0)
	ZEND_ARG_INFO(0, identifier)
	ZEND_ARG_INFO(0, record)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_get_record_with_names, 0)
	ZEND_ARG_INFO(0, identifier)
	ZEND_ARG_INFO(0, record)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_create, 0)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_ARRAY_INFO(0, fields, 0)
ZEND_END_ARG_INFO()

PHP_DBASE_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_dbase_get_header_info, 0)
	ZEND_ARG_INFO(0, database_handle)
ZEND_END_ARG_INFO()

/* }}} */

/* {{{ dbase_functions[]
 */
const zend_function_entry dbase_functions[] = {
	PHP_FE(dbase_open,								arginfo_dbase_open)
	PHP_FE(dbase_create,							arginfo_dbase_create)
	PHP_FE(dbase_close,								arginfo_dbase_close)
	PHP_FE(dbase_numrecords,						arginfo_dbase_numrecords)
	PHP_FE(dbase_numfields,							arginfo_dbase_numfields)
	PHP_FE(dbase_add_record,						arginfo_dbase_add_record)
	PHP_FE(dbase_replace_record,					arginfo_dbase_replace_record)
	PHP_FE(dbase_get_record,						arginfo_dbase_get_record)
	PHP_FE(dbase_get_record_with_names,				arginfo_dbase_get_record_with_names)
	PHP_FE(dbase_delete_record,						arginfo_dbase_delete_record)
	PHP_FE(dbase_pack,								arginfo_dbase_pack)
	PHP_FE(dbase_get_header_info,					arginfo_dbase_get_header_info)
	{NULL, NULL, NULL}
};
/* }}} */

/* Added by Zak Greant <zak@php.net> */
/* {{{ proto array dbase_get_header_info(int database_handle)
 */
PHP_FUNCTION(dbase_get_header_info)
{
	zval		row, *dbh;
	dbhead_t 	*dbht;
	zend_long 	dbh_id;
	dbfield_t	*dbf, *cur_f;
	int 		dbh_type;
	DBase_TLS_VARS;	

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &dbh_id) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	dbh = zend_hash_index_find(&EG(regular_list), dbh_id);
	if (dbh == NULL || Z_RES_P(dbh)->type != DBase_GLOBAL(le_dbhead)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find database for identifier %ld", dbh_id);
		RETURN_FALSE;
	}
	dbht = Z_RES_P(dbh)->ptr;

	array_init(return_value);

	dbf = dbht->db_fields;
	for (cur_f = dbf; cur_f < &dbht->db_fields[dbht->db_nfields]; ++cur_f) {
		
		array_init(&row);
		
		add_next_index_zval(return_value, &row);
		
		/* field name */
		add_assoc_string(&row, "name", cur_f->db_fname);
		
		/* field type */
		switch (cur_f->db_type) {
			case 'C': add_assoc_string(&row, "type", "character");	break;
			case 'D': add_assoc_string(&row, "type", "date"); 		break;
			case 'I': add_assoc_string(&row, "type", "integer"); 		break;
			case 'N': add_assoc_string(&row, "type", "number"); 		break;
			case 'L': add_assoc_string(&row, "type", "boolean");		break;
			case 'M': add_assoc_string(&row, "type", "memo");			break;
			case 'F': add_assoc_string(&row, "type", "float");     break;
			default:  add_assoc_string(&row, "type", "unknown");		break;
		}
		
		/* length of field */
		add_assoc_long(&row, "length", cur_f->db_flen);
		
		/* number of decimals in field */
		switch (cur_f->db_type) {
			case 'N':
			case 'I':
				add_assoc_long(&row, "precision", cur_f->db_fdc);
				break;
			default:
				add_assoc_long(&row, "precision", 0);
		}

		/* format for printing %s etc */
		add_assoc_string(&row, "format", cur_f->db_format);
		
		/* offset within record */
		add_assoc_long(&row, "offset", cur_f->db_foffset);
	}
}
/* }}} */

zend_module_entry dbase_module_entry = {
	STANDARD_MODULE_HEADER,
	"dbase", 
	dbase_functions, 
	PHP_MINIT(dbase), 
	PHP_MSHUTDOWN(dbase), 
	NULL, NULL, NULL, 
	PHP_DBASE_VERSION, 
	STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_DBASE
ZEND_GET_MODULE(dbase)

#if defined(PHP_WIN32) && defined(THREAD_SAFE)

/*NOTE: You should have an odbc.def file where you
export DllMain*/
BOOL WINAPI DllMain(HANDLE hModule, 
                      DWORD  ul_reason_for_call, 
                      LPVOID lpReserved)
{
    return 1;
}
#endif
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
