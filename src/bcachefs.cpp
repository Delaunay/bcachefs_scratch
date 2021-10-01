#include "bcachefs.h"
#include "logger.h"

#include <iostream>

#include <cassert>
#include <cstdio>
#include <cstdlib>

// ========================================================================================

BCacheFSReader::BCacheFSReader(String const &file) {
    _file   = fopen(file.c_str(), "rb");
    _sblock = read_superblock();

    // Get the location of the different BTrees
    // bch_sb_field_clean entry is written on clean shutdown
    // it contains the jset_entry that holds the root node of the BTrees
    //
    // The btrees are necessary for us to do everything in BcacheFS
    // so this code belongs here
    debug("Look for superblock field clean");
    // on my example I have the fields below
    // - journal
    // - replicas_v0
    // - clean
    // - last one look like a dummy struct
    auto field = (const SuperBlockFieldClean *)find_superblock_field(BCH_SB_FIELD_clean);
    assert(field != nullptr);

    debug("Look for journal entry");
    // on my example I have the entries below
    // - usage
    // - usage
    // - usage
    // - usage
    // - usage
    // - data_usage
    // - data_usage
    // - deb_usage
    // - clock
    // - clock
    // - btree_root extents
    // - btree_root inodes
    // - btree_root dirents
    // - btree_root alloc
    _btree_roots = find_journal_entries(field);
}

BCacheFSReader::~BCacheFSReader() {
    fclose(_file);
    free(_sblock);
}

// Load the superblock in 2 phases
//  - first phase reads only enough to be able to extract the full size of the superblock
//  - second phase reallocate the superblock using its full size
Superblock *BCacheFSReader::read_superblock() {
    debug(">>> Reading superblock");
    // Read the superblock at 4096 | 0x1000
    auto size = sizeof(Superblock);

    Superblock *block = (Superblock *)malloc(size);
    fseek(_file, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
    fread(block, size, 1, _file);

    // Realloc the superblock now we have all the data to allocate the true size
    assert(memcmp(&block->magic, &BCACHE_MAGIC, sizeof(BCACHE_MAGIC)) == 0);
    size = sizeof(struct bch_sb) + block->u64s * BCH_U64S_SIZE;

    block = (Superblock *)realloc(block, size);
    fseek(_file, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
    fread(block, size, 1, _file);

    debug("<<< Read superblock");
    return block;
}

SuperBlockFieldBase const *BCacheFSReader::find_superblock_field(SuperBlockFieldType type) const {
    auto iter = FieldIterator<SuperBlockFieldBase>((const uint8_t *)_sblock + sizeof(Superblock));
    auto end  = FieldIterator<SuperBlockFieldBase>((const uint8_t *)_sblock + _sblock->u64s * BCH_U64S_SIZE);

    for (; iter != end; ++iter) {
        debug("(size: {}) (type: {}) looking for {}", iter->u64s, iter->type, type);

        if (iter->type == type) {
            return *iter;
        }
    }

    return nullptr;
}

BTreePtr const *BCacheFSReader::find_btree_root(JournalSetEntry const *entry) const {
    // benz_bch_first_bch_val(&jset_entry->start->k, BKEY_U64s);
    auto key = &entry->start->k;

    // auto end   = (BTreePtr const *)((const uint8_t *)key + key->u64s * BCH_U64S_SIZE);
    auto end = FieldIterator<BTreePtr>((const uint8_t *)key + key->u64s * BCH_U64S_SIZE);

    // auto start  = (BTreeValue const *)((const uint8_t *)key + key_u64s * BCH_U64S_SIZE);
    auto key_u64s = BKEY_U64s;
    auto cursor   = FieldIterator<BTreePtr>((const uint8_t *)key + key_u64s * BCH_U64S_SIZE);

    // Look for the value inside this journal entry that holds
    // the btree_root pointer
    // usually it is the first value anyway
    while (cursor != end && *cursor && cursor->start->unused) {
        ++cursor;
    }

    debug("BTree root: (unused: {}) (offset: {})", cursor->start->unused, cursor->start->offset);
    return *cursor;
}

Array<JournalSetEntry const *> BCacheFSReader::find_journal_entries(SuperBlockFieldClean const *field) const {

    auto iter = FieldIterator<JournalSetEntry>((uint8_t const *)field + sizeof(SuperBlockFieldClean));
    auto end  = FieldIterator<JournalSetEntry>((uint8_t const *)field + field->field.u64s * BCH_U64S_SIZE);

    Array<JournalSetEntry const *> out(BTREE_ID_NR);
    auto                           type = BCH_JSET_ENTRY_btree_root;

    for (; iter < end; ++iter) {
        debug("(size: {}) (type: {}) looking for {}", iter->u64s, iter->type, type);

        if (iter->type == type) {
            out[iter->btree_id] = *iter;
        }

        if (iter->u64s <= 0) {
            break;
        }
    }

    return out;
}

BTreeIterator BCacheFSReader::iterator(BTreeType type) const {
    auto entry = _btree_roots[type];

    debug("Look for the btree ptr pointing to the node {} {}", entry->btree_id, type);
    auto btree_ptr = find_btree_root(entry);

    //
    return BTreeIterator(*this, btree_ptr, type);
}

// ========================================================================================
// Iterator
// ----------------------------------------------------------------------------------------

// BKeyITerator
// -------------------------------------------------------------------
BKeyIterator::BKeyIterator(BSet const *bset) {
    assert(bset != nullptr);
    iter = (uint8_t const *)bset + sizeof(BSet);
    end  = (uint8_t const *)(bset + bset->u64s * BCH_U64S_SIZE);
}

BKey const *BKeyIterator::next() {
    if (iter == nullptr || iter == end) {
        return nullptr;
    }

    auto val = *iter;
    ++iter;

    if (val->u64s == 0) {
        return nullptr;
    }

    return val;
}

// Bset
// -------------------------------------------------------------------
BSetIterator::BSetIterator(BTreeNode const *node_, uint64_t size):
    node(node_), iter((uint8_t *)&node->keys), end((uint8_t *)node_ + size) {

    // not really getting what is going on there
    //
    // debug("{} = {} - {} | {}", INT(node + size) - INT(&node->keys), INT(node + size), INT(&node->keys), size);
    // 20971384 = 140454618304528 - 140454597333144 | 131072

    // but this fixes my issue
    iter.current = (uint8_t const *)INT((uint8_t *)&node->keys);
    end.current  = (uint8_t const *)INT((uint8_t *)node + size);

    assert(node != nullptr);
    assert((uint8_t *)node < (uint8_t *)&node->keys);
    assert(iter < end);
}

uint8_t const *BSetIterator::offset(uint64_t block_size) {
    auto *v = *iter;

    const uint8_t *_cb = (const uint8_t *)v;

    _cb -= (uint64_t)node;

    // standard next
    _cb += sizeof(BSet) + v->u64s * BCH_U64S_SIZE;

    _cb += block_size - (uint64_t)_cb % block_size +
           // skip btree_node_entry csum
           sizeof(struct bch_csum);

    _cb += (uint64_t)node;
    return _cb;
}

BSet const *BSetIterator::next(uint64_t block_size) {
    // this does not get executed, why
    if (iter >= end) {
        return nullptr;
    }

    auto val     = *iter;
    iter.current = offset(block_size);

    if (val->u64s == 0) {
        return next(block_size);
    }

    return val;
}

// BTreeIterator
// -------------------------------------------------------------------
BTreeIterator::BTreeIterator(BCacheFSReader const &reader, const BTreePtr *root_ptr, BTreeType type):
    _reader(reader), _type(type), _ptr(root_ptr) {

    debug("load the btree node");
    _iter = load_btree_node(root_ptr);

    assert(_iter);

    // // BCacheFS_iter_next_bset
    _bset_iterator = BSetIterator(_iter.get(), _reader.btree_node_size());
}

BValue const *get_value(BTreeNode const *node, const BKey *key) {
    auto format = node->format;

    uint8_t key_u64s = 0;
    if (key->format == KEY_FORMAT_LOCAL_BTREE) {
        key_u64s = format.key_u64s;
    } else {
        key_u64s = BKEY_U64s;
    }

    return (BValue const *)((uint8_t const *)key + key_u64s * BCH_U64S_SIZE);
}

BKey const *BTreeIterator::_next_key() {
    if (has_children()) {
        BTreeIterator *nodeiter = &_children[_children.size() - 1];
        auto           key      = nodeiter->_next_key();

        // That node is over, get next key
        if (key == nullptr) {
            _children.pop_back();
            return _next_key();
        }

        return key;
    }

    // get next key in the current bset
    auto key = _key_iterator.next();

    if (key != nullptr) {

        // we are pointing to another btree
        if (key->type == KEY_TYPE_btree_ptr_v2) {
            debug("entering a new node");
            auto value = get_value(_iter.get(), (const BKey *)key);
            _children.emplace_back(_reader, (const BTreePtr *)value, _type);
            return _next_key();
        }

        return key;
    }
    debug("fetching next bset");

    // _key == null that means
    //  1. we need to find the first bset
    //  2. we a have reached the end of the previous bset
    auto bset = _bset_iterator.next(_reader.btree_block_size());

    if (bset != nullptr) {
        debug("iterate through a bset: {} {}", INT(bset), bset->u64s);
        _key_iterator = BKeyIterator(bset);
        return _next_key();
    }

    debug("bset is done");
    // _bset is null
    // we finished our current node
    return nullptr;
}

uint64_t benz_uintXX_as_uint64(const uint8_t *bytes, uint8_t sizeof_uint) {
    switch (sizeof_uint) {
    case 64:
        return *(const uint64_t *)(const void *)bytes;
    case 32:
        return *(const uint32_t *)(const void *)bytes;
    case 16:
        return *(const uint16_t *)(const void *)bytes;
    case 8:
        return *(const uint8_t *)bytes;
    }
    return (uint64_t)-1;
}

struct bkey_local parse_bkey(const struct bkey *bkey, const struct bkey_format *format) {
    // clang-format off
    struct bkey_local ret = {
        .u64s = bkey->u64s, 
        .format = bkey->format, 
        .needs_whiteout = bkey->needs_whiteout, 
        .type = bkey->type
    };
    // clang-format on

    uint64_t tmp[6]{0};

    if (bkey->format == KEY_FORMAT_LOCAL_BTREE && memcmp(format, &BKEY_FORMAT_SHORT, sizeof(struct bkey_format)) == 0) {
        debug("1st");
        const struct bkey_short *bkey_short = (const struct bkey_short *)bkey;

        ret.p        = bkey_short->p;
        ret.key_u64s = format->key_u64s;

    } else if (bkey->format == KEY_FORMAT_LOCAL_BTREE &&
               // Does not support field_offset yet
               memcmp(format->field_offset, tmp, sizeof(format->field_offset)) == 0) {
        debug("2nd");
        const uint8_t *bytes = (const uint8_t *)bkey;
        bytes += format->key_u64s * BCH_U64S_SIZE;

        for (int i = 0; i < BKEY_NR_FIELDS; ++i) {
            if (format->bits_per_field[i] == 0) {
                continue;
            }
            bytes -= format->bits_per_field[i] / 8;
            uint64_t value = benz_uintXX_as_uint64(bytes, format->bits_per_field[i]);
            switch (i) {
            case BKEY_FIELD_INODE:
                ret.p.inode = value;
                break;
            case BKEY_FIELD_OFFSET:
                ret.p.offset = value;
                break;
            case BKEY_FIELD_SNAPSHOT:
                ret.p.snapshot = (uint32_t)value;
                break;
            case BKEY_FIELD_SIZE:
                ret.size = (uint32_t)value;
                break;
            case BKEY_FIELD_VERSION_HI:
                ret.version.hi = (uint32_t)value;
                break;
            case BKEY_FIELD_VERSION_LO:
                ret.version.lo = value;
                break;
            }
        }
        ret.key_u64s = format->key_u64s;
    } else if (bkey->format == KEY_FORMAT_CURRENT) {
        debug("3rd");
        memcpy(&ret, bkey, sizeof(*bkey));
        ret.key_u64s = BKEY_U64s;
    } else {
        debug("4th");
    }
    return ret;
}

DirectoryEntry BTreeIterator::directory(BKey const *key) {
    if (!key) {
        error("null key");
        return DirectoryEntry();
    }

    if (key->type != KEY_TYPE_dirent) {
        error("not a directory");
        return DirectoryEntry();
    }

    auto iter  = iterator();
    auto btree = iter._iter.get();
    auto value = (BDirEnt const *)get_value(btree, key);
    auto local = parse_bkey(key, &btree->format);

    return DirectoryEntry{
        .parent_inode = local.p.inode,
        .inode        = value->d_inum,
        .type         = value->d_type,
        .name         = value->d_name,
    };
}

Extend BTreeIterator::extend(BKey const *key) {
    if (!key) {
        error("null key");
        return Extend();
    }

    if (key->type != KEY_TYPE_extent && key->type != KEY_TYPE_inline_data) {
        error("not a directory");
        return Extend();
    }

    auto iter  = iterator();
    auto btree = iter._iter.get();
    auto local = parse_bkey(key, &btree->format);
    auto ext   = Extend{};
    auto val   = get_value(btree, key);

    if (key->type == KEY_TYPE_extent) {
        debug("extend - extend ptr");

        auto value      = (BExtendPtr const *)val;
        ext.file_offset = (key->p.offset - key->size) * BCH_SECTOR_SIZE;
        ext.offset      = value->offset * BCH_SECTOR_SIZE;
        ext.size        = key->size * BCH_SECTOR_SIZE;

    } else if (key->type == KEY_TYPE_inline_data) {
        debug("extend - inline data");
        ext.file_offset = (key->p.offset - key->size) * BCH_SECTOR_SIZE;
        ext.offset      = 0;
        ext.size        = key->u64s * BCH_U64S_SIZE;

        //
        auto     start  = _ptr->start->offset * BCH_SECTOR_SIZE;
        uint64_t offset = (uint64_t)((const uint8_t *)val - (const uint8_t *)btree) + start;

        ext.offset = offset;
        ext.size -= (uint64_t)((const uint8_t *)val - (const uint8_t *)key);
    } else {
    }

    return ext;
}

std::shared_ptr<BTreeNode> BTreeIterator::load_btree_node(BTreePtr const *ptr) {
    auto size       = _reader.btree_node_size();
    auto btree_node = malloc(size);

    uint64_t offset = ptr->start->offset * BCH_SECTOR_SIZE;
    fseek(_reader._file, (long)offset, SEEK_SET);
    fread(btree_node, size, 1, _reader._file);

    return std::shared_ptr<BTreeNode>((BTreeNode *)btree_node, free);
}

BSet const *next(BSet const *iter, uint64_t block_size, BTreeNode const *node) {
    const uint8_t *_cb = (const uint8_t *)iter;

    // Bset is located after the blocksize
    // set it a the beginning of the first block
    _cb -= (uint64_t)node;

    // standard next
    _cb += sizeof(*iter) + iter->u64s * BCH_U64S_SIZE;

    _cb += block_size - (uint64_t)_cb % block_size +
           // skip btree_node_entry csum
           sizeof(struct bch_csum);

    _cb += (uint64_t)node;

    return (BSet const *)_cb;
}
