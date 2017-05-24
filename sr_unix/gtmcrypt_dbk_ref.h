/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
 * key's hash, name, or path, we have tree structures consisting of gtm_keystore_hash_link_t-, gtm_keystore_keyname_link_t-, and
 * gtm_keystore_keypath_linkt_t-typed nodes, respectively. Since we expect hashes to be unique along with the path of each key, the
 * hash and path information is already stored in gtm_keystore_t nodes, and the hash- and path- based trees' nodes contain no
 * additional information and have a one-to-one and onto relationship with the gtm_keystore_t nodes. The keyname-based tree's nodes,
 * on the other hand, also contain the key name information, which may correspond to the database's name or a user-chosen string in
 * case of device encryption. Additionally, several databases or devices may use the same key, in which case one gtm_keystore_t
 * element may be referenced by multiple keyname tree's nodes. An example is given below:
 *
 *       keystore_by_hash_head                                    keystore_by_keyname_head / keystore_by_keypath_head
 *    (gtm_keystore_hash_link_t *)   (gtm_keystore_t *)      (gtm_keystore_keyname_link_t * / gtm_keystore_keypath_link_t *)
 *                 |                      ________                                       |
 *           ______|_____                |        |                                ______|_____
 *          |    link ---|-------------> | key #1 | <-----.    .------------------|--- link    |
 *          | left right |               |________|        \  /                   | left right |
 *          |_/_______\__|                                  \/                    |_/_______\__|
 *           /         \                  ________          /\                     /         \
 *   _______/____       \                |        |        /  \             ______/_____      \
 *  |    link ---|-------\-------------> | key #2 | <-----`    `-----------|--- link    |      \
 *  | left right |        \              |________|                        | left right |       \
 *  |_/_______\__|         \                                               |_/_______\__|        \
 *  ...       ...           \             ________                          /        ...          \
 *                      _____\______     |        |                        /                  _____\______
 *                     |    link ---|--> | key #3 | <-.-------------------/------------------|--- link    |
 *                     | left right |    |________|    \                 /                   | left right |
 *                     |_/_______\__|                   \         ______/_____               |_/_______\__|
 *                     ...       ...                     `-------|--- link    |               ...      ...
 *                                                               | left right |
 *                                                               |_/_______\__|
 *                                                               ...       ...
 *
 * Because we do not want to decrypt every key from the configuration file just as we read it, we first store whatever information
 * we processed in an unresolved key list. The list is singly-linked and consists of gtm_keystore_keyname_link_t-typed elements;
 * each element contains information about the key name, path, index in the configuration file, and whether it is a key for a device
 * encryption, database whose address we already resolved, or the one that awaits resolution. (Certain database files referenced in
 * the configuration file may not actually exist, such as during MUPIP CREATE time or when loading an extract.)
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
 *                     ___________________________________________________
 *                    |     cipher_head                db_cipher_entry    |
 *                .-->| (gtm_cipher_ctx_t *)         (gtm_cipher_ctx_t *) |
 *               |    |________|_______________________________|__________|
 *               |             |                               |
 *               |             |                           (DB ENCR) ----> (DB DECR)
 *               |             |                               |
 *               |         ____v____       _________       ____v____       _________
 *               |        |  prev   |<----|- prev   |<----|- prev   |<----|- prev   |
 *               |        |  next --|---->|  next --|---->|  next --|---->|  next   |
 *               |        |  store  |     |  store  |     |  store  |     |  store  |
 *               |        |____|____|     |____|____|     |____|____|     |____|____|
 *               |             |               |               |               |
 *               `-------------'---------------'---------------'---------------'
 *
 * For the actual implementation of the above design please refer to gtmcrypt_dbk_ref.c.
 */

/* Principal structure for storing key information required to perform encryption / decryption. */
typedef struct gtm_keystore_struct
{
	unsigned char					key[SYMMETRIC_KEY_MAX];		/* Raw symmetric key contents. */
	unsigned char					key_hash[GTMCRYPT_HASH_LEN];	/* SHA-512 hash of symmetric key. */
	char						key_path[GTM_PATH_MAX];		/* Path to the key file. */
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

/* Structure to organize references to the key object by the key path, in a binary search tree fashion. */
typedef struct gtm_keystore_keypath_link_struct
{
	gtm_keystore_t					*link;				/* Link to respective key object. */
	struct gtm_keystore_keypath_link_struct		*left;				/* Pointer to left child. */
	struct gtm_keystore_keypath_link_struct		*right;				/* Pointer to right child. */
} gtm_keystore_keypath_link_t;

/* Structure to organize references to the key object by the key hash, in a binary search tree fashion. */
typedef struct gtm_keystore_hash_link_struct
{
	gtm_keystore_t					*link;				/* Link to respective key object. */
	struct gtm_keystore_hash_link_struct		*left;				/* Pointer to left child. */
	struct gtm_keystore_hash_link_struct		*right;				/* Pointer to right child. */
} gtm_keystore_hash_link_t;

/* Structure to temporarily store key information if the real path of the respective database file name could not be obtained. */
typedef struct gtm_keystore_unres_key_link_struct
{
	char						key_name[GTM_PATH_MAX];		/* Logical entity that the symmetric key
											 * maps to. For databases it is the name of
											 * the database file. For devices it is a
											 * user-chosen string. */
	char						key_path[GTM_PATH_MAX];		/* Path to the key file. */
	int						index;				/* Index in the configuration file */
	int						status;				/* Indication of whether it is for a device,
											 * unresolved, or resolved database. */
	struct gtm_keystore_unres_key_link_struct	*next;				/* Pointer to next element. */
} gtm_keystore_unres_key_link_t;

STATICFNDEF int			keystore_refresh(void);
STATICFNDEF int 		read_files_section(config_t *cfgp);
STATICFNDEF int 		read_database_section(config_t *cfgp);
STATICFNDEF int			gtm_keystore_cleanup_node(gtm_keystore_t *);
int				gtm_keystore_cleanup_all(void);
STATICFNDEF int			gtm_keystore_cleanup_hash_tree(gtm_keystore_hash_link_t *entry);
STATICFNDEF void		gtm_keystore_cleanup_keyname_tree(gtm_keystore_keyname_link_t *entry);
STATICFNDEF void		gtm_keystore_cleanup_keypath_tree(gtm_keystore_keypath_link_t *entry);
STATICFNDEF void		gtm_keystore_cleanup_unres_key_list(void);
int				gtmcrypt_getkey_by_keyname(char *keyname, char *keypath, gtm_keystore_t **entry, int database);
int				gtmcrypt_getkey_by_hash(unsigned char *hash, char *dbpath, gtm_keystore_t **entry);
STATICFNDEF gtm_keystore_t	*gtmcrypt_decrypt_key(char *key_path, int path_length, char *key_name, int name_length);
STATICFNDEF void		insert_unresolved_key_link(char *keyname, char *keypath, int index, int status);
STATICFNDEF gtm_keystore_t	*keystore_lookup_by_hash(unsigned char *hash);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_keyname(char *keyname);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_keyname_plus(char *keyname, char *search_field, int search_type);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_keypath(char *keypath);
STATICFNDEF gtm_keystore_t 	*keystore_lookup_by_unres_key(char *search_field1, int search_field1_type,
				char *search_field2, int search_field2_type, int database, int *error);
int 				keystore_new_cipher_ctx(gtm_keystore_t *entry, char *iv, int length, int action);
int				keystore_remove_cipher_ctx(gtm_cipher_ctx_t *ctx);
STATICFNDEF void		print_debug(void);

#endif /* GTMCRYPT_DBK_REF_H */
