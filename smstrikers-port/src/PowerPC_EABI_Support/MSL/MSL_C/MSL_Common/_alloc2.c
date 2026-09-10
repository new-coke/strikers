#include "alloc.h"
#include "critical_regions.h"
#include "stddef.h"

#ifndef _MSL_PRO4_MALLOC

/*
 *	alloc.c
 *	
 *	Routines
 *	--------
 *		init_alloc			Initialize allocs' memory pool to be fixed size and location
 *		init_default_heap	initialize the default heap
 *		malloc				Allocate memory
 *		calloc				Allocate and clear memory
 *		realloc				
 *		free				Return memory block to allocs' memory pool
 *	
 *
 */

// #include <pool_alloc.h>
// #include <critical_regions.h>
// #ifndef __MIPS__
// 	#include <inttypes.h>
// #endif
// #include <string.h>
// #include <stdlib.h>

// #define NDEBUG

// #include <assert.h>
// #ifndef NDEBUG
// 	#include <stdio.h>
// #endif

/* These flags are can be switched on/off in anis_prefix.XXX.h
//
//	#define _MSL_MALLOC_0_RETURNS_NON_NULL
//	#define _No_Alloc_OS_Support
*/
/*///////////////////////////////////////////
// Design choices and Platform configuration
////////////////////////////////////////////*/

/*	There are two competing designs herein:  Original Recipe and Extra Crispy.
//	The first design has a lower overhead, but tends to be slower (50%) under
//	allocation distributions from 0 - sys_alloc_size.  If you want the first
//	design, then comment out _MSL_MALLOC_DESIGN_2.  Both designs are derived
//	from Knuth, and the first design is the same as in previous versions of malloc.
*/
#define _MSL_NO_MALLOC_DESIGN_2
#ifndef _MSL_NO_MALLOC_DESIGN_2
	#define _MSL_MALLOC_DESIGN_2
#endif

#if __MIPS__
	typedef size_t mem_size;  /* must be same size as void* */
#else
	// typedef uintptr_t mem_size;  /* must be same size as void* */
	typedef unsigned long mem_size;
#endif

#define WORD_SIZE 4  /* hack!  Must manually set to sizeof(mem_size) */

/*	These two consts describe how malloc goes to the OS for memory.  The shipping
//	configuration is that malloc will ask for at least 64Kb at a time from the OS
//	and if bigger sizes are requested, they will go to the OS in increments of 4Kb.
*/
static const mem_size sys_alloc_size = 65536; /* 64Kb */
static const mem_size page_size      = 4096;  /* 4Kb */

/*	All returned memory will be aligned as indicated. */
#if __ALTIVEC__
	#define alignment 16   /* valid = {4, 8, 16}, must be >= sizeof(void*) */
#elif __MIPS__
	#define alignment __ALIGNMENT__	/*	Must be >= 8	*/
#else
	#define alignment 8   /* valid = {4, 8, 16}, must be >= sizeof(void*) */
#endif

#define align_ratio (alignment / WORD_SIZE)  /* Not configurable */

/*	For tiny allocations, fixed sized pools help significantly speed allocation/deallocation.
//	You can reserve a pool for a small range of sizes.  The use of these pools can be disabled
//	by commenting out _MSL_USE_FIX_MALLOC_POOLS.  Use of fixed size pools requires further
//	configuration below.  The current shipping configuration is:
//  1. Each pool will handle approx. 4000 bytes worth of requests before asking for more memory.
//	2. There are 4 pool types.  Each type is responsible for a different range of requests:
//		a.  0 - 12 bytes
//		b. 13 - 20 bytes
//		c. 21 - 36 bytes
//		d. 37 - 68 bytes
//		Requests for greater than 68 bytes go to the variable size pools.
//	The number of types of pools is configurable below.  The range of requests for each
//	type is also configurable.
*/
#ifndef _MSL_NO_FIX_MALLOC_POOLS
	#define _MSL_USE_FIX_MALLOC_POOLS
#endif

#ifdef _MSL_USE_FIX_MALLOC_POOLS

	static const mem_size fix_pool_alloc_size = 4000;  /* Approx space for each pool. */
#if __ALTIVEC__ || __MIPS__
	static const mem_size max_fix_pool_size = 76;  /* Need the highest poolsize elsewhere, so kludge it up like this */
	static const mem_size fix_pool_sizes[] = {12, 28, 44, max_fix_pool_size};  /* Client space for each pool */
#else
	static const mem_size max_fix_pool_size = 68;  /* Need the highest poolsize elsewhere, so kludge it up like this */
	static const mem_size fix_pool_sizes[] = {12, 20, 36, max_fix_pool_size};  /* Client space for each pool */
#endif
	/* Sizes must be compatible with align_ratio (there is no check).
	// Allowed Size = (N * align_ratio - 1) * sizeof(void*)
	// Assuming sizeof(void*) == 4, then Allowed Sizes include:
	// align_ratio == 1 : {4,  8, 12, 16, 20, 24, ...}
	// align_ratio == 2 : {4, 12, 20, 28, 36, 44, ...}
	// align_ratio == 4 : {4, 12, 28, 44, 60, 76, ...}
	// Assuming sizeof(void*) == 8, then Allowed Sizes include:
	// align_ratio == 1 : {8, 16, 24, 32, 40, 48, ...}
	// align_ratio == 2 : {8, 24, 40, 56, 72, 88, ...}
	*/
	static const mem_size num_fix_pools = sizeof(fix_pool_sizes) / sizeof(mem_size);  /* Not configurable */
#endif

/*//////////////////////////////////////////////
// End Design choices and Platform configuration
///////////////////////////////////////////////*/

typedef struct Block
{
	struct Block* prev_;
	struct Block* next_;
#ifdef _MSL_MALLOC_DESIGN_2
	#if align_ratio == 4
		mem_size reserve1_;
		mem_size reserve2_;
	#endif
	mem_size max_size_;
#endif
	mem_size size_;
/*	...                 // Client space here only
//	mem_size size_;     // Block trailer starts here
//	#ifdef _MSL_MALLOC_DESIGN_2
//		SubBlock* start_;
//	#endif
*/
} Block;

typedef struct SubBlock
{
	mem_size size_;
	#ifdef _MSL_MALLOC_DESIGN_2
		Block* bp_;
	#endif
	struct SubBlock* prev_;  /* Client space starts here */
	struct SubBlock* next_;
/*	...
//	mem_size size_;   // SubBlock Trailer, only used when SubBlock is not allocated
                      // Client space end here */
} SubBlock;

	static const mem_size Block_min_size = sys_alloc_size;
#ifdef _MSL_MALLOC_DESIGN_2
	#if align_ratio == 4
		static const mem_size Block_overhead = 8*sizeof(mem_size);
		static const mem_size Block_header_size = 6*sizeof(mem_size);
	#else
		static const mem_size Block_overhead = 6*sizeof(mem_size);
		static const mem_size Block_header_size = 4*sizeof(mem_size);
	#endif
	static const mem_size Block_trailer_size = 2*sizeof(mem_size);
#else
	static const mem_size Block_overhead = 4*sizeof(mem_size);
	static const mem_size Block_header_size = 3*sizeof(mem_size);
	static const mem_size Block_trailer_size = 1*sizeof(mem_size);
#endif

	static void Block_construct(Block* ths, mem_size size);
#ifdef _MSL_MALLOC_DESIGN_2
	static SubBlock* Block_subBlock(Block* ths, mem_size size);
	static void Block_link(Block* ths, SubBlock* sb);
	static void Block_unlink(Block* ths, SubBlock* sb);
#else
	static SubBlock* Block_subBlock(mem_size size);
	static void Block_link(SubBlock* sb);
	static void Block_unlink(SubBlock* sb);
#endif
#ifndef NDEBUG
	static void Block_report(Block* ths, int verbose);
#endif

#ifndef _MSL_MALLOC_DESIGN_2
	static SubBlock* Block_sub_start_;
#endif

#ifdef _MSL_MALLOC_DESIGN_2
	static const mem_size SubBlock_overhead = 2*sizeof(mem_size);
#else
	static const mem_size SubBlock_overhead = 1*sizeof(mem_size);
#endif
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	static const mem_size SubBlock_min_size = SubBlock_overhead + max_fix_pool_size + 1 + (alignment-1) & ~(alignment-1);
#else
	static const mem_size SubBlock_min_size = SubBlock_overhead + 3*sizeof(mem_size) + (alignment-1) & ~(alignment-1);
#endif
	static const mem_size SubBlock_header_size = SubBlock_overhead;

#ifdef _MSL_MALLOC_DESIGN_2
	static void SubBlock_construct(SubBlock* ths, mem_size size, Block* bp, int prev_alloc, int this_alloc);
#else
	static void SubBlock_construct(SubBlock* ths, mem_size size, int prev_alloc, int this_alloc);
#endif
	static SubBlock* SubBlock_split(SubBlock* ths, mem_size size);
	static SubBlock* SubBlock_merge_prev(SubBlock* ths, SubBlock** start);
	static void SubBlock_merge_next(SubBlock* ths, SubBlock** start);
#ifndef NDEBUG
	static void SubBlock_report(SubBlock* ths, int verbose);
#endif

/* Inline Utilities */

/* Note, there is a bit of information currently unused when alignment == 16 (0x80) */

#if WORD_SIZE == 4
	#if alignment == 4
		#define size_flag 0x7FFFFFFC
	#elif alignment == 8
		#define size_flag 0xFFFFFFF8
	#elif alignment == 16
		#define size_flag 0xFFFFFFF0
	#else
		#error Unsupported Alignment
	#endif
#elif WORD_SIZE == 8
	#if alignment == 8
		#define size_flag 0xFFFFFFFFFFFFFFF8
	#elif alignment == 16
		#define size_flag 0xFFFFFFFFFFFFFFF0
	#else
		#error Unsupported Alignment
	#endif
#else
	#error Unsupported word size in alloc.c
#endif

#define fix_var_flag 0x01

#define this_alloc_flag 0x02

#if alignment == 4
	#define prev_alloc_flag 0x40000000
#else
	#define prev_alloc_flag 0x04
#endif

#define align(size, algn) (size + (algn-1) & ~(algn-1))

#ifdef _MSL_USE_FIX_MALLOC_POOLS

	/* return true for SubBlock, false for FixSubBlock */
	#define classify(ptr) (*(mem_size*)((char*)(ptr) - sizeof(mem_size)) & fix_var_flag)

#endif

/* SubBlock inlines */

#define SubBlock_size(ths) ((ths)->size_ & size_flag)

#ifndef _MSL_MALLOC_DESIGN_2

	#define SubBlock_only(ths) ((!((ths)->size_ & prev_alloc_flag) && \
		*(mem_size*)((char*)(ths) - sizeof(mem_size)) & this_alloc_flag) && \
		SubBlock_size((ths)) + Block_overhead == (*(mem_size*)((char*)(ths) - sizeof(mem_size)) & size_flag))

#endif

#ifdef _MSL_MALLOC_DESIGN_2
	#define SubBlock_block(ths) ((Block*)((mem_size)((ths)->bp_) & ~fix_var_flag))
#else
	#define SubBlock_block(ths) ((Block*)((char*)(ths) - Block_header_size))  /* only works if this is first SubBlock! */
#endif

#define SubBlock_client_space(ths) ((char*)(ths) + SubBlock_header_size)

#define SubBlock_set_free(ths)                                               \
	mem_size this_size = SubBlock_size((ths));                               \
	(ths)->size_ &= ~this_alloc_flag;                                        \
	*(mem_size*)((char*)(ths) + this_size) &= ~prev_alloc_flag;              \
	*(mem_size*)((char*)(ths) + this_size - sizeof(mem_size)) = this_size

#define SubBlock_set_not_free(ths)                              \
	mem_size this_size = SubBlock_size((ths));                  \
	(ths)->size_ |= this_alloc_flag;                            \
	*(mem_size*)((char*)(ths) + this_size) |= prev_alloc_flag

#define SubBlock_is_free(ths) !((ths)->size_ & this_alloc_flag)

#define SubBlock_set_size(ths, sz)                                 \
	(ths)->size_ &= ~size_flag;                                    \
	(ths)->size_ |= (sz) & size_flag;                              \
	if (SubBlock_is_free((ths)))                                   \
		*(mem_size*)((char*)(ths) + (sz) - sizeof(mem_size)) = (sz)

#define SubBlock_from_pointer(ptr) ((SubBlock*)((char*)(ptr) - SubBlock_header_size))

/* Block inlines */

#ifdef _MSL_MALLOC_DESIGN_2

	#define Block_max_possible_size(ths) ((ths)->max_size_)

#endif

#define Block_size(ths) ((ths)->size_ & size_flag)

#ifdef _MSL_MALLOC_DESIGN_2
	#define Block_start(ths) (*(SubBlock**)((char*)(ths) + Block_size((ths)) - sizeof(mem_size)))
#else
	#define Block_start Block_sub_start_
#endif

#define Block_empty(ths)                                                      \
	(_sb = (SubBlock*)((char*)(ths) + Block_header_size)),                    \
	SubBlock_is_free(_sb) && SubBlock_size(_sb) == Block_size((ths)) - Block_overhead

/* Block Implementation */

static
void
Block_construct(Block* ths, mem_size size)
{
	SubBlock* sb;

	ths->size_ = size | this_alloc_flag | fix_var_flag;
	*(mem_size*)((char*)ths + size - Block_trailer_size) = ths->size_;
	sb = (SubBlock*)((char*)ths + Block_header_size);
#ifdef _MSL_MALLOC_DESIGN_2
	SubBlock_construct(sb, size - Block_overhead, ths, 0, 0);
	ths->max_size_ = size - Block_overhead;
	Block_start(ths) = 0;
	Block_link(ths, sb);
#else
	SubBlock_construct(sb, size - Block_overhead, 0, 0);
	Block_link(sb);
#endif
}

static
SubBlock*
#ifdef _MSL_MALLOC_DESIGN_2
	Block_subBlock(Block* ths, mem_size size)
#else
	Block_subBlock(mem_size size)
#endif
{
	SubBlock* st;
	SubBlock* sb;
	mem_size sb_size;
#ifdef _MSL_MALLOC_DESIGN_2
	mem_size max_found;
#endif

#ifdef _MSL_MALLOC_DESIGN_2
	st = Block_start(ths);
#else
	st = Block_start;
#endif
	if (st == 0)
		return 0;
	sb = st;
	sb_size = SubBlock_size(sb);
#ifdef _MSL_MALLOC_DESIGN_2
	max_found = sb_size;
#endif
	while (sb_size < size)
	{
		sb = sb->next_;
		sb_size = SubBlock_size(sb);
	#ifdef _MSL_MALLOC_DESIGN_2
		if (max_found < sb_size)
			max_found = sb_size;
	#endif
		if (sb == st)
		{
		#ifdef _MSL_MALLOC_DESIGN_2
			ths->max_size_ = max_found;
		#endif
			return 0;
		}
	}
	if (sb_size - size >= SubBlock_min_size)
		SubBlock_split(sb, size);
#ifdef _MSL_MALLOC_DESIGN_2
	Block_start(ths) = sb->next_;
	Block_unlink(ths, sb);
#else
	Block_start = sb->next_;
	Block_unlink(sb);
#endif
	return sb;
}

static
void
#ifdef _MSL_MALLOC_DESIGN_2
	Block_link(Block* ths, SubBlock* sb)
#else
	Block_link(SubBlock* sb)
#endif
{
	SubBlock** st;

	SubBlock_set_free(sb);
#ifdef _MSL_MALLOC_DESIGN_2
	st = &Block_start(ths);
#else
	st = &Block_start;
#endif
	if (*st != 0)
	{
		sb->prev_ = (*st)->prev_;
		sb->prev_->next_ = sb;
		sb->next_ = *st;
		(*st)->prev_ = sb;
		*st = sb;
		*st = SubBlock_merge_prev(*st, st);
		SubBlock_merge_next(*st, st);
	}
	else
	{
		*st = sb;
		sb->prev_ = sb;
		sb->next_ = sb;
	}
	#ifdef _MSL_MALLOC_DESIGN_2
		if (ths->max_size_ < SubBlock_size(*st))
			ths->max_size_ = SubBlock_size(*st);
	#endif
}

static
void
#ifdef _MSL_MALLOC_DESIGN_2
	Block_unlink(Block* ths, SubBlock* sb)
#else
	Block_unlink(SubBlock* sb)
#endif
{
	SubBlock** st;

	SubBlock_set_not_free(sb);
#ifdef _MSL_MALLOC_DESIGN_2
	st = &Block_start(ths);
#else
	st = &Block_start;
#endif
	if (*st == sb)
		*st = sb->next_;
	if (*st == sb)
	{
		*st = 0;
		#ifdef _MSL_MALLOC_DESIGN_2
			ths->max_size_ = 0;
		#endif
	}
	else
	{
		sb->next_->prev_ = sb->prev_;
		sb->prev_->next_ = sb->next_;
	}
}

#ifndef NDEBUG

	static
	void
	Block_report(Block* ths, int verbose)
	{
		SubBlock* st;
		SubBlock* end;
		int i;
		int prev_free;
	#ifdef _MSL_MALLOC_DESIGN_2
		SubBlock* sb;
	#endif

		st = (SubBlock*)((char*)ths + Block_header_size);
		end = (SubBlock*)((char*)ths + Block_size(ths) - Block_trailer_size);
		i = 1;
		if (verbose)
		#ifdef _MSL_MALLOC_DESIGN_2
			printf("\tsize_ = %d, max_size = %d\n", Block_size(ths), Block_max_possible_size(ths));
		#else
			printf("\tsize_ = %d\n", Block_size(ths));
		#endif
		if (Block_size(ths) > 1042*1024)
		{
			printf("\t**ERROR** Block size suspiciously large %d\n", Block_size(ths));
			exit(1);
		}
		prev_free = 0;
		do
		{
			if (SubBlock_is_free(st))
			{
				if (verbose)
					printf("\tSubBlock %d at %p\n", i, st);
				if (prev_free)
				{
					printf("\t**ERROR** SubBlock failed to merge with previous\n");
					exit(1);
				}
				SubBlock_report(st, verbose);
				prev_free = 1;
			}
			else
			{
				SubBlock_report(st, verbose);
				prev_free = 0;
			}
			st = (SubBlock*)((char*)st + SubBlock_size(st));
			++i;
		} while (st != end);
		#ifdef _MSL_MALLOC_DESIGN_2
			sb = Block_start(ths);
			if (sb != 0)
			{
				do
				{
					if (SubBlock_size(sb) > 1042*1024)
					{
						printf("\t**ERROR** SubBlock size suspiciously large %d\n", SubBlock_size(sb));
						exit(1);
					}
					sb = sb->next_;
				} while (sb != Block_start(ths));
				sb = Block_start(ths);
				do
				{
					if (SubBlock_size(sb) > 1042*1024)
					{
						printf("\t**ERROR** SubBlock size suspiciously large %d\n", SubBlock_size(sb));
						exit(1);
					}
					sb = sb->prev_;
				} while (sb != Block_start(ths));
			}
		#endif
	}

#endif

/* SubBlock Implementation */

static
void
#ifdef _MSL_MALLOC_DESIGN_2
	SubBlock_construct(SubBlock* ths, mem_size size, Block* bp, int prev_alloc, int this_alloc)
#else
	SubBlock_construct(SubBlock* ths, mem_size size, int prev_alloc, int this_alloc)
#endif
{
#ifdef _MSL_MALLOC_DESIGN_2
	ths->bp_ = (Block*)((mem_size)bp | fix_var_flag);
	ths->size_ = size;
#else
	ths->size_ = size | fix_var_flag;
#endif
	if (prev_alloc)
		ths->size_ |= prev_alloc_flag;
	if (this_alloc)
	{
		ths->size_ |= this_alloc_flag;
		*(mem_size*)((char*)ths + size) |= prev_alloc_flag;
	}
	else
		*(mem_size*)((char*)ths + size - sizeof(mem_size)) = size;
}

static
SubBlock*
SubBlock_split(SubBlock* ths, mem_size sz)
{
	mem_size origsize;
	int isfree;
	int isprevalloc;
	SubBlock* np;
#ifdef _MSL_MALLOC_DESIGN_2
	Block* bp;
#endif

	origsize = SubBlock_size(ths);
	isfree = SubBlock_is_free(ths);
	isprevalloc = (int)(ths->size_ & prev_alloc_flag);
	np = (SubBlock*)((char*)ths + sz);
#ifdef _MSL_MALLOC_DESIGN_2
	bp = SubBlock_block(ths);
	
	SubBlock_construct(ths, sz, bp, isprevalloc, !isfree);
	SubBlock_construct(np, origsize - sz, bp, !isfree, !isfree);
#else
	SubBlock_construct(ths, sz, isprevalloc, !isfree);
	SubBlock_construct(np, origsize - sz, !isfree, !isfree);
	#endif
	if (isfree)
	{
		np->next_ = ths->next_;
		np->next_->prev_ = np;
		np->prev_ = ths;
		ths->next_ = np;
	}
	return np;
}

static
SubBlock*
SubBlock_merge_prev(SubBlock* ths, SubBlock** start)
{
	mem_size prevsz;
	SubBlock* p;

	if (!(ths->size_ & prev_alloc_flag))
	{
		prevsz = *(mem_size*)((char*)ths - sizeof(mem_size));
		if (prevsz & this_alloc_flag)  /* Is it the block header? */
			return ths;
		p = (SubBlock*)((char*)ths - prevsz);
		SubBlock_set_size(p, prevsz + SubBlock_size(ths));
		/* unlink ths */
		if (*start == ths)
			*start = (*start)->next_;
		ths->next_->prev_ = ths->prev_;
		ths->next_->prev_->next_ = ths->next_;
		return p;
	}
	return ths;
}

static
void
SubBlock_merge_next(SubBlock* ths, SubBlock** start)
{
	SubBlock* next_sub;
	mem_size this_size;

	next_sub = (SubBlock*)((char*)ths + SubBlock_size(ths));
	if (SubBlock_is_free(next_sub))
	{
		this_size = SubBlock_size(ths) + SubBlock_size(next_sub);
		SubBlock_set_size(ths, this_size);
		if (SubBlock_is_free(ths))
			*(mem_size*)((char*)ths + this_size) &= ~prev_alloc_flag;
		else
			*(mem_size*)((char*)ths + this_size) |= prev_alloc_flag;
		/* unlink next_sub */
		if (*start == next_sub)
			*start = (*start)->next_;
		if (*start == next_sub)
			*start = 0;
		next_sub->next_->prev_ = next_sub->prev_;
		next_sub->prev_->next_ = next_sub->next_;
	}
}

#ifndef NDEBUG

	static
	void
	SubBlock_report(SubBlock* ths, int verbose)
	{
		if (verbose)
		#ifdef _MSL_MALLOC_DESIGN_2
			printf("\t\tsize_ = %d, bp_ = %p\n", SubBlock_size(ths), SubBlock_block(ths));
		#else
			printf("\t\tsize_ = %d\n", SubBlock_size(ths));
		#endif
		if (SubBlock_size(ths) > 1042*1024)
		{
			printf("\t**ERROR** SubBlock size suspiciously large %d\n", SubBlock_size(ths));
			exit(1);
		}
	}

#endif

static Block* start_;

static
void
link(Block* bp)
{
	if (start_ != 0)
	{
		bp->prev_ = start_->prev_;
		bp->prev_->next_ = bp;
		bp->next_ = start_;
		start_->prev_ = bp;
		start_ = bp;
	}
	else
	{
		start_ = bp;
		bp->prev_ = bp;
		bp->next_ = bp;
	}
}

static
Block*
unlink(Block* bp)
{
	Block* result = bp->next_;
	if (result == bp)
		result = 0;
	if (start_ == bp)
		start_ = result;
	if (result != 0)
	{
		result->prev_ = bp->prev_;
		result->prev_->next_ = result;
	}
	bp->next_ = 0;
	bp->prev_ = 0;
	return result;
}

static
Block*
link_new_block(mem_size size)
{
	Block* bp;

	size += Block_overhead;
	size = align(size, page_size);
	if (size < Block_min_size)
		size = Block_min_size;
	bp = (Block*)__sys_alloc(size);
	if (bp == 0)
		return 0;
	Block_construct(bp, size);
	link(bp);
	return bp;
}

static
void*
allocate_from_var_pools(mem_size size)
{
	Block* bp;
	SubBlock* ptr;

	size += SubBlock_overhead;
	size = align(size, alignment);
	if (size < SubBlock_min_size)
		size = SubBlock_min_size;
	bp = start_ != 0 ? start_ : link_new_block(size);
	if (bp == 0)
		return 0;
#ifdef _MSL_MALLOC_DESIGN_2
	do
	{
		if (size <= Block_max_possible_size(bp))
		{
			ptr = Block_subBlock(bp, size);
			if (ptr != 0)
			{
				start_ = bp;
				break;
			}
		}
		bp = bp->next_;
		if (bp == start_)
		{
			bp = link_new_block(size);
			if (bp == 0)
				return 0;
			ptr = Block_subBlock(bp, size);
			break;
		}
	} while (1);
#else
	ptr = Block_subBlock(size);
	if (ptr == 0)
	{
		bp = link_new_block(size);
		if (bp == 0)
			return 0;
		ptr = Block_subBlock(size);
	}
#endif
	return SubBlock_client_space(ptr);
}

	static
	void
	deallocate_from_var_pools(void* ptr)
	{
		SubBlock* sb = SubBlock_from_pointer(ptr);
	#ifdef _MSL_MALLOC_DESIGN_2
		SubBlock* _sb;
		Block* bp = SubBlock_block(sb);
	#else
		Block* bp = start_;
	#endif
#ifdef _MSL_MALLOC_DESIGN_2
		Block_link(bp, sb);
	#ifndef _No_Alloc_OS_Support
		if (Block_empty(bp))
		{
	#endif
#else
		Block_link(sb);
	#ifndef _No_Alloc_OS_Support
		sb = Block_start;
		if (SubBlock_only(sb))
		{                       /* } */
			bp = SubBlock_block(sb);
			Block_unlink(sb);
	#endif
#endif
	#ifndef _No_Alloc_OS_Support
			unlink(bp);
			__sys_free(bp);
		}
	#endif
	}

#ifdef _MSL_USE_FIX_MALLOC_POOLS

	typedef struct FixBlock
	{
		struct FixBlock* reserve_;
		struct FixBlock* next_;
		mem_size client_size_;
	} FixBlock;

	typedef struct FixSubBlock
	{
		FixBlock* block_;
		struct FixSubBlock* next_;  /* Client space starts here */
	} FixSubBlock;

	static const mem_size FixBlock_overhead = 3*sizeof(mem_size);
	static const mem_size FixBlock_header_size = FixBlock_overhead;

	static void FixBlock_construct(FixBlock* ths, FixBlock* prev, FixBlock* next, mem_size index,
		FixSubBlock* chunk, mem_size chunk_size);

	static const mem_size FixSubBlock_overhead = sizeof(mem_size);
	static const mem_size FixSubBlock_header_size = FixSubBlock_overhead;

	typedef struct FixStart
	{
		FixBlock* next_;
		FixSubBlock* start_;
		mem_size n_allocated_;
	} FixStart;
	
	FixStart fix_start[num_fix_pools];

	/* FixBlock Implementation */
	
	#define FixBlock_client_size(ths) ((ths)->client_size_)

	/* FixSubBlock Implementation */
	
	#define FixSubBlock_construct(ths, block, next) (((FixSubBlock*)(ths))->block_ = block, ((FixSubBlock*)(ths))->next_ = next)

	#define FixSubBlock_client_space(ths) ((char*)(ths) + FixSubBlock_header_size)

	#define FixSubBlock_size(ths) (FixBlock_client_size((ths)->block_))

	#define FixSubBlock_from_pointer(ptr) ((FixSubBlock*)((char*)(ptr) - FixSubBlock_header_size))

	/* FixBlock Implementation */
	
	static
	void
	FixBlock_construct(FixBlock* ths, FixBlock* prev, FixBlock* next, mem_size index,
		FixSubBlock* chunk, mem_size chunk_size)
	{
		mem_size fixSubBlock_size;
		mem_size n;
		char* p;
		mem_size i;
		char* np;

		ths->reserve_ = prev;
		ths->next_ = next;
		ths->client_size_ = fix_pool_sizes[index];
		fixSubBlock_size = fix_pool_sizes[index] + FixSubBlock_overhead;
		n = chunk_size / fixSubBlock_size;
		p = (char*)chunk;
		for (i = 0; i < n-1; ++i)
		{
			np = p + fixSubBlock_size;
			FixSubBlock_construct(p, ths, (FixSubBlock*)np);
			p = np;
		}
		FixSubBlock_construct(p, ths, fix_start[index].start_);
		fix_start[index].start_ = chunk;
	}

#endif /* _MSL_USE_FIX_MALLOC_POOLS */

#ifdef _MSL_USE_FIX_MALLOC_POOLS
	#define __msize_inline(ptr) (!classify(ptr) ? FixSubBlock_size(FixSubBlock_from_pointer(ptr)) : SubBlock_size(SubBlock_from_pointer(ptr)) - SubBlock_overhead)
#else
	#define __msize_inline(ptr) (SubBlock_size(SubBlock_from_pointer(ptr)) - SubBlock_overhead)
#endif

#ifdef _MSL_USE_FIX_MALLOC_POOLS

	void* allocate_from_fixed_pools(mem_size size);
	void*
	allocate_from_fixed_pools(mem_size size)
	{
		mem_size i = 0;
		FixSubBlock* p;

		while (size > fix_pool_sizes[i])
			++i;
		if (fix_start[i].start_ == 0)
		{
			char* newblock = (char*)allocate_from_var_pools(fix_pool_alloc_size);
			mem_size size_received;

			if (newblock == 0)
				return 0;
			size_received = __msize_inline(newblock);
			FixBlock_construct((FixBlock*)newblock, 0, fix_start[i].next_, i,
				(FixSubBlock*)(newblock + FixBlock_overhead), size_received - FixBlock_overhead);
			fix_start[i].next_ = (FixBlock*)newblock;
		}
		p = fix_start[i].start_;
		fix_start[i].start_ = p->next_;
		++fix_start[i].n_allocated_;
		return FixSubBlock_client_space(p);
	}

	void deallocate_from_fixed_pools(void* ptr, mem_size size);
	void
	deallocate_from_fixed_pools(void* ptr, mem_size size)
	{
		mem_size i = 0;
		FixSubBlock* p;

		while (size > fix_pool_sizes[i])
			++i;
		p = FixSubBlock_from_pointer(ptr);
		p->next_ = fix_start[i].start_;
		fix_start[i].start_ = p;
		if (--fix_start[i].n_allocated_ == 0)
		{
			FixBlock* bp = fix_start[i].next_;
			while (bp != 0)
			{
				FixBlock* bpn = bp->next_;
				deallocate_from_var_pools(bp);
				bp = bpn;
			}
			fix_start[i].next_ = 0;
			fix_start[i].start_ = 0;
		}
	}

#endif

#ifndef NDEBUG

	void __report_on_heap(int verbose);
	void
	__report_on_heap(int verbose)
	{
		Block* bp;
		int i;
	#ifndef _MSL_MALLOC_DESIGN_2
		SubBlock* sb;
	#endif

		if (verbose)
			printf("\n-------------------------\n");
		bp = start_;
		if (bp == 0)
		{
			if (verbose)
				printf("\nNothing to report\n");
			return;
		}
		i = 1;
		do
		{
			if (verbose)
				printf("\nBlock %d at %p\n", i, bp);
			Block_report(bp, verbose);
			bp = bp->next_;
			++i;
		} while (bp != start_);
	#ifndef _MSL_MALLOC_DESIGN_2
		sb = Block_start;
		if (sb != 0)
		{
			do
			{
				if (SubBlock_size(sb) > 1042*1024)
				{
					printf("\t**ERROR** SubBlock size suspiciously large %d\n", SubBlock_size(sb));
					exit(1);
				}
				sb = sb->next_;
			} while (sb != Block_start);
			sb = Block_start;
			do
			{
				if (SubBlock_size(sb) > 1042*1024)
				{
					printf("\t**ERROR** SubBlock size suspiciously large %d\n", SubBlock_size(sb));
					exit(1);
				}
				sb = sb->prev_;
			} while (sb != Block_start);
			}
		#endif
	}

#endif

// #define __msize_inline(ptr)                                                                        \
//     (!classify(ptr) ? FixSubBlock_size(FixSubBlock_from_pointer(ptr)) :                            \
//                       SubBlock_size(SubBlock_from_pointer(ptr)) - 8)
size_t
__msize(void* ptr)
{
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	if (!classify(ptr))
		return FixSubBlock_size(FixSubBlock_from_pointer(ptr));
#endif
	return SubBlock_size(SubBlock_from_pointer(ptr)) - SubBlock_overhead;
}

#ifndef __MALLOC
#define __MALLOC malloc
#endif

#ifndef __REALLOC
#define __REALLOC realloc
#endif

#ifndef __CALLOC
#define __CALLOC calloc
#endif

#ifndef __FREE
#define __FREE free
#endif

#ifdef _No_Alloc_OS_Support
	static int defaultheapinitialized = 0;
	void init_default_heap(void);
#endif

void*
__MALLOC(size_t size)
{
	void* result;

	assert(sizeof(mem_size) == sizeof(void*));
	assert(align_ratio >= 1);

	if (size == 0)
	#ifdef _MSL_MALLOC_0_RETURNS_NON_NULL
		++size;
	#else
		return 0;
	#endif
	__begin_critical_region(malloc_pool_access);
#ifdef _No_Alloc_OS_Support
	if (!defaultheapinitialized)
		init_default_heap();
#endif
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	if (size <= max_fix_pool_size)
		result = allocate_from_fixed_pools(size);
	else
#endif
		result = allocate_from_var_pools(size);
	__end_critical_region(malloc_pool_access);
	return result;
}

void
__FREE(void* ptr)
{
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	mem_size size;
#endif

	if (ptr == 0)
		return;
	__begin_critical_region(malloc_pool_access);
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	size = __msize_inline(ptr);
	if (size <= max_fix_pool_size)
		deallocate_from_fixed_pools(ptr, size);
	else
#endif
		deallocate_from_var_pools(ptr);
	__end_critical_region(malloc_pool_access);
}

void*
__REALLOC(void* ptr, size_t size)
{
	mem_size current_size;
	mem_size sz;
	SubBlock* sb;
	void* newptr;

	if (ptr == 0)
		return __MALLOC(size);
	if (size == 0)
	{
		__FREE(ptr);
		return 0;
	}
	current_size = __msize_inline(ptr);
	if (size > current_size)
	{
	#ifdef _MSL_USE_FIX_MALLOC_POOLS
		if (classify(ptr)) /* is var_block */
		{
	#endif
			sz = size + SubBlock_overhead;
			sz = align(sz, alignment);
			if (sz < SubBlock_min_size)
				sz = SubBlock_min_size;
			sb = SubBlock_from_pointer(ptr);
			__begin_critical_region(malloc_pool_access);
		#ifdef _MSL_MALLOC_DESIGN_2
			SubBlock_merge_next(sb, &Block_start(SubBlock_block(sb)));
		#else
			SubBlock_merge_next(sb, &Block_start);
		#endif
			if (SubBlock_size(sb) >= sz)
			{
				if (SubBlock_size(sb) - sz >= SubBlock_min_size)
				#ifdef _MSL_MALLOC_DESIGN_2
					Block_link(SubBlock_block(sb), SubBlock_split(sb, sz));
				#else
					Block_link(SubBlock_split(sb, sz));
				#endif
				__end_critical_region(malloc_pool_access);
				return ptr;
			}
			__end_critical_region(malloc_pool_access);
	#ifdef _MSL_USE_FIX_MALLOC_POOLS
		}
	#endif
		newptr = __MALLOC(size);
		if (newptr == 0)
			return 0;
		memcpy(newptr, ptr, current_size);
		__FREE(ptr);
		return newptr;
	}
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	if (classify(ptr)) /* is var_block */
	{
#endif
		size += SubBlock_overhead;
		size = align(size, alignment);
		if (size < SubBlock_min_size)
			size = SubBlock_min_size;
		sb = SubBlock_from_pointer(ptr);
		if (SubBlock_size(sb) - size >= SubBlock_min_size)
		{
			__begin_critical_region(malloc_pool_access);
		#ifdef _MSL_MALLOC_DESIGN_2
			Block_link(SubBlock_block(sb), SubBlock_split(sb, size));
		#else
			Block_link(SubBlock_split(sb, size));
		#endif
			__end_critical_region(malloc_pool_access);
		}
#ifdef _MSL_USE_FIX_MALLOC_POOLS
	}
#endif
	return ptr;
}

void*
__CALLOC(size_t nmemb, size_t size)
{
	void* result;

	size *= nmemb;
	result = __MALLOC(size);
	if (result != 0)
		memset(result, 0, size);
	return result;
}

#ifndef _No_Alloc_OS_Support

	void
	__pool_free_all()
	{
		Block* bp = start_;
		Block* bpn;
		mem_size i;

		if (bp == 0)
			return;
		do
		{
			bpn = bp->next_;
			__sys_free(bp);
			bp = bpn;
		} while (bp != start_);
		start_ = 0;
	#ifdef _MSL_USE_FIX_MALLOC_POOLS
		for (i = 0; i < num_fix_pools; ++i)
		{
			fix_start[i].next_ = 0;
			fix_start[i].start_ = 0;
			fix_start[i].n_allocated_ = 0;
		}
	#endif
	}

#endif

#ifdef _No_Alloc_OS_Support

	/***************************************************************************
	 *	init_alloc
	 *		Initializes the memory pool that malloc uses. This call is designed to
	 *		be used for systems that DO NOT have memory allocation functions
	 *  	built into the system. The first call will define the pool and use the
	 *		rest of the space as a block of memory. Subsequent calls will add the
	 *		memory as another block of memory to the malloc's existing pool. Once
	 *		a heap of memory is allocated to the malloc memory pool, it can not 
	 *		be returned.
	 *
	 *	Inputs:
	 *		pool_ptr		pointer to memory to be used in malloc's memory pool
	 *		size				size of memory to be used in (or added to) malloc's memory pool
	 *
	 *	Outputs:
	 *		None
	 *
	 *	Returns: int
	 *		0		Fail
	 *		1		Success: Memory initialized as malloc's memory pool
	 *		2		Success: Memory added to initial heap successfully
	 *
	 *  NOTE: 
	 *		This function must be called prior to using other alloc.c functions
	 *
	 ***************************************************************************/
	int
	init_alloc(void* heap_ptr, size_t heap_size)
	{
		mem_size size;
		Block* bp;
		int result;

		assert((mem_size)heap_ptr % alignment == 0);

		if (heap_ptr == 0)
			return 0;
		size = align(heap_size, alignment);
		if (size > heap_size)
			size -= alignment;
		if (size < Block_overhead + SubBlock_min_size)
			return 0;
		result = start_ == 0 ? 1 : 2;
		bp = (Block*)heap_ptr;
		Block_construct(bp, size);
		link(bp);
		return result;
	}

	#if __dest_os	== __ppc_eabi  || __dest_os ==__dolphin_os

	__declspec(section ".init") extern char		_heap_addr[];	/* starting address for heap */
	__declspec(section ".init") extern char 	_heap_end[];	/* address after end byte of heap */

	#elif __dest_os == __nec_eabi_bare

		extern unsigned int _heap_size;			/* size of the stack (PPC EABI pref panel) */
		extern unsigned int _heap_addr;			/* starting address for heap */
		
	#elif __dest_os == __mcore_bare
		
		extern unsigned int _heap_size;			/* size of the stack (mCore EABI pref panel) */
		extern unsigned int _heap_addr;			/* starting address for heap */
		extern unsigned int __heap_size;   		/*_ = (unsigned int)&_heap_size;*/
		extern unsigned int __heap_addr;  		/* = (unsigned int)&_heap_addr;*/
		/* unsigned int _heap_end;	*/			/* si 010899 Linker has no _heap_end		 */ 

	#elif defined(__m56800__)		/* mm 981029 */

		extern unsigned int _heap_size;			/* size of the stack (PPC EABI pref panel) */
		extern unsigned int _heap_addr;			/* starting address for heap */
		extern unsigned int _heap_end;			/* starting address for heap */

	#elif __dest_os == __mips_bare
		extern char	_heap_addr[];				/* starting address for heap */
		extern char _heap_end[];				/* address after end byte of heap */

	#elif __dest_os == __n64_os					/* ad 01.28.99, added nintendo support */
		extern char _heap_addr[];
		extern char _heap_end[];

	#else
		#error
	#endif  /* __dest_os	== __ppc_eabi */

	void init_default_heap(void)
	{
		defaultheapinitialized = 1;
		#if (__dest_os == __mcore_bare) || (__dest_os == __nec_eabi_bare)
			if (__heap_size!= 0)
				init_alloc((void *)__heap_addr, __heap_size);
		#else
			if (_heap_end - _heap_addr != 0)
				init_alloc((void *)_heap_addr, (_heap_end - _heap_addr));
		#endif
	}

	void*
	__sys_alloc(size_t x)
	{
	#pragma unused(x)
		return 0;
	}

#endif

#if __ALTIVEC__

	void* vec_malloc(size_t size);
	void* vec_calloc(size_t nmemb, size_t size);
	void* vec_realloc(void* ptr, size_t size);
	void vec_free(void* ptr);

	void*
	vec_malloc(size_t size)
	{
		return malloc(size);
	}

	void*
	vec_calloc(size_t nmemb, size_t size)
	{
		return calloc(nmemb, size);
	}

	void*
	vec_realloc(void* ptr, size_t size)
	{
		return realloc(ptr, size);
	}

	void
	vec_free(void* ptr)
	{
		free(ptr);
	}

#endif

#endif /* __dest_os != __be_os */               /* mm 970708 */

/*     Change record
 *	20-Mar-95 JFH  First code release.
 *  09-Jun-95 JFH  Changed malloc_pool to __malloc_pool as per standard naming conventions.
 * mm 970708  Inserted Be changes
 *	20-Jul-97 MEA  changed __no_os to _No_Alloc_OS_Support and  __ppc_eabi_bare to __ppc_eabi.
 * mm 970904  Added include ansi_parms.h  to allow compilation without prefix
 * mm 981015  Added code for _No_Alloc_OS_Support and __DSP568
 * mm 981029  Changed __DSP568 to __m56800__
 * mf 981115  fixed mm change of 981015
 * blc 990209 added __msize
 * hh 990227 rewrote in an effort to make faster.
 * hh 990504 Added configuration for MIPS in several places
 */

#else /* _MSL_PRO4_MALLOC */

/*
 *	alloc.c
 *	
 *	Routines
 *	--------
 *		init_alloc			Initialize allocs' memory pool to be fixed size and location
 *		GetHeapSymbols		get linker defined symbols for embedded
 *		init_default_heap	initialize the default heap
 *		malloc				Allocate memory
 *		calloc				Allocate and clear memory
 *		realloc				
 *		free				Return memory block to allocs' memory pool
 *	
 *
 */

#include <ansi_parms.h>                 /* mm 970904 */

#if __dest_os != __be_os               /* mm 970708 */

#include "critical_regions.h"
#include <stdlib.h>
#include "pool_alloc.h"

#if __ALTIVEC__
mem_pool_obj	__vec_malloc_pool;
static int		vec_initialized = 0;
#endif
mem_pool_obj	__malloc_pool;
static int		initialized = 0;

#ifdef _No_Alloc_OS_Support	                   /* begin mm 981015 insert */
mem_pool_obj *  __malloc_pool_ptr = &__malloc_pool;        

static int	defaultheapinitialized = 0;

int init_alloc(void * pool_ptr, mem_size size);

/***************************************************************************
 *	init_alloc
 *		Initializes the memory pool that malloc uses. This call is designed to
 *		be used for systems that DO NOT have memory allocation functions
 *  	built into the system. The first call will define the pool and use the
 *		rest of the space as a block of memory. Subsequent calls will add the
 *		memory as another block of memory to the malloc's existing pool. Once
 *		a heap of memory is allocated to the malloc memory pool, it can not 
 *		be returned.
 *
 *	Inputs:
 *		pool_ptr		pointer to memory to be used in malloc's memory pool
 *		size				size of memory to be used in (or added to) malloc's memory pool
 *
 *	Outputs:
 *		None
 *
 *	Returns: int
 *		0		Fail
 *		1		Success: Memory initialized as malloc's memory pool
 *		2		Success: Memory added to initial heap successfully
 *
 *  NOTE: 
 *		This function must be called prior to using other alloc.c functions
 *
 ***************************************************************************/
extern int init_alloc(void * pool_ptr, mem_size size)
{

 if (!initialized) {
 	
 /*************************************************************************** 
  * Memory not initialized, make a pool and use the rest of the room for
  *	memory blocks
	***************************************************************************/
		/* Size of pool overhead */
		size_t pool_overhead	=	sizeof(mem_pool_obj);
		
		/* Remove pool overhead from size */
		size -= pool_overhead; 
		
		/* Point malloc pool at memory address provided */
		__malloc_pool_ptr = (mem_pool_obj *)pool_ptr;
	
		/* Create pool structure at memory address provided */
		__init_pool_obj(__malloc_pool_ptr);
		
		/* Assign the rest of the space (size - pool_overhead) to a memory block  */
		__pool_preassign(__malloc_pool_ptr, (void *)((size_t)pool_ptr+pool_overhead), size);
		
		/* Mark memory as initialized */
		initialized = 1;
		
		/* Return successful pool initialization value */
		return(1);
		
	} else {
	/***************************************************************************
	 * Memory already initialized, use all memory space as a memory block
	 ***************************************************************************/
		__pool_preassign(__malloc_pool_ptr, (void *)pool_ptr, size);
		
		/* Return successful heap addition value */
		return(2);
	}
}

#if __dest_os	== __ppc_eabi  || __dest_os ==__dolphin_os

__declspec(section ".init") extern char		_heap_addr[];	/* starting address for heap */
__declspec(section ".init") extern char 	_heap_end[];	/* address after end byte of heap */

#elif __dest_os == __nec_eabi_bare

	extern unsigned int _heap_size;			/* size of the stack (PPC EABI pref panel) */
	extern unsigned int _heap_addr;			/* starting address for heap */
	
#elif __dest_os == __mcore_bare

	extern unsigned int _heap_size;			/* size of the stack (mCore EABI pref panel) */
	extern unsigned int _heap_addr;			/* starting address for heap */
	extern unsigned int __heap_size;   		/*_ = (unsigned int)&_heap_size;*/
	extern unsigned int __heap_addr;  		/* = (unsigned int)&_heap_addr;*/
	/*unsigned int _heap_end;  */	/* si 010899 Linker has no _heap_end		 */ 

#elif defined(__m56800__)		/* mm 981029 */

	extern unsigned int _heap_size;			/* size of the stack (PPC EABI pref panel) */
	extern unsigned int _heap_addr;			/* starting address for heap */
	extern unsigned int _heap_end;			/* starting address for heap */

#elif __dest_os == __mips_bare
	extern char	_heap_addr[];				/* starting address for heap */
	extern char _heap_end[];				/* address after end byte of heap */

#elif __dest_os == __n64_os					/* ad 01.28.99, added nintendo support */
	extern char _heap_addr[];
	extern char _heap_end[];

#else
#error
#endif  /* __dest_os	== __ppc_eabi */

void init_default_heap(void);
void init_default_heap(void)
{
	defaultheapinitialized = 1;
	
	#if __dest_os == __mcore_bare || (__dest_os == __nec_eabi_bare)
		if (__heap_size!= 0)
			init_alloc((void *)__heap_addr, __heap_size);
	#else
		if (_heap_end - _heap_addr != 0)
			init_alloc((void *)_heap_addr, (_heap_end - _heap_addr));
	#endif
}

#endif  /* _No_Alloc_OS_Support */   	         /* end mm 981015 insert */
/*  #include <stdio.h>  */  /*  who added this???  */

void * malloc(size_t size)
{
	void *	block;
	
	__begin_critical_region(malloc_pool_access);
	
#ifdef _No_Alloc_OS_Support					/* mm 981015 */
 
 if (!defaultheapinitialized)				/* mm 981015 */
	{											/* mm 981015 */
		init_default_heap();					/* mm 981015 */
	}
	if (!initialized)		/*  vss  990121  */
	{
		return(0);
	}
	block = __pool_alloc(__malloc_pool_ptr, size);	
#else

	if (!initialized)
	{
		__init_pool_obj(&__malloc_pool);
		initialized = 1;
	}
	block = __pool_alloc(&__malloc_pool, size);	
#endif	/* _No_Alloc_OS_Support */

	__end_critical_region(malloc_pool_access);
		
	return(block);
}

void * calloc(size_t nmemb, size_t size)
{
	void *	block;
	
	__begin_critical_region(malloc_pool_access);
	
#ifndef _No_Alloc_OS_Support								/* mm 981015 */
	if (!initialized)
	{
		__init_pool_obj(&__malloc_pool);
		initialized = 1;
	}
	block = __pool_alloc_clear(&__malloc_pool, nmemb*size);
	
#else															/* mm 981015 */
	if (!defaultheapinitialized)								/* mm 981015 */
	{															/* mm 981015 */
		init_default_heap();									/* mm 981015 */
	}															/* mm 981015 */
	if (!initialized)											/* mm 981015 */
	{															/* mm 981015 */
		return(0);												/* mm 981015 */
	}															/* mm 981015 */
	block = __pool_alloc_clear(__malloc_pool_ptr, nmemb*size);	/* mm 981015 */
#endif	/* _No_Alloc_OS_Support */								/* mm 981015 */
	
	__end_critical_region(malloc_pool_access);
	
	return(block);
}

void * realloc(void * ptr, size_t size)
{
	void *	block;
	
	__begin_critical_region(malloc_pool_access);
	
#ifndef _No_Alloc_OS_Support								/* mm 981015 */
	if (!initialized)
	{
		__init_pool_obj(&__malloc_pool);
		initialized = 1;
	}
	
	block = __pool_realloc(&__malloc_pool, ptr, size);
#else														/* mm 981015 */
	if (!defaultheapinitialized)							/* mm 981015 */
	{														/* mm 981015 */
		init_default_heap();								/* mm 981015 */
	}														/* mm 981015 */
	if (!initialized)										/* mm 981015 */
	{														/* mm 981015 */
		return(0);											/* mm 981015 */
	}														/* mm 981015 */
	block = __pool_realloc(__malloc_pool_ptr, ptr, size);	/* mm 981015 */
#endif	/* _No_Alloc_OS_Support */							/* mm 981015 */

	__end_critical_region(malloc_pool_access);
	
	return(block);
}

void free(void * ptr)
{
	if (!initialized)
		return;
	
	__begin_critical_region(malloc_pool_access);
	

#ifndef _No_Alloc_OS_Support		  /* mm 981015 */								
	__pool_free(&__malloc_pool, ptr);		
#else																						/* mm 981015 */
	__pool_free(__malloc_pool_ptr, ptr);					/* mm 981015 */
#endif	  /* _No_Alloc_OS_Support */							/* mm 981015 */
	__end_critical_region(malloc_pool_access);
}

size_t __msize(void * ptr)
{
	/* clear lower two bits to remove pool_alloc flags */
	return *((size_t *) ptr - 1) & ~0x03;
}


#if __ALTIVEC__
/*
 * AltiVec memory routines which deal with 16-byte aligned blocks
 *
 */
 
void * vec_malloc(size_t size)
{
	void *	block;
	
	__begin_critical_region(malloc_pool_access);
	
	if (!vec_initialized)
	{
		__init_align_pool_obj(&__malloc_pool, 16);
		vec_initialized = 1;
	}
	
	block = __pool_alloc(&__malloc_pool, size);
	
	__end_critical_region(malloc_pool_access);
	
	return(block);
}

void * vec_calloc(size_t nmemb, size_t size)
{
	void *	block;
	
	__begin_critical_region(malloc_pool_access);
	
	if (!vec_initialized)
	{
		__init_align_pool_obj(&__malloc_pool, 16);
		vec_initialized = 1;
	}
	
	block = __pool_alloc_clear(&__malloc_pool, nmemb*size);
	
	__end_critical_region(malloc_pool_access);
	
	return(block);
}

void * vec_realloc(void * ptr, size_t size)
{
	void *	block;
	
	__begin_critical_region(malloc_pool_access);
	
	if (!vec_initialized)
	{
		__init_align_pool_obj(&__malloc_pool, 16);
		vec_initialized = 1;
	}
	
	block = __pool_realloc(&__malloc_pool, ptr, size);
	
	__end_critical_region(malloc_pool_access);
	
	return(block);
}

void vec_free(void * ptr)
{
	if (!vec_initialized)
		return;
	
	__begin_critical_region(malloc_pool_access);
	
	__pool_free(&__malloc_pool, ptr);
	
	__end_critical_region(malloc_pool_access);
}
#endif

#endif /* __dest_os != __be_os */               /* mm 970708 */

/*     Change record
 *	20-Mar-95 JFH  First code release.
 *  09-Jun-95 JFH  Changed malloc_pool to __malloc_pool as per standard naming conventions.
 * mm 970708  Inserted Be changes
 *	20-Jul-97 MEA  changed __no_os to _No_Alloc_OS_Support and  __ppc_eabi_bare to __ppc_eabi.
 * mm 970904  Added include ansi_parms.h  to allow compilation without prefix
 * mm 981015  Added code for _No_Alloc_OS_Support and __DSP568
 * mm 981029  Changed __DSP568 to __m56800__
 * mf 981115  fixed mm change of 981015
 * ad 990128  added nintendo support
 * blc 990209 added __msize
 * bobc 990302 added vec_malloc (etc) for AltiVec
 */

#endif
