<?php
// CitrusleafResult is returned on each Citrusleaf call
// It specifies the success or failure condition of the call
class CitrusleafResult 
{
	// Negative error codes are errors from the client side
	const CITRUSLEAF_NO_HOSTS=-5;
    const CITRUSLEAF_INVALID_API_PARAM=-4;
	const CITRUSLEAF_FAIL_ASYNCQ_FULL = -3;
    const CITRUSLEAF_FAIL_TIMEOUT = -2;
    const CITRUSLEAF_FAIL_CLIENT = -1;
    const CITRUSLEAF_OK=0; 
    const CITRUSLEAF_UNKNOWN=1;
    const CITRUSLEAF_KEY_NOT_FOUND_ERROR=2; 
    const CITRUSLEAF_GENERATION_ERROR=3; 
    const CITRUSLEAF_PARAMETER_ERROR=4;
    const CITRUSLEAF_KEY_FOUND_ERROR=5; 
    const CITRUSLEAF_BIN_FOUND_ERROR=6;
    const CITRUSLEAF_CLUSTER_KEY_MISMATCH=7;
    const CITRUSLEAF_PARTITION_OUT_OF_SPACE=8;
    const CITRUSLEAF_SERVERSIDE_TIMEOUT=9;
    const CITRUSLEAF_NO_XDS=10;
    const CITRUSLEAF_SERVER_UNAVAILABLE=11;
    const CITRUSLEAF_INCOMPATIBLE_TYPE=12;
    const CITRUSLEAF_RECORD_TOO_BIG=13;
    const CITRUSLEAF_KEY_BUSY=14;
};

// Specifies if any retry should automatically be done on writes
class CitrusleafWritePolicy
{
    const ONCE=1;
    const RETRY=2;
    const ASSURED=3;
};

// Specifies if any uniqueness constraint should be applied on a write
class CitrusleafWriteUniqueFlag
{
    const NONE=0;
    const WRITE_ONLY_KEY_NOT_EXIST=1;
    const WRITE_ONLY_BINS_NOT_EXIST=2;
}
?>
