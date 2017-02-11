/* �������֌W */

#include "bootpack.h"
#include "mylib.h"
#include "mailbox.h"

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000
#define MALLOC_MAX_NUM 1024
unsigned int malloc_start_table[MALLOC_MAX_NUM];
unsigned int malloc_size_table[MALLOC_MAX_NUM];


unsigned int memtest(unsigned int start, unsigned int end)
{
	return mailbox_getMemorySize();
}

void memman_init(struct MEMMAN *man)
{
	man->frees = 0;			/* �������̌� */
	man->maxfrees = 0;		/* �󋵊ώ@�p�Ffrees�̍ő�l */
	man->lostsize = 0;		/* ����Ɏ��s�������v�T�C�Y */
	man->losts = 0;			/* ����Ɏ��s������ */
	return;
}

unsigned int memman_total(struct MEMMAN *man)
/* �����T�C�Y�̍��v��� */
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;
	}
	return t;
}

void memman_monitor(struct MEMMAN *man)
{
	unsigned int i, t;
	char s[1024];
	s[0]=0;
	printf("MEMMAN_MONITOR:\n");
	printf("\tfrees: %d, maxfrees: %d, lostsize: %d, losts: %d\n", man->frees, man->maxfrees, man->lostsize, man->losts);
	t = 0;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size>0) {
			log(s, "\t[%d]: free addr: %x, size:%x \n", i, man->free[i].addr, man->free[i].size);
		}
		t += man->free[i].size;
	}
	printf(s);
	printf("\tTotal free size: %d\n", t);
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* �m�� */
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			/* �\���ȍL���̂����𔭌� */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				/* free[i]���Ȃ��Ȃ����̂őO�ւ߂� */
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1]; /* �\���̂̑�� */
				}
			}
			return a;
		}
	}
	return 0; /* �������Ȃ� */
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* ��� */
{
	unsigned int i, j;
	/* �܂Ƃ߂₷�����l����ƁAfree[]��addr���ɕ���ł���ق������� */
	/* ������܂��A�ǂ��ɓ����ׂ��������߂� */
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	/* free[i - 1].addr < addr < free[i].addr */
	if (i > 0) {
		/* �O������ */
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			/* �O�̂����̈�ɂ܂Ƃ߂��� */
			man->free[i - 1].size += size;
			if (i < man->frees) {
				/* �������� */
				if (addr + size == man->free[i].addr) {
					/* �Ȃ�ƌ��Ƃ��܂Ƃ߂��� */
					man->free[i - 1].size += man->free[i].size;
					/* man->free[i]�̍폜 */
					/* free[i]���Ȃ��Ȃ����̂őO�ւ߂� */
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1]; /* �\���̂̑�� */
					}
				}
			}
			return 0; /* �����I�� */
		}
	}
	/* �O�Ƃ͂܂Ƃ߂��Ȃ����� */
	if (i < man->frees) {
		/* ��낪���� */
		if (addr + size == man->free[i].addr) {
			/* ���Ƃ͂܂Ƃ߂��� */
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* �����I�� */
		}
	}
	/* �O�ɂ����ɂ��܂Ƃ߂��Ȃ� */
	if (man->frees < MEMMAN_FREES) {
		/* free[i]�������A���ւ��炵�āA�����܂���� */
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; /* �ő�l���X�V */
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /* �����I�� */
	}
	/* ���ɂ��点�Ȃ����� */
	man->losts++;
	man->lostsize += size;
	return -1; /* ���s�I�� */
}

unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size)
{
	unsigned int a;
	size = (size + 0x3fff) & 0xffffC000; // actually this is memman_alloc_16k
	a = memman_alloc(man, size);
	return a;
}

int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
	int i;
	size = (size + 0x3fff) & 0xffffC000; // actually this is memman_free_16k
	i = memman_free(man, addr, size);
	return i;
}

void *malloc (unsigned nSize)		// result must be 4-byte aligned
{
	// this won't work for separate processes with dedicated memory space
	// because it assumes single memory space
	// need to update before application is added
	// also the address is not checked to be aligned by 4 (although expected)
	int i;
    unsigned int addr;
	unsigned int size;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	unsigned irq;

	size = (nSize + 0xF) & 0xFFFFFFF0;
	irq = hold_IRQ();
	for(i=0; i<MALLOC_MAX_NUM; i++) {
		if (malloc_size_table[i]==0) {
			addr = memman_alloc(memman, size);
			unhold_IRQ(irq);
			malloc_start_table[i] = addr;
			malloc_size_table[i] = size;
			//			printf("Malloc: %08x, %d\n", addr, size);
			return (void *) addr;
		}
	}
	irq = hold_IRQ();
	// no space left for malloc table
	return 0;
}

void free (void *pBlock)
{
	// this won't work for separate processes with dedicated memory space
	// because it assumes single memory space
	// need to update before application is added
	// also the address is not checked to be aligned by 4 (although expected)
	int i, j;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	unsigned int irq;

	irq = hold_IRQ();
	for(i=0; i<MALLOC_MAX_NUM; i++) {
		if (malloc_start_table[i] == (unsigned int) pBlock) {
			j = memman_free(memman, malloc_start_table[i], malloc_size_table[i]);
			if (j<0) {
				printf("Error: Failed to free: %08X, %d bytes\n", (unsigned int) pBlock, malloc_size_table[i]);
				log_error(3);
			}
			malloc_start_table[i] = 0;
			malloc_size_table[i]=0;
			return;
		}
	}
	unhold_IRQ(irq);
	printf("Error: Failed to free: %08X\n", (unsigned int) pBlock);
	return;
}
