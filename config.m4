PHP_ARG_ENABLE(citrusleaf, whether to enable Citrusleaf support,
	[ --enable-citrusleaf   Enable Citrusleaf support])

PHP_ARG_WITH(citrusleaf-dir, for Citrusleaf support,
	[  --with-citrusleaf[=DIR]      Include Citrusleaf support.  DIR is the Citrusleaf base
	  directory.])

# The extra debug flags gets logging in the default log file
# CFLAGS="-std=gnu99 $CFLAGS -DMARCH_x86_64 -DLOGDEBUG -DDEBUG -DEBUG_VERBOSE"
CFLAGS="-std=gnu99 $CFLAGS -DMARCH_x86_64"
LDFLAGS="-lssl -lpthread -lrt $LDFLAGS"

if test "$PHP_CITRUSLEAF" != "no"; then
	if test -z "$PHP_CITRUSLEAF_DIR"; then
		if test -d "$PHP_CITRUSLEAF_DIR"; then
			CITRUSLEAF_INC_DIR=$PHP_CITRUSLEAF_DIR/include/
		else
			AC_MSG_ERROR([$PHP_CITRUSLEAF_DIR not found. Please give proper location])
		fi
	else
		CWD=`pwd`
		CITRUSLEAF_INC_DIR=$CWD/ext/citrusleaf/include/
	fi
	AC_DEFINE(HAVE_CITRUSLEAF, 1, [Whether you have Citrusleaf])
	PHP_ADD_INCLUDE($CITRUSLEAF_INC_DIR)
	CITRUSLEAF_INCLUDE=-I$CITRUSLEAF_INC_DIR
	PHP_SUBST_OLD(CITRUSLEAF_INCLUDE)
	PHP_ADD_LIBRARY_WITH_PATH(ssl, "", CITRUSLEAF_SHARED_LIBADD)
	PHP_SUBST(CITRUSLEAF_SHARED_LIBADD)
	
	citrusleaf_sources="\
	src/php_citrusleaf.c \
	src/php_cl_query.c \
	src/php_cl_utils.c \
	src/cf_alloc.c \
	src/cf_average.c \
	src/cf_digest.c \
	src/cf_hist.c \
	src/cf_hooks.c \
	src/cf_ll.c \
	src/cf_log.c \ 
	src/cf_proto.c \ 	
	src/cf_queue.c \ 
	src/cf_shash.c \ 
	src/cf_socket.c \ 
	src/cf_vector.c \ 
	src/cf_packet_compression.c \ 
	src/version.c \ 
	src/citrusleaf.c \ 
	src/cl_async.c \ 
	src/cl_batch.c \ 
	src/cl_cluster.c \ 
	src/cl_info.c \ 
	src/cl_lookup.c \ 
	src/cl_partition.c \ 
	src/cl_request.c \ 
	src/cl_scan.c \
	src/cl_shm.c"
	
	citrusleaf_headers="\
	include/citrusleaf/cf_alloc.h \
	include/citrusleaf/cf_atomic.h \
	include/citrusleaf/cf_average.h \
	include/citrusleaf/cf_base_types.h \
	include/citrusleaf/cf_byte_order.h \
	include/citrusleaf/cf_clock.h \
	include/citrusleaf/cf_digest.h \
	include/citrusleaf/cf_errno.h \
	include/citrusleaf/cf_hist.h \
	include/citrusleaf/cf_hooks.h \
	include/citrusleaf/cf_ll.h \
	include/citrusleaf/cf_log.h \
	include/citrusleaf/cf_log_internal.h \
	include/citrusleaf/cf_queue.h \
	include/citrusleaf/cf_shash.h \
	include/citrusleaf/cf_socket.h \
	include/citrusleaf/cf_vector.h \
	include/citrusleaf/proto.h \
	include/citrusleaf/citrusleaf.h \
	include/citrusleaf/citrusleaf-internal.h \
	include/citrusleaf/cl_cluster.h \
	include/citrusleaf/cl_request.h \
	include/citrusleaf/cl_shm.h \
	php_citrusleaf.h"

	PHP_NEW_EXTENSION(citrusleaf, $citrusleaf_sources, $ext_shared)
	PHP_INSTALL_HEADERS([ext/citrusleaf], [$citrusleaf_headers])
fi

