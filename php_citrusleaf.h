#ifndef PHP_CITRUSLEAF_H
#define PHP_CITRUSLEAF_H 1

#include <inttypes.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <citrusleaf/citrusleaf.h>

#define PHP_CITRUSLEAF_VERSION "1.0"
#define PHP_CITRUSLEAF_EXTVER "1.0"
#define PHP_CITRUSLEAF_EXTNAME "citrusleaf"

#define PHP_CITRUSLEAF_DESCRIPTOR_RES_NAME "File Descriptor"

// this one seemed to work
PHP_FUNCTION(citrusleaf);
// this one didn't
// PHP_MINIT(citrusleaf);

extern zend_module_entry citrusleaf_entry;
#define phpext_citrusleaf_ptr &citrusleaf_module_entry

// Move these to a CF file at some point if they work
extern bool base64_validate_input(const uint8_t *b, const int len);
extern int base64_encode_maxlen(int len);
extern void base64_encode(uint8_t * in_bytes, uint8_t *out_bytes, int *len_r);
extern void base64_tostring(uint8_t * in_bytes, char *out_bytes, int *len_r);
extern int base64_decode_inplace(uint8_t *bytes, int *len_r, bool validate);
extern int base64_decode(uint8_t *in_bytes, uint8_t *out_bytes, int *len_r, bool validate);

extern void make_date_hour(char *c_date_hour_s, int year, int month, int day, int hour);
extern void str_split(char split_c, char *str, cf_vector *v);

typedef struct index_record_s {
	uint64_t count;
	cf_digest redirect_id[];
} __attribute__ ((__packed__)) index_record;

typedef struct sconfig_s {
	
	char *ns;
	char *set;
	char *iset;
	
	bool verbose;

	bool index;
	char *account_id;
	char *begin_date;
	char *end_date;
	char *group_attr;
	char *distinct_attr;
	char *count_attr;
	char *date_hour_attr;
	char *account_id_attr;

	uint32_t begin_date_hour;
	uint32_t end_date_hour;
	
	cf_atomic_int records_counter;
	// uint32_t bytes_counter;
	uint64_t elapsed_time;
	
	cl_cluster	*asc;
	
    index_record **irs;
	cf_atomic_int irs_curr;
	int64_t irs_size;

	cf_digest *digests;
	int64_t n_digests;

	shash *group_hash;
	
} sconfig;

typedef struct iconfig_s {
	char *ns;
	char *set;
	char *iset;
	
	bool index;
	bool verbose;
	int timeout;

	cl_cluster	*asc;

	char *account_id;
	char *domain_id;
	char *campaign_id;
	char *original_url_id;
	char *share_type;
	char *create_type;
	char *sharer_id;
	char *parent_id;
	char *user_id;
	char *notes;
	char *redirection_id;
	char *hour;
	char *clicks;
} iconfig;

typedef struct uconfig_s {
	char *ns;
	char *set;
	
	bool verbose;
	int timeout;

	cl_cluster	*asc;

	char *account_id;
	char *hour;
	char *redirection_id;
	int64_t clicks;
	char *clicks_attr;
} uconfig;

extern int update_clicks(uconfig *c);
extern int put_index(iconfig *c);
extern int group_query(sconfig *c);

// error codes that is returned back to the php interface
// these MUST match up to citrusleaf_enums.php
// and should also match up with csharp interface (or any other client interface)
// Negative error codes-- failure at the client side
typedef enum {
		CITRUSLEAF_RESULT_NO_HOSTS=-5,
        CITRUSLEAF_RESULT_INVALID_API_PARAM=-4,
		CITRUSLEAF_RESULT_FAIL_ASYNCQ_FULL = -3,
	    CITRUSLEAF_RESULT_TIMEOUT = -2,
    	CITRUSLEAF_RESULT_FAIL_CLIENT = -1,
        CITRUSLEAF_RESULT_OK=0, 
        CITRUSLEAF_RESULT_UNKNOWN=1,
        CITRUSLEAF_RESULT_KEY_NOT_FOUND_ERROR=2, 
        CITRUSLEAF_RESULT_GENERATION_ERROR=3, 
        CITRUSLEAF_RESULT_PARAMETER_ERROR=4,
        CITRUSLEAF_RESULT_KEY_FOUND_ERROR=5, 
        CITRUSLEAF_RESULT_BIN_FOUND_ERROR=6,
		CITRUSLEAF_RESULT_CLUSTER_KEY_MISMATCH=7,
		CITRUSLEAF_RESULT_PARTITION_OUT_OF_SPACE=8,
		CITRUSLEAF_RESULT_SERVERSIDE_TIMEOUT=9,
		CITRUSLEAF_RESULT_NO_XDS=10,
		CITRUSLEAF_RESULT_SERVER_UNAVAILABLE=11,
		CITRUSLEAF_RESULT_INCOMPATIBLE_TYPE=12,
		CITRUSLEAF_RESULT_RECORD_TOO_BIG=13,
		CITRUSLEAF_RESULT_KEY_BUSY=14,
} cl_result_codes_enum;

typedef enum {
    CITRUSLEAF_WRITE_UNIQUE_NONE=0,
    CITRUSLEAF_WRITE_IF_KEY_NOT_EXIST=1,
    CITRUSLEAF_WRITE_IF_BINS_NOT_EXIST=2,
} cl_write_options_enum;

extern zend_module_entry citrusleaf_module_entry;
#endif

