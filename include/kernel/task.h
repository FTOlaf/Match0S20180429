#ifndef _TASK_H
#define _TASK_H
#include "kernel/types.h"
#include "lib/fifo.h"
#include "gui/layer.h"
#include "gui/desktop.h"

#define NR_TASKS 2
#define MAX_TASKS 100

#define TASK_UNUSED 0

#define TASK_READY 1
#define TASK_RUNNING 2

#define TASK_TYPE_USER  3
#define TASK_TYPE_DRIVER  1
#define TASK_TYPE_UNKNOWN -1


#define TASK_NAME_LEN 24
#define TASK_NAME_MAX TASK_NAME_LEN+1

//ջ��Ľṹ
struct stackframe
{
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t kernel_esp;		//esp
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t ss;
};

//���̿��ƿ�Ľṹ
struct task 
{
	struct stackframe regs;
	int cr3;
	int *ptr_pdir;
	int32_t priority;	//��Ȩ��
	int32_t status;	//��Ȩ��
	int32_t ticks;
	char name[TASK_NAME_MAX];
	int pid;		//����id����Ϊ0������pid
	char type;		//��������ͣ�DRIVER ��USER
	struct FIFO32 *fifo;
	uint32_t esp_addr;
	int stack_page_count;
	struct layer *layer;
	int key_data;
	int deprived;	//����������ȵ�ʱ��Ƭ�����·���ʱˢ��
	struct task_bar_block *tbb;
};

extern struct task tasks_table[MAX_TASKS];
extern struct task *tasks_ptr[MAX_TASKS];
extern struct task *task_ready;
extern int task_running;
extern int task_now;

extern struct task *task_mouse, *task_idle;	//�����������������

void init_task();
struct task *task_alloc(char type);
void task_run(struct task *task, int priority);
void task_switch();
struct task *task_current();
void task_deprive(struct task *task);
void task_name(struct task *task, char *name);
void task_init(struct task *task, int entry, int stack, char *name);


void delay(int sec);
void task_idle_entry();
void task_close(struct task *task);

#endif

