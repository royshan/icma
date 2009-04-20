#ifndef _CMACCONFIG_H
#define	_CMACCONFIG_H

#define DEBUG

/** if set, output all the training data */
//#define DEBUG_TRAINING

/** Enable assert statement */
#define EN_ASSERT
#ifdef EN_ASSERT
    #include <assert.h>
#endif

/**
 * If define, the POC tag set is B(begin) and E(end) <br>
 * Else, it would be I(Independent), L(Left), M(Middle) and R(Right).
 */
#define USE_BE_TAG_SET

//#define _ME_STRICT_POC_MATCHED



//#define TEST_SEG_CMD

#ifdef TEST_SEG_CMD
//Print the Trie Node Information
//#define DEBUG_TRIE_MATCH
#endif

#include <iostream>

#endif	/* _CMACCONFIG_H */

