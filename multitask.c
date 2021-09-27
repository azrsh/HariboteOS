#include "bootpack.h"

struct TASKCONTROL *taskControl;
struct TIMER *taskTimer;

struct TASK *task_now(void);
void task_add(struct TASK *task);
void task_remove(struct TASK *task);
void task_switchsub(void);
void task_idle(void);

struct TASK *task_init(struct MEMORYMANAGER *memoryManager)
{
    int i;
    struct TASK *task, *idle;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADRESS_GDT;
    taskControl = (struct TASKCONTROL *)memorymanager_allocate_4k(memoryManager, sizeof(struct TASKCONTROL));
    for (i = 0; i < MAX_TASKS; i++)
    {
        taskControl->tasks0[i].flags = 0;
        taskControl->tasks0[i].selector = (TASK_GDT0 + i) * 8;
        set_segment_descriptor(gdt + TASK_GDT0 + i, 103, (int)&taskControl->tasks0[i].tss, AR_TSS32);
    }
    for (i = 0; i < MAX_TASKLEVELS; i++)
    {
        taskControl->levels[i].running = 0;
        taskControl->levels[i].now = 0;
    }
    task = task_allocate();
    task->flags = 2;    //動作中マーク
    task->priority = 2; //0.02秒
    task->level = 0;    //最高レベル
    task_add(task);
    task_switchsub(); //レベル設定
    load_tr(task->selector);
    taskTimer = timer_allocate();
    timer_set_time(taskTimer, task->priority);

    // Task の番兵である task_idle をセットする
    idle = task_allocate();
    idle->tss.esp = memorymanager_allocate_4k(memoryManager, 64 * 1024) + 64 * 1024;
    idle->tss.eip = (int)&task_idle;
    idle->tss.es = 1 * 8;
    idle->tss.cs = 2 * 8;
    idle->tss.ss = 1 * 8;
    idle->tss.ds = 1 * 8;
    idle->tss.fs = 1 * 8;
    idle->tss.gs = 1 * 8;
    task_run(idle, MAX_TASKLEVELS - 1, 1);

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

struct TASK *task_now(void)
{
    struct TASKLEVEL *taskLevel = &taskControl->levels[taskControl->nowLevel];
    return taskLevel->tasks[taskLevel->now];
}

void task_add(struct TASK *task)
{
    struct TASKLEVEL *taskLevel = &taskControl->levels[task->level];
    if (taskLevel->running >= MAX_TASKS_LEVEL)
        return;

    taskLevel->tasks[taskLevel->running] = task;
    taskLevel->running++;
    task->flags = 2;
    return;
}

void task_remove(struct TASK *task)
{
    int i;
    struct TASKLEVEL *taskLevel = &taskControl->levels[task->level];

    for (i = 0; i < taskLevel->running; i++)
    {
        if (taskLevel->tasks[i] == task)
        {
            break;
        }
    }

    taskLevel->running--;
    if (i < taskLevel->now)
    {
        taskLevel->now--;
    }

    if (taskLevel->now >= taskLevel->running)
    {
        taskLevel->now = 0;
    }
    task->flags = 1;

    for (; i < taskLevel->running; i++)
    {
        taskLevel->tasks[i] = taskLevel->tasks[i + 1];
    }
    return;
}

void task_run(struct TASK *task, int level, int priority)
{
    if (level < 0)
    {
        level = task->level; //レベルを変更しない
    }
    if (priority > 0)
    {
        task->priority = priority;
    }

    if (task->flags == 2 && task->level != level)
    {
        task_remove(task);
    }
    if (task->flags != 2) //スリープ状態から起動する場合
    {
        task->level = level;
        task_add(task);
    }
    taskControl->levelChange = 1; //次回タスクスイッチの時にレベルを見直す
    return;
}

void task_switch(void)
{
    struct TASKLEVEL *taskLevel = &taskControl->levels[taskControl->nowLevel];
    struct TASK *newTask, *nowTask = taskLevel->tasks[taskLevel->now];
    taskLevel->now++;
    if (taskLevel->now == taskLevel->running)
    {
        taskLevel->now = 0;
    }
    if (taskControl->levelChange != 0)
    {
        task_switchsub();
        taskLevel = &taskControl->levels[taskControl->nowLevel];
    }
    newTask = taskLevel->tasks[taskLevel->now];
    timer_set_time(taskTimer, newTask->priority);
    if (newTask != nowTask)
    {
        farjump(0, newTask->selector);
    }
    return;
}

void task_switchsub(void)
{
    int i;
    for (i = 0; i < MAX_TASKLEVELS; i++)
    {
        if (taskControl->levels[i].running > 0)
        {
            break;
        }
    }
    taskControl->nowLevel = i;
    taskControl->levelChange = 0;
    return;
}

void task_sleep(struct TASK *task)
{
    struct TASK *nowTask;
    if (task->flags == 2)
    {
        nowTask = task_now();
        task_remove(task); //これを実行するとflagは1になる
        if (task == nowTask)
        {
            task_switchsub(); //自分自身がSleepするのでタスクスイッチが必要
            nowTask = task_now();
            farjump(0, nowTask->selector);
        }
    }
    return;
}

void task_idle(void)
{
    for (;;)
    {
        io_hlt();
    }
}

