#include "kernel/memory.h"
#include "kernel/ards.h"
#include "kernel/types.h"
#include "driver/vga.h"
#include "kernel/8259a.h"
#include "lib/bitmap.h"
#include "lib/string.h"

struct bitmap phy_mem_bitmap;
struct bitmap vir_mem_bitmap;
int memory_total_size;
int memory_used_size;
struct mem_block_desc k_block_descs[DESC_CNT];	// �ں��ڴ������������

void init_memory()
{
	int idx, i; 
	int a,b,*c;
	int *new_page;
	
	memory_total_size = 0;
	//��ʼ��ȡ
	init_ards();
	
	//ֻ֧�����512MB�����ڴ�
	if(memory_total_size >= 512*1024*1024){
		memory_total_size = 512*1024*1024;
	}
	
	int pages = memory_total_size/PAGE_SIZE;
	int page_byte = pages/8;
	
	phy_mem_bitmap.bits = (uint8_t*)PHY_MEM_BITMAP;
	phy_mem_bitmap.btmp_bytes_len = page_byte;
	
	vir_mem_bitmap.bits = (uint8_t*)VIR_MEM_BITMAP;
	vir_mem_bitmap.btmp_bytes_len = page_byte;
	
	bitmap_init(&phy_mem_bitmap);
	bitmap_init(&vir_mem_bitmap);
	
	//���ں˵�ҳĿ¼ǰ2048��գ��ڳ����������ã��ں�ռ�ø�2048�ֽ�
	memset((void *)PAGE_DIR_VIR_ADDR, 0, 2048);
	
	
	block_desc_init(k_block_descs);
	
	/*
	a = (int )kernel_alloc_page(1);
	put_int(a);
	put_str("\n");
	
	a = (int )kernel_alloc_page(2);
	put_int(a);
	put_str("\n");
	
	kernel_free_page(a, 2);
	
	a = (int )kernel_alloc_page(1);
	put_int(a);
	put_str("\n");
	
	kernel_free_page(a, 1);
	
	a = (int )kernel_alloc_page(3);
	put_int(a);
	put_str("\n");
	
	kernel_free_page(a, 3);

	a = (int )kernel_alloc_page(1);
	put_int(a);
	put_str("\n");
	*/
	/*
	a = alloc_mem_page();
	put_int(a);
	put_str("\n");
	a = alloc_mem_page();
	put_int(a);
	put_str("\n");
	free_mem_page(a);
	
	a = alloc_mem_page();
	put_int(a);
	put_str("\n");
	
	b = alloc_vir_page();
	put_int(b);
	put_str("\n");
	b = alloc_vir_page();
	put_int(b);
	put_str("\n");
	
	free_vir_page( b);
	
	b  = alloc_vir_page();
	put_int(b);
	put_str("\n");
	put_str("\n");
	
	c = kernel_alloc_page(1);
	put_int(c);
	put_str("\n");
	
	*c = 0xaa55;
	
	put_int(*c);
	put_str("\n");*/
	/*new_page = copy_kernel_page();
	put_int(new_page);
	put_str("\n");
	put_int(*new_page);
	put_str("\n");*/
}

/* ����arena�е�idx���ڴ��ĵ�ַ */
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
  return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/* �����ڴ��b���ڵ�arena��ַ */
static struct arena* block2arena(struct mem_block* b) {
   return (struct arena*)((uint32_t)b & 0xfffff000);
}

/* �ڶ�������size�ֽ��ڴ� */
void* sys_malloc(uint32_t size) {
  
	struct mem_block_desc* descs;
  
	descs = k_block_descs;

   /* ��������ڴ治���ڴ��������Χ����ֱ�ӷ���NULL */
   if (!(size > 0)) {
      return NULL;
   }
   struct arena* a;
   struct mem_block* b;	
  
/* ��������ڴ��1024, �ͷ���ҳ�� */
	if (size > 1024) {
		uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PAGE_SIZE);    // ����ȡ����Ҫ��ҳ����
		a = kernel_alloc_page(page_cnt);

		if (a != NULL) {
			memset(a, 0, page_cnt * PAGE_SIZE);	 // ��������ڴ���0  

			/* ���ڷ���Ĵ��ҳ��,��desc��ΪNULL, cnt��Ϊҳ����,large��Ϊtrue */
			a->desc = NULL;
			a->cnt = page_cnt;
			a->large = true;
			return (void*)(a + 1);		 // ���arena��С����ʣ�µ��ڴ淵��
		} else { 	
			return NULL; 
		}
	} else {    // ��������ڴ�С�ڵ���1024,���ڸ��ֹ���mem_block_desc��ȥ����
		uint8_t desc_idx;
      
		/* ���ڴ����������ƥ����ʵ��ڴ���� */
		for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
			if (size <= descs[desc_idx].block_size) {  // ��С�����,�ҵ����˳�
				break;
			}
		}
	
		/* ��mem_block_desc��free_list���Ѿ�û�п��õ�mem_block,
		* �ʹ����µ�arena�ṩmem_block */
		if (list_empty(&descs[desc_idx].free_list)) {
		a = kernel_alloc_page(1);       // ����1ҳ����Ϊarena
		if (a == NULL) {
			return NULL;
		}
		memset(a, 0, PAGE_SIZE);

		/* ���ڷ����С���ڴ�,��desc��Ϊ��Ӧ�ڴ��������, 
		 * cnt��Ϊ��arena���õ��ڴ����,large��Ϊfalse */
		a->desc = &descs[desc_idx];
		a->large = false;
		a->cnt = descs[desc_idx].blocks_per_arena;
		uint32_t block_idx;
		
		 /* ��ʼ��arena��ֳ��ڴ��,����ӵ��ڴ����������free_list�� */
		for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++) {
			b = arena2block(a, block_idx);
			list_append(&a->desc->free_list, &b->free_elem);	
		}
    }

   /* ��ʼ�����ڴ�� */
    b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
    memset(b, 0, descs[desc_idx].block_size);

    a = block2arena(b);  // ��ȡ�ڴ��b���ڵ�arena
    a->cnt--;		   // ����arena�еĿ����ڴ������1
    return (void*)b;
   }
}

/* �����ڴ�ptr */
void sys_mfree(void* ptr) {
   if (ptr != NULL) {
		struct mem_block* b = ptr;
		struct arena* a = block2arena(b);	     // ��mem_blockת����arena,��ȡԪ��Ϣ
		if (a->desc == NULL && a->large == true) { // ����1024���ڴ�
			kernel_free_page((int )a, a->cnt); 
		} else {				 // С�ڵ���1024���ڴ��
			/* �Ƚ��ڴ����յ�free_list */
			list_append(&a->desc->free_list, &b->free_elem);

			/* ���жϴ�arena�е��ڴ���Ƿ��ǿ���,����Ǿ��ͷ�arena */
			if (++a->cnt == a->desc->blocks_per_arena) {
				uint32_t block_idx;
				for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
					struct mem_block*  b = arena2block(a, block_idx);
					//ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
					list_remove(&b->free_elem);
				}
				kernel_free_page((int )a, 1); 
			} 
		}   
	}
}


/* Ϊmalloc��׼�� */
void block_desc_init(struct mem_block_desc* desc_array) {				   
   uint16_t desc_idx, block_size = 16;

   /* ��ʼ��ÿ��mem_block_desc������ */
   for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
      desc_array[desc_idx].block_size = block_size;
      /* ��ʼ��arena�е��ڴ������ */
      desc_array[desc_idx].blocks_per_arena = (PAGE_SIZE - sizeof(struct arena)) / block_size;	  

      list_init(&desc_array[desc_idx].free_list);

      block_size *= 2;         // ����Ϊ��һ������ڴ��
   }
}

/**���ҳ�ķ���״��*/
int pages_status()
{
	int i , j = 0;
	for(i = 0; i < memory_total_size/PAGE_SIZE/8; i++){
		if(bitmap_scan_test(&phy_mem_bitmap, i) == 0)j++;
	}
	return j;
}
/**����û��ʹ�õ��ڴ���*/
int get_free_memory()
{
	int pages = pages_status()*8;
	int free_size = pages*PAGE_SIZE;
	return free_size;
}

int *copy_kernel_page()
{
	void *new_page;
	new_page = kernel_alloc_page(1);
	/*�����ں�ǰ2048�ֽ��Ѿ���գ���������ʹ�õĻ���ֻ�и�2048�ֽڣ���2048�ֽڸ�δ��������ʹ��*/
	/*
	*��Ϊ��ַ��int���ͣ�����new_page+512�൱�ڷ���	(char )new_page+2048
	*/
	//memcpy((void *)(new_page+2048), (void *)(PAGE_DIR_VIR_ADDR+2048), 2048);
	memcpy((void *)new_page, (void *)PAGE_DIR_VIR_ADDR, PAGE_SIZE);
	
	
	return new_page;
}

void *get_kernel_page()
{
	return (void *)PAGE_DIR_VIR_ADDR;
}

/*
�������ַת���������ַ
*/
uint32_t addr_v2p(uint32_t vaddr)
{
	uint32_t *pte = pte_ptr(vaddr);
	uint32_t phy_addr;
	//put_int((int )*pte);
	phy_addr = ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
	//put_int((int )phy_addr);
	return phy_addr;
}

uint32_t *pte_ptr(uint32_t vaddr)
{
	uint32_t *pte = (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
	return pte;
}

uint32_t *pde_ptr(uint32_t vaddr)
{
	uint32_t *pde = (uint32_t *)((0xfffff000) + PDE_IDX(vaddr) * 4);
	return pde;
}

void fill_vir_page_talbe(int vir_address)
{
	int va = vir_address;
	int *pde, *pte;
	int pte_addr;
	pde = (int *)(((va&0xffc00000)>>20)|0xfffff000);	//���ҳĿ¼��ĵ�ַ
	if(((*pde)&0x00000001) != 0x00000001){	//������ҳ��
		int pt = alloc_mem_page();	//����ҳ���ַ
		pt |= 0x00000007;
		*pde = pt;		//��дҳĿ¼��Ϊҳ��ĵ�ַ
	}
	pte_addr = (int )(((va>>10)&0x003ff000)|0xffc00000);	//���ҳĿ����ĵ�ַ
	va &= 0x003ff000;
	va >>= 10;
	pte_addr |= va;
	pte = (int *)pte_addr;
	int page = alloc_mem_page();	//����ҳ��ַ
	page |= 0x00000007;
	*pte = page;	//��дҳ����Ϊҳ�ĵ�ַ
}

void clean_vir_page_table(int vir_address)
{
	int va = vir_address;
	int *pde, *pte;
	int pte_addr;
	int page_phy_addr;
	pde = (int *)(((va & 0xffc00000) >> 20) | 0xfffff000);
	if(((*pde)&0x00000001) != 0x00000001){	//������ҳ��,ֱ�Ӳ���ҳ����
		//error
		put_str("clean vir page failed.");
		while(1);
	}
	pte_addr = (int )(((va>>10)&0x003ff000)|0xffc00000);	//���ҳĿ����ĵ�ַ
	va &= 0x003ff000;
	va >>= 10;
	pte_addr |= va;
	pte = (int *)pte_addr;
	
	
	page_phy_addr = *pte;	//���ҳ������ҳ�������ַ
	*pte = 0;	//���ҳ����
	
	page_phy_addr &= 0xfffff000;	//������22λ������
	free_mem_page(page_phy_addr);	//�ͷŶ�Ӧ������ҳ
}


/*
	����һ������ҳ
	�ɹ����ص�ַ��ʧ�ܷ���-1
*/
int alloc_mem_page()
{
	int idx;
	int mem_addr;
	idx = bitmap_scan(&phy_mem_bitmap, 1);
	if(idx != -1){
		bitmap_set(&phy_mem_bitmap, idx, 1);
	}else{
		return -1;
	}
	mem_addr = idx*0x1000 + PHY_MEM_BASE_ADDR;
	/*put_int(mem_addr);
	put_str("\n");
	*/
	return mem_addr;
}
/*
	�ͷ�һ������ҳ
	�ɹ�����0��ʧ�ܷ���-1
*/
 int free_mem_page(int address)
{
	int addr = address;
	int idx, i;

	idx = (addr-PHY_MEM_BASE_ADDR)/0x1000;
	if(bitmap_scan_test(&phy_mem_bitmap, idx)){
		bitmap_set(&phy_mem_bitmap, idx, 0);
	}else{
		return -1;
	}
	return 0;
}

/*
	����һ������ҳ
	�ɹ����ص�ַ��ʧ�ܷ���-1
*/
int alloc_vir_page()
{
	int idx;
	int vir_addr;
	idx = bitmap_scan(&vir_mem_bitmap, 1);
	if(idx != -1){
		bitmap_set(&vir_mem_bitmap, idx, 1);
	}else{
		return -1;
	}
	vir_addr = idx*0x1000 +  VIR_MEM_BASE_ADDR;
	
	
	return vir_addr;
}

/*
	�ͷ�һ������ҳ
	�ɹ�����0��ʧ�ܷ���-1
*/
int free_vir_page(int address)
{
	int addr = address;
	int idx, i;

	idx = (addr-VIR_MEM_BASE_ADDR)/0x1000;
	if(bitmap_scan_test(&vir_mem_bitmap, idx)){
		bitmap_set(&vir_mem_bitmap, idx, 0);
	}else{
		return -1;
	}
	return 0;
}

void *kernel_alloc_page(uint32_t pages)
{
	int i;
	int vir_page_addr, vir_page_addr_more;
	vir_page_addr = alloc_vir_page();	//����һ�������ַ��ҳ
	fill_vir_page_talbe(vir_page_addr);		//��ҳ��ӵ���ǰҳĿ¼��ϵͳ�У�ʹ�����Ա�ʹ��
	
	if(pages == 1){	//���ֻ��һ��ҳ
		return (void *)vir_page_addr;
	}else if(pages > 1){
		for(i = 1; i < pages; i++){
			vir_page_addr_more = alloc_vir_page();	//����һ�������ַ��ҳ
			fill_vir_page_talbe(vir_page_addr_more);		//��ҳ��ӵ���ǰҳĿ¼��ϵͳ�У�ʹ�����Ա�ʹ��	
		}
		return (void *)vir_page_addr;
	}else{
		return (void *)-1;
	}
	//Ӧ�ò��ᵽ������
	return (void *)-1;
}


void kernel_free_page(int vaddr, uint32_t pages)
{
	int i;
	int vir_page_addr = vaddr;
	free_vir_page(vir_page_addr);
	clean_vir_page_table(vir_page_addr);		//��ҳ��ӵ���ǰҳĿ¼��ϵͳ�У�ʹ�����Ա�ʹ��
	if(pages == 1){	//���ֻ��һ��ҳ
		return;
	}else if(pages > 1){
		for(i = 1; i < pages; i++){
			vir_page_addr += PAGE_SIZE;
			free_vir_page(vir_page_addr);
			clean_vir_page_table(vir_page_addr);		//��ҳ��ӵ���ǰҳĿ¼��ϵͳ�У�ʹ�����Ա�ʹ��
		}
		return ;
	}
}

