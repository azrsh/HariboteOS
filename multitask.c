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
    task->flags = 2;    //動作中マーク
    task->priority = 2; //0.02秒
    taskControl->running = 1;
    taskControl->now = 0;
    taskControl->tasks[0] = task;
    load_tr(task->selector);
    taskTimer = timer_allocate();
    timer_set_time(taskTimer, task->priority);
    return task;
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

void task_run(struct TASK *task, int priority)
{
    if (priority > 0)
    {
        task->priority = priority;
    }

    task->flags = 2; //動作中マーク
    taskControl->tasks[taskControl->running] = task;
    taskControl->running++;
}

void task_switch(void)
{
    struct TASK *task;
    taskControl->now++;
    if (taskControl->now == taskControl->running)
    {
        taskControl->now = 0;
    }
    task = taskControl->tasks[taskControl->now];
    timer_set_time(taskTimer, task->priority);
    if (taskControl->running >= 2)
    {
        farjump(0, taskControl->tasks[taskControl->now]->selector);
    }

    return;
}

void task_sleep(struct TASK *task)
{
    int i;
    char taskswitch = 0;
    if (task->flags == 2) //タスクが実行中なら
    {
        if (task == taskControl->tasks[taskControl->now])
        {
            taskswitch = 1;
        }

        //taskがどこにいるか探索
        for (i = 0; i < taskControl->running; i++)
        {
            if (taskControl->tasks[i] == task)
            {
                break;
            }
        }
        taskControl->running--;
        if (i < taskControl->now) //nowがずれる場合は修正
        {
            taskControl->now--;
        }
        for (; i < taskControl->running; i++)
        {
            taskControl->tasks[i] = taskControl->tasks[i + 1];
        }
        task->flags = 1; //動作していない状態
        if (taskswitch != 0)
        {
            if (taskControl->now >= taskControl->running)
            {
                taskControl->now = 0;
            }
            farjump(0, taskControl->tasks[taskControl->now]->selector);
        }
    }
    return;
}
