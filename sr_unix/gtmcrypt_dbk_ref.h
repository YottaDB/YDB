/****************************************************************
 *								*
 *	Copyright 2009, 2014 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCRYPT_DBK_REF_H
#define GTMCRYPT_DBK_REF_H

/*
 * This file defines several structures that store information about every symmetric key that we load as well the encryption /
 * decryption context for any database or device that uses that particular key. The information about the key includes its raw
 * content and its hash; those are stored in gtm_keystore_t->typed objects malloced invdividually. To assist lookups based on the
 * key's hash or name, we have tree structures consisting of gtm_keystore_hash_link_t- and gtm_keystore_keyname_link_t-typed nodes,
 * respectively. Since we expect hashes to be unique, and the hash information is already stored in gtm_keystore_t nodes, the hash-
 * based tree's nodes contain no additional information and have a one-to-one and onto relationship with the gtm_keystore_t nodes.
 * The keyname-based tree's nodes, on the other hand, also contain the key name information, which may correspond to the database's
 * name or a user-chosen string, in case of device encryption. Additionally, several databases or devices may use the same key, in
 * which case one gtm_keystore_t element may be referenced by multiple keyname tree's nodes. An example is given below:
 *
 *       keystore_by_hash_head                                         keystore_by_keyname_head
 *    (gtm_keystore_hash_link_t *)   (gtm_keystore_t *)             (gtm_keystore_keyname_link_t *)
 *                 |                      ________                                 |
 *           ______|_____                |        |                          ______|_____
 *          |    link ---|-------------> | key #1 | <-----.    .------------|--- link    |
 *          | left right |               |________|        \  /             | left right |
 *          |_/_______\__|                                  \/              |_/_______\__|
 *           /         \                  ________          /\               /         \
 *   _______/____       \                |        |        /  \       ______/_____      \
 *  |    link ---|-------\-------------> | key #2 | <-----`    `-----|--- link    |      \
 *  | left right |        \              |________|                  | left right |       \
 *  |_/_______\__|         \                                         |_/_______\__|        \
 *  ...       ...           \             ________                    /        ...          \
 *                      _____\______     |        |                  /                  _____\______
 *                     |    link ---|--> | key #3 | <-.------------ / -----------------|--- link    |
 *                     | left right |    |________|    \           /                   | left right |
 *                     |_/_______\__|                   \   ______/_____               |_/_______\__|
 *                     ...       ...                     `-|--- link    |               ...      ...
 *                                                         | left right |
 *                                                         |_/_______\__|
 *                                                         ...       ...
 *
 * Because we resolve database file names' real paths when we read the configuration file, it is possible that one or more databases
 * might not yet exist (such as when issuing MUPIP CREATE), so we temporarily store whatever information we processed in an
 * unresolved databases' list, hoping that a later attempt of resolving the path will succeed. The list is singly-linked and consist
 * of gtm_keystore_keyname_link_t-typed elements; each element contains information about the key name, raw content, and hash.
 *
 * As for gtm_keystore_t elements, in addition to the key data they host a pointer to the head of the encryption / decryption state
 * list as well as to the one element of that list that is specific to database encryption. (Because database encryption does not
 * preserve its state beyond one block, we only ever create one database encryption and one database decryption state object, which
 * gets reused continuously.) Unlike devices, which can activate encryption and decryption individually, databases have to have both
 * encryption and decryption enabled at once. For that reason, if there is a database encryption context entry in one key's contexts
 * list, there is a database decryption context right after.
 *
 * The encryption / decryption contexts list is doubly-linked and consists of gtm_cipher_ctx_t-typed elements. Each element contains
 * the algorithm-specific (such as OpenSSL or GCRYPT) encryption or decryption state object, initialization vector (used only when
 * initializing the encryption / decryption state) and a pointer back to its respective key structure. An example is given below:
 *
 *                       ____________________________________________
 *                      |     cipher_head          db_cipher_entry   |
 *                .---> | (gtm_cipher_ctx_t *)  (gtm_cipher_ctx_t *) |
 *               |      |___/__________________________|_____________|
 *               |         /                           |
 *               |        /                        (DB ENCR) ---> (DB DECR)
 *               |   ____/____      _________      ____|____      _________
 *               |  |  prev   | <--|- prev   | <--|- prev   | <--|- prev   |
 *               |  |  next --|--> |  next --|--> |  next --|--> |  next   |
 *               |  |  store  |    |  store  |    |  store  |    |  store  |
 *               |  |____|____|    |____|____|    |____|____|    |____|____|
 *               |       |              |              |              |
 *               `-------'--------------'--------------'--------------`
 *
 * For the actual implementation of the above design please refer to gtmcrypt_dbk_ref.c.
 */

/* Principal structure for storing key information required to perform encryption / decryption. */
typedef struct gtm_keystore_struct
{
	unsigned char					key[SYMMETRIC_KEY_MAX];		/* Raw symmetric key contents. */
	unsigned char					key_hash[GTMCRYPT_HASH_LEN];	/* SHA-512 hash of symmetric key. */
	struct gtm_cipher_ctx_struct			*cipher_head;			/* Linked list of cipher handles for
											 * either encryption or decryption. A list
											 * is needed because multiple devices or
											 * databases can map to the same key, but
											 * the internal encryption / decryption
											 * state cannot be shared. */
	struct gtm_cipher_ctx_struct			*db_cipher_entry;		/* Direct pointer to the (only) DB
											 * encryption cipher entry (followed by the
											 * DB decryption entry). */
} gtm_keystore_t;

/* Structure for storing the encryption / decryption state for one device or any DB using the key pointed to by the store field. */
typedef struct gtm_cipher_ctx_struct
{
	crypt_key_t					handle;				/* Encryption / decryption state. */
	unsigned char					iv[GTMCRYPT_IV_LEN];		/* Initialization vector. */
	gtm_keystore_t					*store;				/* Pointer to master key object. */
	struct gtm_cipher_ctx_struct			*prev;				/* Pointer to previous element. */
	struct gtm_cipher_ctx_struct			*next; 				/* Pointer to next element. */
} gtm_cipher_ctx_t;

/* Structure to organize references to the key object by the key name, in a binary search tree fashion. */
typedef struct gtm_keystore_keyname_link_struct
{
	gtm_keystore_t					*link;				/* Link to respective key object. */
	char						key_name[GTM_PATH_MAX];		/* Logical entity that the symmetric key
											 * maps to. For databases it is the name of
											 * the database file. For devices it is a
											 * user-chosen string. */
	struct gtm_keystore_keyname_link_struct		*left;				/* Pointer to left child. */
	struct gtm_keystore_keyname_link_struct		*right;				/* Pointer to right child. */
} gtm_keystore_keyname_link_t;

/* Structure to organize references to the key object by the key hash, in a binary search tree fashion. */
typedef struct gtm_keystore_hash_link_struct
{
	gtm_keystore_t					*link;				/* Link to respective key object. */
	struct gtm_keystore_hash_link_struct		*left;				/* Pointer to left child. */
	struct gtm_keystore_hash_link_struct		*right;				/* Pointer to right child. */
} gtm_keystore_hash_link_t;

/* Structure to temporarily store key information if the real path of the respective database file name could not be obtained. */
typedef struct gtm_keystore_unres_keyname_link_struct
{
	unsigned char					key[SYMMETRIC_KEY_MAX];		/* Raw symmetric key contents. */
	unsigned char					key_hash[GTMCRYPT_HASH_LEN];	/* SHA-512 hash of symmetric key. */
	char						key_name[GTM_PATH_MAX];		/* Logical entity that the symmetric key
											 * maps to. For databases it is the name of
											 * the database file. For devices it is a
											 * user-chosen string. */
	struct gtm_keystore_unres_keyname_link_struct	*next;				/* Pointer to next element. */
} gtm_keystore_unres_keyname_link_t;

STATICFNDEF int			keystore_refresh(int *new_db_keys, int *new_db_hashes, int *new_dev_keys, int *new_dev_hashes);
STATICFNDEF int 		read_files_section(config_t *cfgp, int *n_mappings, int *new_keynames, int *new_hashes);
STATICFNDEF int 		read_database_section(config_t *cfgp, int *n_mappings, int *new_keynames, int *new_hashes);
STATICFNDEF void		gtm_keystore_cleanup_node(gtm_keystore_t *);
void				gtm_keystore_cleanup_all(void);
STATICFNDEF void		gtm_keystore_cleanup_hash_tree(gtm_keystore_hash_link_t *entry);
STATICFNDEF void		gtm_keystore_cleanup_keyname_tree(gtm_keystore_keyname_link_t *entry);
STATICFNDEF void		gtm_keystore_cleanup_unres_keyname_list(gtm_keystore_unres_keyname_link_t *entry);
int				gtmcrypt_getkey_by_keyname(char *keyname, int length, gtm_keystore_t **entry,
				int database, int nulled);
int				gtmcrypt_getkey_by_hash(unsigned char *hash, gtm_keystore_t **entry);
STATICFNDEF gtm_keystore_t	*keystore_lookup_by_hash(unsigned char *hash);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_keyname(char *keyname, int length, int nulled);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_unres_keyname(char *keyname, int *error);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_unres_keyname_hash(unsigned char *hash);
int 				keystore_new_cipher_ctx(gtm_keystore_t *entry, char *iv, int length, int action);
void 				keystore_remove_cipher_ctx(gtm_cipher_ctx_t *ctx);

/* Allocate a gtm_keystore_t element. */
#define GC_ALLOCATE_KEYSTORE_ENTRY(X)						\
{										\
	X = MALLOC(SIZEOF(gtm_keystore_t));					\
	(X)->cipher_head = NULL;						\
	(X)->db_cipher_entry = NULL;						\
}

/* Allocate a gtm_keystore_xxx_link_t element. */
#define GC_ALLOCATE_KEYSTORE_LINK(X, TYPE)					\
{										\
	X = (TYPE *)MALLOC(SIZEOF(TYPE));					\
	(X)->left = (X)->right = NULL;						\
}

/* Insert a new gtm_keystore_xxx_link_t element in a respective tree. It assumes
 * (and asserts) that there is no existing matching node.
 */
#define INSERT_KEY_LINK(ROOT, LINK, TYPE, FIELD, VALUE, LENGTH, FILL_LEN)	\
{										\
	int	diff;								\
	TYPE	*cur_node, **target_node;					\
										\
	target_node = &ROOT;							\
	while (cur_node = *target_node)	/* NOTE: Assignment!!! */		\
	{									\
		diff = memcmp(cur_node->FIELD, VALUE, LENGTH);			\
		assert(0 != diff);						\
		if (diff < 0)							\
			target_node = &cur_node->left;				\
		else								\
			target_node = &cur_node->right;				\
	}									\
	GC_ALLOCATE_KEYSTORE_LINK(*target_node, TYPE);				\
	(*target_node)->link = LINK;						\
	memset((*target_node)->FIELD, 0, FILL_LEN);				\
	memcpy((*target_node)->FIELD, VALUE, LENGTH);				\
}

/* Find a particular key based on a binary tree with a specific search criterion, such
 * as the key's name or hash. The macro causes the caller to return the found node.
 */
#define LOOKUP_KEY(ROOT, TYPE, FIELD, VALUE, LENGTH, CHECK_NULL)		\
{										\
	int	diff;								\
	TYPE	*cur_node;							\
										\
	cur_node = ROOT;							\
	while (cur_node)							\
	{									\
		diff = memcmp(cur_node->FIELD, VALUE, LENGTH);			\
		if (0 < diff)							\
			cur_node = cur_node->right;				\
		else if ((0 == diff) &&						\
			(CHECK_NULL						\
			 ? '\0' == *(((char *)cur_node->FIELD) + LENGTH)	\
			 : TRUE))						\
			return cur_node->link;					\
		else								\
			cur_node = cur_node->left;				\
	}									\
	return NULL;								\
}

/* Insert a new gtm_keystore_unres_keyname_link_t element in the unresolved keys list. */
#define INSERT_UNRESOLVED_KEY_LINK(KEY, HASH, KEYNAME)				\
{										\
	gtm_keystore_unres_keyname_link_t *node;				\
										\
	node = (gtm_keystore_unres_keyname_link_t *)MALLOC(			\
		SIZEOF(gtm_keystore_unres_keyname_link_t));			\
	memcpy(node->key, KEY, SYMMETRIC_KEY_MAX);				\
	memcpy(node->key_hash, HASH, GTMCRYPT_HASH_LEN);			\
	memset(node->key_name, 0, GTM_PATH_MAX);				\
	strncpy(node->key_name, KEYNAME, GTM_PATH_MAX);				\
	node->next = keystore_by_unres_keyname_head;				\
	keystore_by_unres_keyname_head = node;					\
}

/* Remove all elements from the unresolved keys tree. */
#define REMOVE_UNRESOLVED_KEY_LINKS						\
{										\
	gtm_keystore_unres_keyname_link_t *curr, *temp;				\
										\
	curr = keystore_by_unres_keyname_head;					\
	while (curr)								\
	{									\
		temp = curr->next;						\
		FREE(curr);							\
		curr = temp;							\
	}									\
	keystore_by_unres_keyname_head = NULL;					\
}

#endif /* GTMCRYPT_DBK_REF_H */
