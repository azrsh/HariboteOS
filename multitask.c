#include "bootpack.h"

struct TASKCONTROL *taskControl;
struct TIMER *taskTimer;

struct TASK *task_init(struct MEMORYMANAGER *memoryManager)
{
    int i;
    struct TASK *task;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADRESS_GDT;
    taskControl = (struct TASKCONTROL *)memorymanager_allocate_4k(memoryManager, sizeof(struct TASKCONTROL));
    for (i = 0; i < MAX_TASKS; i++)
    {
        taskControl->tasks0[i].flags = 0;
        taskControl->tasks0[i].selector = (TASK_GDT0 + i) * 8;
        set_segment_descriptor(gdt + TASK_GDT0 + i, 103, (int)&taskControl->tasks0[i].tss, AR_TSS32);
    }
    task = task_allocate();
    task->flags = 2; //動作中マーク
    taskControl->running = 1;
    taskControl->now = 0;
    taskControl->tasks[0] = task;
    load_tr(task->selector);
    taskTimer = timer_allocate();
    timer_set_time(taskTimer, 2);
}

struct TASK *task_allocate()
{
    int i;
    struct TASK *task;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (taskControl->tasks0[i].flags == 0)
        {
            task = &taskControl->tasks0[i];
            task->flags = 1;               //使用中マーク
            task->tss.eflags = 0x00000202; //IF = 1;割込み許可フラグ
            task->tss.eax = 0;             //レジスタはとりあえずすべて0
            task->tss.ecx = 0;
            task->tss.edx = 0;
            task->tss.ebx = 0;
            task->tss.ebp = 0;
            task->tss.esi = 0;
            task->tss.edi = 0;
            task->tss.es = 0;
            task->tss.ds = 0;
            task->tss.fs = 0;
            task->tss.gs = 0;
            task->tss.ldtr = 0;
            task->tss.iomap = 0x40000000;
            return task;
        }
    }
    return 0; //もう全部使用中
}

void task_run(struct TASK *task)
{
    task->flags = 2; //動作中マーク
    taskControl->tasks[taskControl->running] = task;
    taskControl->running++;
}

void task_switch(void)
{
    timer_set_time(taskTimer, 2);
    if (taskControl->running >= 2)
    {
        taskControl->now++;
        if (taskControl->now == taskControl->running)
        {
            taskControl->now = 0;
        }
        farjump(0, taskControl->tasks[taskControl->now]->selector);
    }

    return;
}
