#ifndef _RKP_H
#define _RKP_H

#ifndef __ASSEMBLY__
#include <linux/uh.h>

/* uH_RKP Command ID */
enum __RKP_CMD_ID {
	RKP_START = 1,
	RKP_DEFERRED_START,
	RKP_WRITE_PGT1,
	RKP_WRITE_PGT2,
	RKP_WRITE_PGT3,
	RKP_EMULT_TTBR0,
	RKP_EMULT_TTBR1,
	RKP_EMULT_DORESUME,
	RKP_FREE_PGD,
	RKP_NEW_PGD,
	RKP_KASLR_MEM,
	RKP_FIMC_VERIFY, /* CFP cmds */
	RKP_JOPP_INIT,
	RKP_ROPP_INIT,
	RKP_ROPP_SAVE,
	RKP_ROPP_RELOAD,
	/* and KDT cmds */
	RKP_NOSHIP,
#ifdef CONFIG_RKP_TEST
	CMD_ID_TEST_GET_PAR = 0x81,
	CMD_ID_TEST_GET_RO = 0x83,
	CMD_ID_TEST_GET_VA_XN,
	CMD_ID_TEST_GET_VMM_INFO,
#endif
	RKP_MAX
};

#define RKP_PREFIX  UL(0x83800000)

#ifdef CONFIG_RKP_TEST
#define RKP_INIT_MAGIC		0x5afe0002
#else
#define RKP_INIT_MAGIC		0x5afe0001
#endif

#define RKP_VMM_BUFFER		0x600000
#define RKP_RO_BUFFER		UL(0x800000)

#define RKP_FIMC_FAIL		0x10
#define RKP_FIMC_SUCCESS	0xa5

/* For 8G RAM */
#ifdef CONFIG_UH_RKP_8G
#define RKP_EXTRA_MEM_START	(0xAF600000)
#define RKP_PGT_BITMAP_LEN	(0x40000)
#define RKP_ROBUF_SIZE		(18ULL << 20)
#else
#define RKP_EXTRA_MEM_START	(0xAF800000)
#define RKP_PGT_BITMAP_LEN	(0x30000)
#define RKP_ROBUF_SIZE		(16ULL << 20)
#endif

#define RKP_EXTRA_MEM_SIZE	(0x600000)
#define RKP_ROBUF_START		(0xB0800000)

#define RKP_REGION_START	(RKP_EXTRA_MEM_START)
#define RKP_REGION_SIZE		(RKP_REGION_START - RKP_ROBUF_START + RKP_ROBUF_SIZE)

#define RKP_RBUF_VA		(phys_to_virt(RKP_ROBUF_START))
#define RO_PAGES		(RKP_ROBUF_SIZE >> PAGE_SHIFT) // (RKP_ROBUF_SIZE/PAGE_SIZE)
#define CRED_JAR_RO		"cred_jar_ro"
#define TSEC_JAR		"tsec_jar"
#define VFSMNT_JAR		"vfsmnt_cache"

#ifdef CONFIG_KNOX_KAP
extern int boot_mode_security;
#endif

extern u8 rkp_pgt_bitmap[];
extern u8 rkp_map_bitmap[];
extern u8 __rkp_start_prot_page[];
extern u8 __rkp_end_prot_page[];

typedef struct rkp_init rkp_init_t;
extern u8 rkp_started;
void *rkp_ro_alloc(void);
void rkp_ro_free(void *free_addr);
unsigned int is_rkp_ro_page(u64 addr);

struct rkp_init { //copy from uh (app/rkp/rkp.h)
         u32 magic;
         u64 vmalloc_start;
         u64 vmalloc_end;
         u64 init_mm_pgd;
         u64 id_map_pgd;
         u64 zero_pg_addr;
         u64 rkp_pgt_bitmap;
         u64 rkp_dbl_bitmap;
         u32 rkp_bitmap_size;
         u32 rkp_prot_page_size;
         u32 fimc_type;
	 u64 fimc_phys_addr;
         u64 _text;
         u64 _etext;
         u64 extra_memory_addr;
         u32 extra_memory_size;
         u64 physmap_addr; //not used. what is this for?
         u64 _srodata;
         u64 _erodata;
         u32 large_memory;
         u64 tramp_pgd;
         u64 tramp_valias;
};

#define rkp_is_pg_protected(va)	rkp_is_protected(va, __pa(va), (u64 *)rkp_pgt_bitmap, 1)
#define rkp_is_pg_dbl_mapped(pa) rkp_is_protected((u64)__va(pa), pa, (u64 *)rkp_map_bitmap, 0)

#define RKP_PHYS_ADDR_MASK		((1ULL << 40)-1)

/*
 * The following all assume PHYS_OFFSET is fix addr
 */
#define	PHYS_PFN_OFFSET_MIN_DRAM1	(0x80000ULL)
#define	PHYS_PFN_OFFSET_MAX_DRAM1	(0x100000ULL)
#define	PHYS_PFN_OFFSET_MIN_DRAM2	(0x880000ULL)
#ifdef CONFIG_UH_RKP_8G
#define	PHYS_PFN_OFFSET_MAX_DRAM2	(0xA00000ULL)
#else
#define	PHYS_PFN_OFFSET_MAX_DRAM2	(0x980000ULL)
#endif

#define DRAM_PFN_GAP			(PHYS_PFN_OFFSET_MIN_DRAM2 - PHYS_PFN_OFFSET_MAX_DRAM1)

#define FIMC_LIB_OFFSET_VA		(VMALLOC_START + 0xF6000000 - 0x8000000)
#define FIMC_LIB_START_VA		(FIMC_LIB_OFFSET_VA + 0x04000000)
#define FIMC_VRA_LIB_SIZE		(0x80000)
#define FIMC_DDK_LIB_SIZE		(0x400000)
#define FIMC_RTA_LIB_SIZE		(0x400000)

#define FIMC_LIB_SIZE			(FIMC_VRA_LIB_SIZE + FIMC_DDK_LIB_SIZE + FIMC_RTA_LIB_SIZE)

static inline u64 rkp_get_sys_index(u64 pfn)
{
	if (pfn >= PHYS_PFN_OFFSET_MIN_DRAM1
		&& pfn < PHYS_PFN_OFFSET_MAX_DRAM1) {
		return ((pfn) - PHYS_PFN_OFFSET);
	}
	if (pfn >= PHYS_PFN_OFFSET_MIN_DRAM2
		&& pfn < PHYS_PFN_OFFSET_MAX_DRAM2) {
		return ((pfn) - PHYS_PFN_OFFSET - DRAM_PFN_GAP);
	}
	return (~0ULL);
}

static inline u8 rkp_is_protected(u64 va, u64 pa, u64 *base_addr, u64 type)
{
	u64 phys_addr = pa & (RKP_PHYS_ADDR_MASK);
	u64 *p = base_addr;
	u64 rindex;
	u8 val;
	u64 index = rkp_get_sys_index((phys_addr>>PAGE_SHIFT));

	if (index == (~0ULL))
		return 0;

	p += (index>>6);
	rindex = index % 64;
	val = (((*p) & (1ULL<<rindex))?1:0);
	return val;
}
#endif //__ASSEMBLY__
#endif //_RKP_H
