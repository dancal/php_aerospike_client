/*
 *  Citrusleaf Tools
 *  php_cl_query.c - Uses the 'multi_get' function to get a large number of keys, and
 *     compute stats
 *
 *  Copyright 2009 by Citrusleaf.  All rights reserved.
 *  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE.  THE COPYRIGHT NOTICE
 *  ABOVE DOES NOT EVIDENCE ANY ACTUAL OR INTENDED PUBLICATION.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_interfaces.h" // needed for calling the internal serialize function
#include "Zend/zend_exceptions.h" // the serialize boilerplate's throwing exceptions, so I should too

#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"

#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/stat.h>

#include "citrusleaf/cf_atomic.h"
#include "citrusleaf/cf_shash.h"
#include "citrusleaf/cf_clock.h"
#include "citrusleaf/citrusleaf.h"
#include "php_citrusleaf.h"

#define DATE_HOUR_SIZE 10
#define MAX_KEY_SIZE 160

typedef struct stats_count_s {
	shash *distinct_hash;
	cf_atomic_int click_count;
} __attribute__ ((__packed__)) stats_count;

#define MAX_FIELD_SIZE 80
typedef struct stats_key_s {
	char s[MAX_FIELD_SIZE+1];
} __attribute__ ((__packed__)) stats_key;

static pthread_mutex_t group_query_lock = PTHREAD_MUTEX_INITIALIZER;

/* FNV32
 * The 32-bit Fowler-Noll-Voll hash function (FNV-1a) */

#define FNV_PRIME_32 16777619
#define FNV_OFFSET_32 2166136261U
uint32_t FNV32(void *k)
{
	stats_key *key = (stats_key *)k;
	uint8_t *bufp = (uint8_t *)key->s, *bufe = bufp + strlen(key->s);

    uint32_t hash = FNV_OFFSET_32;
	while (bufp < bufe) {
		hash ^= (uint64_t)*bufp++; // xor next byte into the bottom of the hash
        hash = hash * FNV_PRIME_32; // Multiply by prime number found to work well
    }

    return hash;
} 

//
// ad_space = true - as_space_id
// ad_space = false - flight_ad_id
// mon = true - month
// mon = false - day
//
void add_stats(shash *stats_hash, char *distinct_attr_s, int64_t clicks_i, char *group_attr_s) {

	stats_count value;	
	stats_key key1, key2;
    memset(&key1, 0, sizeof(key1));
    memset(&key2, 0, sizeof(key2));

	strncpy(key1.s, group_attr_s, MAX_FIELD_SIZE);
	strncpy(key2.s, distinct_attr_s, MAX_FIELD_SIZE);

    // check if the groupby item already exists in the hash table
	if (SHASH_OK == shash_get(stats_hash, &key1, &value)) {
		cf_atomic_int_add(&value.click_count, clicks_i);
		int dummy = 1;
		if (SHASH_OK != shash_get(value.distinct_hash, &key2, &dummy))
			shash_put(value.distinct_hash, &key2, &dummy);
	}
	else { // take the lock to create the item
		pthread_mutex_lock(&group_query_lock);
		if (SHASH_OK == shash_get(stats_hash, &key1, &value)) {
			// someone else has created this item already
			pthread_mutex_unlock(&group_query_lock);
			cf_atomic_int_add(&value.click_count, clicks_i);
			int dummy = 1;
			if (SHASH_OK != shash_get(value.distinct_hash, &key2, &dummy))
				shash_put(value.distinct_hash, &key2, &dummy);
		}
		else {
			// insert in critical section
    		shash_create(&value.distinct_hash, FNV32, sizeof(stats_key), sizeof(int), 200000, 0);
			int dummy = 1;
			shash_put(value.distinct_hash, &key2, &dummy);
			cf_atomic_int_set(&value.click_count, clicks_i);
			shash_put(stats_hash, &key1, &value);
			pthread_mutex_unlock(&group_query_lock);
		}
	}

	return;
}

#define ALLOC_BLOCK_SIZE 100000
void add_keys_to_digest_table(index_record *record, sconfig *c)
{
	if (record->count == 0) // no records to add
		return;

	int64_t index = cf_atomic_int_add(&c->irs_curr, 1) - 1; // get the value that was incremented from

	if ((index < c->irs_size) && (c->irs[index] == NULL)) { // this has to be true
		size_t record_size = sizeof(index_record) + record->count * sizeof(cf_digest);
		c->irs[index] = (index_record *) malloc(record_size);
		if (c->irs[index])	
			memcpy(c->irs[index], record, record_size);
	} 
}

int
index_getmany_cb(char *ns, cf_digest *keyd, char* set, uint32_t generation, uint32_t record_ttl, cl_bin *bins, int n_bins, 
	bool is_last, void *udata)
{
	sconfig *c = (sconfig *)udata;
	
	if (n_bins != 1) {
		php_printf( "<p>Illegal number of bins %d in index record, expect 1</p>", n_bins);
		return(0);
	}
	
	cl_bin *bin = &bins[0];
	if (bin->object.type != CL_BLOB) {
		php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
		return(0);
	}
	index_record *record = (index_record *) bin->object.u.blob;
    if (!record) {
		php_printf( "<p> Index record value is NULL!</p>");
		return(0);
	}

	// add the keys to the digest table
	add_keys_to_digest_table(record, c);

	return(0);
}

/*
int
groupby_getmany_cb(char *ns, cl_object *key, cf_digest *keyd, uint32_t generation, uint32_t record_ttl, cl_bin *bins, int n_bins, 
	bool is_last, void *udata)
{
	sconfig *c = (sconfig *)udata;
	
	// base64 encode the key
	char digest64[sizeof(cf_digest) * 2];
	int dlen = sizeof(cf_digest);
	base64_tostring((uint8_t *) keyd, digest64, &dlen);	

    
	int64_t clicks_i = 0;
	char *group_attr_s = NULL;
	char *distinct_attr_s = NULL;
	char *date_hour_s = NULL;
	char *account_id_s = NULL;
	for (int i=0; i < n_bins; i++)
	{
		cl_bin *bin = &bins[i];
		if ((bin->object.type != CL_STR) && (bin->object.type != CL_INT)) {
			if (c->verbose)
				php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
			return(0);
		}
		if (strcmp(bin->bin_name, c->group_attr) == 0) {
			if (bin->object.type != CL_STR) {
				if (c->verbose)
					php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
				return(0);
			}
			group_attr_s = bin->object.u.str;
		}
		else if (strcmp(bin->bin_name, c->distinct_attr) == 0) {
			if (bin->object.type != CL_STR) {
				if (c->verbose)
					php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
				return(0);
			}
			distinct_attr_s = bin->object.u.str;
		}
		else if (strcmp(bin->bin_name, c->count_attr) == 0) {
			if (bin->object.type != CL_INT) {
				if (c->verbose)
					php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
				return(0);
			}
			clicks_i = bin->object.u.i64;
		}
		else if (strcmp(bin->bin_name, c->date_hour_attr) == 0) {
			if (bin->object.type != CL_STR) {
				if (c->verbose)
					php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
				return(0);
			}
			date_hour_s = bin->object.u.str;
		}
		else if (strcmp(bin->bin_name, c->account_id_attr) == 0) {
			if (bin->object.type != CL_STR) {
				if (c->verbose)
					php_printf( "<p>Illegal type %d for bin %s</p>",bin->object.type, bin->bin_name);
				return(0);
			}
			account_id_s = bin->object.u.str;
		}
	}

	int year = 0; int month = 0; int day = 0; int hour = 0;
	char c_date_hour[DATE_HOUR_SIZE+1];
	memset(c_date_hour, 0, sizeof(c_date_hour));
	sscanf(date_hour_s, "%4d-%2d-%2d %2d", &year, &month, &day, &hour);
	if (c->verbose) 
		php_printf( "<p>in db: %s=%s,%s=%s,%s=%"PRIi64"</p>", c->group_attr, group_attr_s, c->distinct_attr, distinct_attr_s, c->count_attr, clicks_i);
	make_date_hour(c_date_hour, year, month, day, hour);

	uint32_t date_hour = atoi(c_date_hour);

	// verify that the record is within the correct range
	if ((date_hour < c->begin_date_hour) || (date_hour >= c->end_date_hour)) {
		if (c->verbose | c->index)
			php_printf( "<p>date %d not within %d and %d</p>", date_hour, c->begin_date_hour, c->end_date_hour);
		return(0);
	}
	// verify that the record is within the correct range
	if (strcmp(c->account_id, account_id_s) != 0) {
		if (c->verbose)
			php_printf( "<p>account id %s does not match %s</p>", account_id_s, c->account_id);
		return(0);
	}
	if (c->verbose) 
		php_printf( "<p>in db: %s=%s,%s=%s,%s=%"PRIi64"</p>", c->group_attr, group_attr_s, c->distinct_attr, distinct_attr_s, c->count_attr, clicks_i);
	if (!group_attr_s || !distinct_attr_s) {
		php_printf( "<p>some bins are null</p>");
		return(0);
	}
	
	add_stats(c->group_hash, distinct_attr_s, clicks_i, group_attr_s);

	c->records_counter++;
	
	return(0);
}
*/

int
fast_groupby_getmany_cb(char *ns, cf_digest *keyd, char* set, uint32_t generation, uint32_t record_ttl, cl_bin *bins, int n_bins, 
	bool is_last, void *udata)
{
	sconfig *c = (sconfig *)udata;
	
	int64_t clicks_i = 0;
	char *group_attr_s = NULL;
	char *distinct_attr_s = NULL;
    int n_found = 0;
	for (int i=0; i < n_bins; i++)
	{
		cl_bin *bin = &bins[i];

		if (strcmp(bin->bin_name, c->group_attr) == 0) {
			n_found++;
			group_attr_s = bin->object.u.str;
		}
		else if (strcmp(bin->bin_name, c->distinct_attr) == 0) {
			n_found++;
			distinct_attr_s = bin->object.u.str;
		}
		else if (strcmp(bin->bin_name, c->count_attr) == 0) {
			n_found++;
			clicks_i = bin->object.u.i64;
		}
		if (n_found == 3)
			break;
	}
	if (n_found == 3) {
		add_stats(c->group_hash, distinct_attr_s, clicks_i, group_attr_s);
		cf_atomic_int_add(&c->records_counter, 1);
	}
	
	return(0);
}


int
do_indexquery(sconfig *c)
{
	if (c->verbose)
		php_printf( "<p>starting index query</p>");

	// compute the range of digests
	
	// 
	// Convert the date into the number of hours since Jan 1, 1970
	// 
	// make_date_hour initializes the tm structure and calls mktime.
	// Note that TZ needs to be UTC (see main)
	//
	
	if ((strlen(c->begin_date) != DATE_HOUR_SIZE) || (strlen(c->end_date) != DATE_HOUR_SIZE)){
		php_printf( "<p> BEGIN %s END %s </p>", c->begin_date, c->end_date);
		php_printf( "<p> BAD begin date or end date not length %d</p>", DATE_HOUR_SIZE);
		return(-1);
	}
	int year = 0; int month =0; int day = 0; int hour = 0;
	char c_begin_date_hour[DATE_HOUR_SIZE+1];
	memset(c_begin_date_hour, 0, sizeof(c_begin_date_hour));
	sscanf(c->begin_date, "%4d%2d%2d%2d", &year, &month, &day, &hour);
	if (c->verbose) 
		php_printf( "<p>begin date: year %d, month %d, day %d, hour %d</p>", year, month, day, hour);
	make_date_hour(c_begin_date_hour, year, month, day, hour);

	year = 0; month =0; day = 0; hour = 0;
	char c_end_date_hour[DATE_HOUR_SIZE+1];
	memset(c_end_date_hour, 0, sizeof(c_end_date_hour));
	sscanf(c->end_date, "%4d%2d%2d%2d", &year, &month, &day, &hour);
	if (c->verbose) 
		php_printf( "<p>end date: year %d, month %d, day %d, hour %d</p>", year, month, day, hour);
	make_date_hour(c_end_date_hour, year, month, day, hour);
	if ((strcmp(c_begin_date_hour, "") == 0) || (strcmp(c_begin_date_hour, "") == 0)){
		php_printf( "<p>BAD begin date or end date </p>");
		return(-1);
	}
	uint32_t begin_date_hour = atoi(c_begin_date_hour);
	uint32_t end_date_hour = atoi(c_end_date_hour);

	if (end_date_hour <= begin_date_hour) { // range is invalid
		php_printf( "<p>begin date hour %s is later than end date hour %s</p>", c_begin_date_hour, c_end_date_hour);
		return(-1);
	}
	c->begin_date_hour = begin_date_hour;
	c->end_date_hour = end_date_hour;

	if (c->index) { // use index is set to true
		char *account_id_s = c->account_id;
		int n_index_lookups = end_date_hour - begin_date_hour;
		// php_printf( "<p>number of index queries %d </p>", n_index_lookups);
		// number of index queries is equal to end_date_hour - begin_date_hour + 1;
		cf_digest *digests = (cf_digest *) malloc(sizeof(cf_digest)*n_index_lookups);
	
		for (int i=0; i < n_index_lookups; i++)
		{
			// generate key and compute digest	
			char index_key[MAX_KEY_SIZE+1];
			memset(index_key, 0, sizeof(index_key));
			size_t account_id_len = strlen(account_id_s);
			size_t index_key_len = account_id_len + DATE_HOUR_SIZE + 1;
			if (index_key_len > MAX_KEY_SIZE) {
				free(digests);
				php_printf( "<p>Index Key %s:%d has size %"PRIu64". Max size allowed is %d</p>", account_id_s, begin_date_hour + i, index_key_len, MAX_KEY_SIZE);
				return(-1);
			}
			memcpy(index_key, account_id_s, account_id_len);
			index_key[account_id_len] = ':';
			sprintf(index_key + account_id_len + 1, "%d", begin_date_hour + i);
        	cl_object index_key_o;
        	citrusleaf_object_init_str(&index_key_o, index_key);
			citrusleaf_calculate_digest(c->iset, &index_key_o , &digests[i]);
		}
     
		c->irs = (index_record **) malloc(sizeof(index_record *) * n_index_lookups);
		memset(c->irs, 0, sizeof(index_record *) * n_index_lookups);
		if (!c->irs) {
			free(digests);
			php_printf( "<p>citrusleaf index getmany failed, malloc </p>");
			return(-1);
		}
		c->irs_size = n_index_lookups;
		cf_atomic_int_set(&c->irs_curr, 0);

	// call the getmany function
		cl_rv rv = citrusleaf_get_many_digest(c->asc, c->ns, digests, n_index_lookups, 0 /*bins*/, 0 /*nbins*/, false, index_getmany_cb, c);
		if ( CITRUSLEAF_OK != rv ) {
			free(digests);
			php_printf( "<p>citrusleaf index getmany failed, error %d</p>",rv);
			return(-1);
		}
		free(digests);
	}
	
	return(0);
}


int
do_batchquery(sconfig *c)
{
	if (c->verbose)
		php_printf( "<p>starting batch query</p>");

    cl_bin values[3];		
	strcpy(values[0].bin_name, c->group_attr);
	//values[0].op = CL_OP_READ;
	citrusleaf_object_init_null(&values[0].object);
	strcpy(values[1].bin_name, c->distinct_attr);
	//values[1].op = CL_OP_READ;
	citrusleaf_object_init_null(&values[1].object);
	strcpy(values[2].bin_name, c->count_attr);
	//values[2].op = CL_OP_READ;
	citrusleaf_object_init_null(&values[2].object);
	

	// call the getmany function
	if (c->index) {
		cl_rv rv = citrusleaf_get_many_digest(c->asc, c->ns, c->digests, c->n_digests, values, 3, false, fast_groupby_getmany_cb, c);
		if ( CITRUSLEAF_OK != rv ) {
			php_printf( "<p>citrusleaf getmany failed, error %d</p>",rv);
			return(-1);
		}
	}
	else {
		cl_rv rv = citrusleaf_scan(c->asc, c->ns, c->set, values, 3, false, fast_groupby_getmany_cb, c, false);
		if ( CITRUSLEAF_OK != rv ) {
			php_printf( "<p>citrusleaf getmany failed, error %d</p>",rv);
			return(-1);
		}
	}
	
	return(0);
}

int print_stats_fn(void *key, void *data, void *udata) {
	char *skey = (char *) key;
    stats_count *value = (stats_count *) data;
	int64_t clicks = cf_atomic_int_get(value->click_count);
	php_printf( "<tr> <td> %s </td> <td>%"PRIi64"</td> <td> %d</td> </tr>", skey, clicks, shash_get_size(value->distinct_hash));
	return(SHASH_OK);
}

// #define LOCAL_DEBUG 1
int
group_query(sconfig *c)
{
	setenv("TZ", "UTC", 1); // make sure that all times are manipulated as UTC
	

	if (!c) {
		php_printf("<p> ERROR: Query with Null parameters </p>");
		return(-1);
	}
	
	if (c->index == false) {
		php_printf("<p> Not enabled for scan </p>");
		return(-1);
	}

#ifdef LOCAL_DEBUG
	php_printf("<p> group query [0] </p>");
#endif
   	
	if (!c->asc || !c->ns || !c->set || !c->iset || !c->account_id || !c->begin_date || !c->end_date || !c->group_attr || !c->distinct_attr || !c->date_hour_attr || !c->account_id_attr || !c->count_attr) {
		php_printf("<p> group query [1] </p>");
		return(-1);
	}
	
	php_printf( "<p> group by: ns %s set %s begin %s end %s group by %s distinct %s </p>",c->ns,c->set,c->begin_date, c->end_date, c->group_attr, c->distinct_attr);

#ifdef LOCAL_DEBUG
	php_printf("<p> group query [2] </p>");
#endif

    shash_create(&c->group_hash, FNV32, sizeof(stats_key), sizeof(stats_count), 1024, 0);

	cf_atomic_int_set(&c->records_counter, 0);
	c->irs = NULL;
	c->digests = NULL;
	c->n_digests = 0;
	cf_atomic_int_set(&c->irs_curr, 0);
	
	uint64_t start_time = cf_getms();
	if (0 != do_indexquery(c)) {
		// php_printf("<p> group query [4] </p>");
		php_printf( "<p> could not execute index query </p>");
		return(-1);
	}

	if (!c->irs) {
		// php_printf("<p> group query [6] </p>");
		php_printf("<p> no results from index query!</p>");
		return(-1);
	}


	c->n_digests = 0;
	// count the data into a digest array
	for (int i=0; i < c->irs_size; i++)
	{
		if (c->irs[i])
			c->n_digests += c->irs[i]->count;
	}

	if (c->n_digests == 0) {
		// php_printf("<p> group query [6] </p>");
		php_printf("<p> no results from index query!</p>");
		return(-1);
	}

	c->digests = (cf_digest *) malloc (c->n_digests * sizeof(cf_digest));

	if (!c->digests) {
		// php_printf("<p> group query [6] </p>");
		php_printf("<p> cannot malloc!</p>");
		return(-1);
	}
	
	int offset = 0;
	// copy the data into a digest array
	for (int i=0; i < c->irs_size; i++)
	{
		if (!c->irs[i] || (c->irs[i]->count == 0)) 
			continue;
		memcpy(&c->digests[offset], c->irs[i]->redirect_id, sizeof(cf_digest) * c->irs[i]->count);
		offset += c->irs[i]->count;
		free(c->irs[i]);
		c->irs[i] = NULL;
	}

#ifdef LOCAL_DEBUG
	php_printf("<p> group query [5] </p>");
#endif
	if (0 != do_batchquery(c)) {
		// php_printf("<p> group query [6] </p>");
		php_printf("<p> could not execute group query!</p>");
		return(-1);
	}
	if (c->digests) {
		free(c->digests);
		c->digests = NULL;
	}
	c->elapsed_time = cf_getms() - start_time;
	
	php_printf( "<p> Date hour: begin %d end %d, index entries = %d </p>", c->begin_date_hour, c->end_date_hour, c->end_date_hour - c->begin_date_hour + 1);
#ifdef LOCAL_DEBUG
	php_printf("<p> group query [7] </p>");
#endif
	php_printf( "<table> <tr> <td>group by %s</td><td>sum(%s)</td><td>count(distinct %s)</td></tr>", c->group_attr, c->count_attr, c->distinct_attr);
#ifdef LOCAL_DEBUG
	php_printf("<p> group query [8] </p>");
#endif
    shash_reduce( c->group_hash, print_stats_fn, 0); 
#ifdef LOCAL_DEBUG
	php_printf("<p> group query [9] </p>");
#endif
	php_printf("<table>");
	int64_t records_counter = cf_atomic_int_get(c->records_counter);
	php_printf( "<p> elapsed time = %"PRIi64" ms, records aggregated = %"PRIi64", rows per second = %"PRIi64"</p>", c->elapsed_time, records_counter,(records_counter * 1000) / c->elapsed_time);
#ifdef LOCAL_DEBUG
	php_printf("<p> group query [10] </p>");
#endif
	
	return(0);
}


// PUT INDEX CODE

index_record *create_record(index_record *current, cf_digest new, uint *records_size) {

	index_record *old_ptr = current;
	uint64_t old_count =  (old_ptr != NULL) ? old_ptr->count : 0;
	uint64_t new_count = old_count + 1;
	uint size = sizeof(index_record) + new_count * sizeof(cf_digest);
	index_record *new_ptr = (index_record *) malloc(size);
	new_ptr->count = new_count;
	if (old_count > 0)
		memcpy(new_ptr->redirect_id, old_ptr->redirect_id, old_count * sizeof(cf_digest));
	new_ptr->redirect_id[new_count-1] = new;
	// if (old_ptr)
	// free(old_ptr);
	
	*records_size =  size;
	return (new_ptr);
}

bool record_is_new(cf_digest new, index_record *old_records, iconfig *c) {
	if (!old_records)
		return (true);
	if (c->verbose) fprintf(stderr, "new :  %"PRIx64" \n", *(uint64_t *)&new);

	for (int i = 0; i < old_records->count; i++) {
		cf_digest old_rec = old_records->redirect_id[i];	
		if (c->verbose) fprintf(stderr, "in db:  %"PRIx64" \n", *(uint64_t *)&old_rec);
		if (memcmp(&old_records->redirect_id[i], &new, sizeof(cf_digest)) == 0)
			return (false);
	}
	return (true);
}


bool is_non_null_string(char *str)
{
	if (!str)
		return false;
	if (*str == '\0')
		return false;
	return true;
}

int
put_index(iconfig *c)
{
	setenv("TZ", "UTC", 1); // make sure that all times are manipulated as UTC
	if (!c) {
		php_printf("<p> ERROR: put_index called with Null parameters </p>");
		return(-1);
	}
	
	char *account_id_s = c->account_id;
	char *domain_id_s = c->domain_id;
	char *campaign_id_s = c->campaign_id;
	char *original_url_id_s = c->original_url_id;
	char *share_type_s = c->share_type;
	char *create_type_s = c->create_type;
	char *sharer_id_s = c->sharer_id;
	char *parent_id_s = c->parent_id;
	char *user_id_s = c->user_id;
	char *notes_s = c->notes;
	char *redirection_id_s = c->redirection_id;
	char *date_hour_s = c->hour;
	char *clicks_s = c->clicks;

	// 
	// Convert the date into the number of hours since Jan 1, 1970
	// 
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	sscanf(date_hour_s, "%4d-%2d-%2d %2d", &year, &month, &day, &hour);
	if (c->verbose) 
		fprintf(stderr, "year %d, month %d, day %d, hour %d\n", year, month, day, hour);
	// 
	// Initialize the tm structure and call mktime.
	// Note that TZ needs to be UTC (see main)
	//
	char c_date_hour_s[DATE_HOUR_SIZE+1];
	memset(c_date_hour_s, 0, sizeof(c_date_hour_s));
	make_date_hour(c_date_hour_s, year, month, day, hour);

	if (c->verbose) 
		fprintf(stderr, "%s\n", c_date_hour_s);

  	bool index_updated = true;

	char index_key[MAX_KEY_SIZE+1];
	memset(index_key, 0, sizeof(index_key));
	size_t account_id_len = strlen(account_id_s);
	size_t c_date_hour_len = strlen(c_date_hour_s);
	size_t redirection_id_len = strlen(redirection_id_s);
	size_t index_key_len = account_id_len + c_date_hour_len + 1;
	if (index_key_len > MAX_KEY_SIZE) {
		fprintf(stderr, "Index Key %s:%s has size %"PRIu64". Max size allowed is %d\n", account_id_s, c_date_hour_s, index_key_len, MAX_KEY_SIZE);
		return(-1);
	}
	memcpy(index_key, account_id_s, account_id_len);
	index_key[account_id_len] = ':';
	memcpy(index_key + account_id_len + 1, c_date_hour_s, c_date_hour_len);

	char rollup_key[MAX_KEY_SIZE+1];
	memset(rollup_key, 0, sizeof(rollup_key));
    size_t rollup_key_len = index_key_len + redirection_id_len + 1;
	if (rollup_key_len > MAX_KEY_SIZE) {
		fprintf(stderr, "Rollup Key %s:%s has size %"PRIu64". Max size allowed is %d\n", index_key, redirection_id_s, rollup_key_len, MAX_KEY_SIZE);
		return(-1);
	}
	memcpy(rollup_key, index_key, index_key_len);
	rollup_key[index_key_len] = ':';
	memcpy(rollup_key + index_key_len + 1, redirection_id_s, redirection_id_len);
                
    // construct the key for the index account_id:date_hour
	// lookup the index object, add the redirection_id if needed, using read modify write until successful
	// 
                
	cl_object index_key_o;
	cf_digest new_recd;
    citrusleaf_object_init_str(&index_key_o, rollup_key);
	citrusleaf_calculate_digest( "dr", &index_key_o, &new_recd);
				// new_rec = atoll(redirection_id_s);
				
	if (c->verbose) 
		fprintf(stderr, "record digest:  %"PRIx64" \n", *(uint64_t *)&new_recd);

#ifdef DEBUG_VERBOSE
			fprintf(stderr, "load test: namespace %s  digest %"PRIx64"\n",c->ns,*(uint64_t *) &digest);
#endif			
	if (c->verbose) 
		fprintf(stderr, "index_key: %s\n", index_key);

	if (c->verbose) 
		fprintf(stderr, "record: %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
				account_id_s,
				domain_id_s,
				campaign_id_s,
				original_url_id_s,
				share_type_s,
				create_type_s,
				sharer_id_s,
				parent_id_s,
				user_id_s,
				notes_s,
				redirection_id_s,
				date_hour_s,
				clicks_s);

	citrusleaf_object_init_str(&index_key_o, index_key);
int tries = 0;

retry: for (int i =0; i<1; i++){} ;
	tries++;
	uint records_size = 0;
	cl_bin bins_buf[13];
	cl_bin *bins = 0, *bin = 0;
	cl_bin *index_bins = 0;
	int     n_index_bins = 0;

	cl_rv index_rv;
	uint32_t index_cl_gen = 0;
	index_rv = citrusleaf_get_all(c->asc, c->ns, c->iset, &index_key_o, &index_bins, &n_index_bins, c->timeout, &index_cl_gen);
			
	if (index_rv == CITRUSLEAF_OK ) {
				// Check if index entry exists

		if (index_cl_gen == 0) {
			fprintf(stderr, "Illegal null value for generation \n");
			return(-1);
		}
		if (n_index_bins != 1)	 {
			fprintf(stderr, "Illegal number of bins %d for key %s\n",n_index_bins, index_key);
			return(-1);
		}
		cl_bin *bin = &index_bins[0];
		if (strcmp(bin->bin_name, "ir") != 0) {
			fprintf(stderr, "Illegal name for user_id bin %s\n",bin->bin_name);
			return(-1);
		}
		if (bin->object.type != CL_BLOB) {
			fprintf(stderr, "Illegal type for blob bin %d\n",bin->object.type);
			return(-1);
		}
				
		// Get the existing blob object and check if the current redirection id is in there already
		// if so, then do nothing. If the current record does not exist, change the value
		index_record *old_record = (index_record *) bin->object.u.blob;
		if (record_is_new(new_recd, old_record, c)) {
			if (c->verbose) fprintf(stderr, "Appending to existing record for rollup key %s generation %d\n",rollup_key, index_cl_gen);
			// atomic_int_add(c->notfound_counter, 1);
			int nbins = 1;
					// we create two bins
					// Create the correct number of bins
			bins = bin = &bins_buf[0];
			strcpy(bin->bin_name, "ir");
			index_record *records = create_record(old_record, new_recd, &records_size);
			citrusleaf_object_init_blob(&bin->object, records, records_size);

			cl_write_parameters cl_w_p;
			cl_write_parameters_set_default(&cl_w_p);
			cl_write_parameters_set_generation(&cl_w_p, index_cl_gen); // need to do CAS so index entries are not overwritten
			index_rv = citrusleaf_put(c->asc, c->ns, c->iset, &index_key_o, bins, nbins, &cl_w_p);
			free(records);
			if (index_rv == CITRUSLEAF_FAIL_GENERATION) {
				if (c->verbose) fprintf(stderr, "CAS Failure - will retry : rv %d\n",index_rv);
				goto retry;
			}
			if (index_rv != CITRUSLEAF_OK) {
				fprintf(stderr, "get failed in load: rv %d\n",index_rv);
				// atomic_int_add(c->fail_counter, 1);
				index_updated = false;
			}
		}
		else if (c->verbose)
			fprintf(stderr, "Record exists already for rollup key %s generation %d\n",rollup_key, index_cl_gen);
	}
	else if (index_rv == CITRUSLEAF_FAIL_TIMEOUT) {
		// atomic_int_add(c->timeout_counter, 1);
		// if (c->verbose) fprintf(stderr, "get timeout: delta %"PRIu64"\n",cf_getms() - start);
		index_updated = false;
	}
	else if (index_rv == CITRUSLEAF_FAIL_NOTFOUND) {
		if (c->verbose) fprintf(stderr, "Creating new record for rollup key %s generation %d\n",rollup_key, index_cl_gen);
		int nbins = 1;
		bins = bin = &bins_buf[0];
		strcpy(bin->bin_name, "ir");
		index_record *record = create_record(NULL, new_recd, &records_size);
		citrusleaf_object_init_blob(&bin->object, record, records_size);

		// these failures will always happen because the backup is out of date
		// compared with the real world
		// atomic_int_add(c->notfound_counter, 1);
		// if (c->verbose) fprintf(stderr, "get failed in load: NOTFOUND\n");
		cl_write_parameters cl_w_p;
		cl_write_parameters_set_default(&cl_w_p);
		cl_w_p.unique = true;
		index_rv = citrusleaf_put(c->asc, c->ns, c->iset, &index_key_o, bins, nbins, &cl_w_p);
		free(record);
		if (index_rv == CITRUSLEAF_FAIL_KEY_EXISTS) {
			if (c->verbose) fprintf(stderr, "Concurrent index record creation Failure - will retry : rv %d\n",index_rv);
			goto retry;
		}
		if (index_rv != CITRUSLEAF_OK) {
			fprintf(stderr, "get failed in load: rv %d\n",index_rv);
			// atomic_int_add(c->fail_counter, 1);
			index_updated = false;
		}
	}
	else {
		fprintf(stderr, "get failed in load: rv %d\n",index_rv);
		// atomic_int_add(c->fail_counter, 1);
		index_updated = false;
	}
	if (index_bins)	{
		citrusleaf_bins_free(index_bins, n_index_bins);
		free(index_bins);
	}
// START DATA WRITE
	if (index_updated == false) {
		if (c->verbose)
		    fprintf(stderr, "INDEX UPDATE FAILED. ERROR, ERROR\n");
		return (-1);
	} 
	else {
       	cl_object key_o;
		citrusleaf_object_init_str(&key_o, rollup_key);

		cl_rv rv;

		int nbins = 0;
		bins = &bins_buf[0];
		if (is_non_null_string(account_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ai");
			citrusleaf_object_init_str(&bin->object, account_id_s);
		}
		if (is_non_null_string(domain_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "di");
			citrusleaf_object_init_str(&bin->object, domain_id_s);
		}
		if (is_non_null_string(campaign_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ci");
			citrusleaf_object_init_str(&bin->object, campaign_id_s);
		}
		if (is_non_null_string(original_url_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ui");
			citrusleaf_object_init_str(&bin->object, original_url_id_s);
		}
		if (is_non_null_string(share_type_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "st");
			citrusleaf_object_init_str(&bin->object, share_type_s);
		}
		if (is_non_null_string(create_type_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ct");
			citrusleaf_object_init_str(&bin->object, create_type_s);
		}
		if (is_non_null_string(sharer_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "si");
			citrusleaf_object_init_str(&bin->object, sharer_id_s);
		}
		if (is_non_null_string(parent_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "pi");
			citrusleaf_object_init_str(&bin->object, parent_id_s);
		}
		if (is_non_null_string(user_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ui");
			citrusleaf_object_init_str(&bin->object, user_id_s);
		}
		if (is_non_null_string(notes_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ns");
			citrusleaf_object_init_str(&bin->object, notes_s);
		}
		if (is_non_null_string(redirection_id_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "ri");
			citrusleaf_object_init_str(&bin->object, redirection_id_s);
		}
		if (is_non_null_string(date_hour_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "dh");
			citrusleaf_object_init_str(&bin->object, date_hour_s);
		}
		if (is_non_null_string(clicks_s)) {
			bin = &bins_buf[nbins++];
			strcpy(bin->bin_name, "cl");
			citrusleaf_object_init_int(&bin->object, atoll(clicks_s));
		}
		// if (c->verbose) fprintf(stderr, "get failed in load: NOTFOUND\n");
		cl_write_parameters cl_w_p;
		cl_write_parameters_set_default(&cl_w_p);
		rv = citrusleaf_put(c->asc, c->ns, c->set, &key_o, bins, nbins, &cl_w_p);
		if (rv != CITRUSLEAF_OK) {
			fprintf(stderr, "get failed in load: rv %d\n",rv);
			return(-1);
			// atomic_int_add(c->fail_counter, 1);
		}
	}
	return(0);
}


// Update Clicks


int
update_clicks(uconfig *c)
{
	setenv("TZ", "UTC", 1); // make sure that all times are manipulated as UTC

	if (!c) {
		php_printf("<p> ERROR: update_clicks called with Null parameters </p>");
		return(-1);
	}
	
	char *account_id_s = c->account_id;
	char *date_hour_s = c->hour;
	char *redirection_id_s = c->redirection_id;
	int64_t clicks = c->clicks;

	// 
	// Convert the date into the number of hours since Jan 1, 1970
	// 
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	sscanf(date_hour_s, "%4d-%2d-%2d %2d", &year, &month, &day, &hour);
	if (c->verbose) 
		fprintf(stderr, "year %d, month %d, day %d, hour %d\n", year, month, day, hour);
	// 
	// Initialize the tm structure and call mktime.
	// Note that TZ needs to be UTC (see main)
	//
	char c_date_hour_s[DATE_HOUR_SIZE+1];
	memset(c_date_hour_s, 0, sizeof(c_date_hour_s));
	make_date_hour(c_date_hour_s, year, month, day, hour);

	if (c->verbose) 
		fprintf(stderr, "%s\n", c_date_hour_s);

	char index_key[MAX_KEY_SIZE+1];
	memset(index_key, 0, sizeof(index_key));
	size_t account_id_len = strlen(account_id_s);
	size_t c_date_hour_len = strlen(c_date_hour_s);
	size_t redirection_id_len = strlen(redirection_id_s);
	size_t index_key_len = account_id_len + c_date_hour_len + 1;
	if (index_key_len > MAX_KEY_SIZE) {
		fprintf(stderr, "Index Key %s:%s has size %"PRIu64". Max size allowed is %d\n", account_id_s, c_date_hour_s, index_key_len, MAX_KEY_SIZE);
		return(-1);
	}
	memcpy(index_key, account_id_s, account_id_len);
	index_key[account_id_len] = ':';
	memcpy(index_key + account_id_len + 1, c_date_hour_s, c_date_hour_len);

	char rollup_key[MAX_KEY_SIZE+1];
	memset(rollup_key, 0, sizeof(rollup_key));
    size_t rollup_key_len = index_key_len + redirection_id_len + 1;
	if (rollup_key_len > MAX_KEY_SIZE) {
		fprintf(stderr, "Rollup Key %s:%s has size %"PRIu64". Max size allowed is %d\n", index_key, redirection_id_s, rollup_key_len, MAX_KEY_SIZE);
		return(-1);
	}
	memcpy(rollup_key, index_key, index_key_len);
	rollup_key[index_key_len] = ':';
	memcpy(rollup_key + index_key_len + 1, redirection_id_s, redirection_id_len);
                
    // construct the key for the index account_id:date_hour
	// lookup the index object, add the redirection_id if needed, using read modify write until successful
	// 
                
// START DATA WRITE
    cl_object key_o;
	citrusleaf_object_init_str(&key_o, rollup_key);

	cl_operation values[1];
	strcpy(values[0].bin.bin_name, c->clicks_attr);
	citrusleaf_object_init_int(&values[0].bin.object, c->clicks);
	values[0].op = CL_OP_INCR;
	
	cl_write_parameters cl_wp;
	cl_write_parameters_set_default(&cl_wp);
	cl_wp.timeout_ms = c->timeout;
	int replace = 0; //default
	int generation;	
	cl_rv rv = citrusleaf_operate(c->asc, c->ns, c->set, &key_o, values, 1, &cl_wp,replace,&generation);
	if (rv != CITRUSLEAF_OK) {
		fprintf(stderr, "update clicks failed: rv %d\n",rv);
		return(-1);
		// atomic_int_add(c->fail_counter, 1);
	}
	return(0);
}

//
// Useful utility function for splitting into a vector
// fmt is a string of characters to split on
// the string is the string to split
// the vector will have pointers into the strings
// this modifies the input string by inserting nulls
//

void
str_split(char split_c, char *str, cf_vector *v)
{

        char *prev = str;
        while (*str) {
                if (split_c == *str) {
                        *str = 0;
                        cf_vector_append(v, &prev);
                        prev = str+1;
                }
                str++;
        }
        if (prev != str) {
                cf_vector_append(v, &prev);
        }
}
