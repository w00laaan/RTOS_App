#include "includes.h"
#define  TASK_STK_SIZE                 512       /* Size of each task's stacks (# of WORDs)            */
#define  N_TASKS                        15       /* Number of identical tasks                          */
#define  NULLPTR								 ((void*)0)	//Define new NULL
#define  queueLength                             100
#define  mailboxesCount                          5

typedef struct {			//Between-task communictaion structure
	INT32U taskID;			//Unique task ID
	INT32U currLoad;		//Current task's load
	INT32U taskIterations;	//Task finished iterations
	INT32U givenLoad;		//Zadane currLoad
	INT32U delta;			//Task executions per second
	INT8U taskState;		//DONE or BUSY state
	INT32U serialNumber;	//Unique data identificator
}	dataFrame;

OS_STK        TaskStacks[N_TASKS][TASK_STK_SIZE]; //Tasks' stacks
OS_STK        TaskStartStk[TASK_STK_SIZE];
OS_STK        inputTaskStk[TASK_STK_SIZE];
OS_STK        inputInterpreterTaskStk[TASK_STK_SIZE];
OS_STK        displayTaskStk[TASK_STK_SIZE];
OS_STK        broadcastTaskStk[TASK_STK_SIZE];

INT32U globalValue = 0;											//Global value for semaphore load tasks
 
																/*OS_EVENT creation*/
OS_EVENT* loadReceiveMbox = NULLPTR;							//MBOX used to receive load from interpreter task
OS_EVENT* displayQueue = NULLPTR;								//Queue for display task
OS_EVENT* loadTaskQueue = NULLPTR;								//Queue for receiving load for Q load tasks
OS_EVENT* globalTaskSemaphore = NULLPTR;						//Semaphore
OS_EVENT* loadTaskMbox[mailboxesCount] = { NULLPTR };			//Group of mailboxes for receiving load for M load tasks
OS_EVENT* interpreterDataQueue = NULLPTR;						//Queue for receiving input from keyboard
OS_MEM*   Q_loadTaskMemPart = NULLPTR;							//Q load tasks memory partition for passing structures
OS_MEM*   M_loadTaskMemPart = NULLPTR;							//M load tasks memory partition for passing structures
INT32U    mboxMemPartArray[queueLength] = { 0 };				//Array neaded to create memory partition 
void*     displayQueueArray[queueLength] = {NULLPTR};			//Array used to create queue for display task
void* loadTaskQueueArray[queueLength] = { NULLPTR };			//Array used to create queue for queue load task
void*     interpreterDataQueueArray[queueLength] = {NULLPTR};	//Array used to create queue for receiving input from keyboard
dataFrame     Q_loadTasksMemPartArray[queueLength];				//Array used to create queue for Q load tasks queue
char	  tasksArray[N_TASKS];									//Task creation array
void	  TaskStart(void *data);								//Function prototypes
void	  TaskStartCreateTasks(void);
void	  TaskStartDispInit(void);
void	  TaskStartDisp(void);
void	  inputTask(void *pdata);
void	  interpreterTask(void *pdata);
void	  displayTask(void *pdata);
void	  broadcastTask(void *pdata);
void	  TaskSem(void *pdata);
void	  TaskMail(void *pdata);
void	  TaskQ(void *pdata);
 
void  main (void) {
	INT8U err;
 
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);      /* Clear the screen                         */
    OSInit();                                              /* Initialize uC/OS-II                      */
    PC_DOSSaveReturn();                                    /* Save environment to return to DOS        */
    PC_VectSet(uCOS, OSCtxSw);                             /* Install uC/OS-II's context switch vector */

	Q_loadTaskMemPart = OSMemCreate(&Q_loadTasksMemPartArray[0], queueLength, sizeof(dataFrame), &err);		//Partition creation
	M_loadTaskMemPart = OSMemCreate(&mboxMemPartArray[0], queueLength, sizeof(INT32U), &err);				//Partition creation
    OSTaskCreate(TaskStart, NULLPTR, &TaskStartStk[TASK_STK_SIZE - 1], 0);									//Create tasks

    OSStart();                                             /* Start multitasking                       */
}
 

void  TaskStart (void *pdata) {
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU taskState register */
    OS_CPU_SR  cpu_sr;
#endif

	INT8U i = 0;
	dataFrame counter;
	counter.taskID = 22;
 
    pdata = pdata;                                         /* Prevent compiler warning                 */
 
    TaskStartDispInit();                                   /* Initialize the display                   */
 
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);                           /* Install uC/OS-II's clock tick ISR        */
    PC_SetTickRate(OS_TICKS_PER_SEC);                      /* Reprogram tick rate                      */
    OS_EXIT_CRITICAL();
 
    OSStatInit();                                          /* Initialize uC/OS-II's statistics         */
    TaskStartCreateTasks();                                /* Create all the application tasks         */
 
	interpreterDataQueue = OSQCreate(&interpreterDataQueueArray[0], queueLength);				//Queue to handle input from keyboard
	displayQueue = OSQCreate(&displayQueueArray[0], queueLength);								//Queue to handle data to be displayed
	loadReceiveMbox = OSMboxCreate(NULLPTR);													//MBOX to receive new load value
	for(i = 0; i < mailboxesCount; i++)															//5 mailboxes creation - M tasks
		loadTaskMbox[i] = OSMboxCreate(NULLPTR);
	loadTaskQueue = OSQCreate(&loadTaskQueueArray[0], queueLength);								//Queue to pass data to Q load tasks
	globalTaskSemaphore = OSSemCreate(1);														//Semaphore for purposes of global tasks
 
    for (;;) {
        TaskStartDisp();									    /* Update the display                       */
        OSCtxSwCtr = 0;    										/* Clear context switch counter             */
		OSQPost(displayQueue, &counter);						//Update delta parameter
		OSTimeDly(OS_TICKS_PER_SEC);    						/* Wait 1 second                          */
	}
}
 

static  void  TaskStartDispInit (void) {		//Display initialization

    PC_DispStr( 0,  0, "                      SYSTEMY OPERACYJNE LABORATORIUM RTOS                       ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
    PC_DispStr( 0,  1, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  2, "New load value            :                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  3, "Current load value        :                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  4, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  5, "Task    Priority        Load           Cycles       Delta                State   ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
    PC_DispStr( 0,  6, "QTask1                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  7, "QTask2                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  8, "QTask3                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  9, "QTask4                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 10, "QTask5                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 11, "MTask1                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 12, "MTask2                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 13, "MTask3                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 14, "MTask4                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 15, "MTask5                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 16, "GTask1                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 17, "GTask2                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 18, "GTask3                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 19, "GTask4                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 20, "GTask5                                                                           ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 21, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 22, "#Tasks          :            CPU Usage:       %                                  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 23, "#Task switch/sec:              SysFreq:       Hz                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 24, "                            <-PRESS 'ESC' TO QUIT->                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
	
}
 
static  void  TaskStartDisp (void) {
    char   string[80];
 
    sprintf(string, "%5d", OSTaskCtr);                                  /* Display #tasks running               */
    PC_DispStr(18, 22, string, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
#if OS_TASK_STAT_EN > 0
    sprintf(string, "%3d", OSCPUUsage);                                 /* Display CPU usage in %               */
    PC_DispStr(40, 22, string, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#endif
 
    sprintf(string, "%5d", OSCtxSwCtr);                                 /* Display #context switches per second */
    PC_DispStr(18, 23, string, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
    sprintf(string, "V%1d.%02d", OSVersion() / 100, OSVersion() % 100); /* Display uC/OS-II's version number    */
    PC_DispStr(75, 24, string, DISP_FGND_YELLOW + DISP_BGND_BLUE);
	sprintf(string, " %3d", OS_TICKS_PER_SEC);
	PC_DispStr(40, 23,string, DISP_FGND_YELLOW + DISP_BGND_BLUE);//wyświetlenie częstotliwości pracy systemu
	
    switch (_8087) {                                               /* Display whether FPU present          */
        case 0:
             PC_DispStr(71, 22, " NO  FPU ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 1:
             PC_DispStr(71, 22, " 8087 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 2:
             PC_DispStr(71, 22, "80287 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 3:
             PC_DispStr(71, 22, "80387 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
    }
}

 
static  void  TaskStartCreateTasks (void) {
    INT8U  i = 0;
	INT16U err[19] = {0};

	err[0] = OSTaskCreate(inputTask, NULLPTR, &inputTaskStk[TASK_STK_SIZE - 1], 1);
	err[1] = OSTaskCreate(interpreterTask, NULLPTR, &inputInterpreterTaskStk[TASK_STK_SIZE - 1], 2);
	err[2] = OSTaskCreate(displayTask, NULLPTR, &displayTaskStk[TASK_STK_SIZE - 1], 3);
	err[3] = OSTaskCreate(broadcastTask, NULLPTR, &broadcastTaskStk[TASK_STK_SIZE - 1], 4);
 
	for(i = 0; i < 15; i++)		//Task ID setting
		tasksArray[i] = i + 5;  //ID matches priority

	for (i = 0; i < 5; i++) {
		err[i + 4]  = OSTaskCreate(TaskQ, &tasksArray[i], &TaskStacks[i][TASK_STK_SIZE - 1], i + 5);
		err[i + 9]  = OSTaskCreate(TaskMail, &tasksArray[i + 5], &TaskStacks[i + 5][TASK_STK_SIZE - 1], i + 10);
		err[i + 14] = OSTaskCreate(TaskSem, &tasksArray[i + 10], &TaskStacks[i + 10][TASK_STK_SIZE - 1], i + 15);
	}
	for(i = 0; i < 19; i++)
		if (err[i] != OS_NO_ERR) {							//In case ANY task creation failed, program will exit
				PC_DispStr(0, 4, "!!!!!!!!!!ERROR HAS OCCURED DURING INITIALIZATION PROGRAM WILL NOW EXIT!!!!!!!!!!", DISP_FGND_YELLOW + DISP_BGND_RED);
				getch();
				PC_DOSReturn();
		}
}
 

void inputTask(void *pdata) { //Scan for input from keyboard
	INT16S key;
	pdata = pdata;
	for(;;)
	{
		if(PC_GetKey(&key) == TRUE)
			OSQPost(interpreterDataQueue, &key);	//Pass data to interpreter task
		OSTimeDly(9);
	}
}
 
void interpreterTask(void *pdata) {
	INT16S* keyPressedPtr = NULLPTR;	//Variable to store received pointer
	INT16S keyPressed = 0;				//Variable to store received pointer's value
	char currValue[11] = {0};			//Temp buffer
	INT8U err = 0;						//Error value handling
	INT8S cursor = 0;					//Current cursor location
	INT8U i = 0;
	dataFrame data, toDisplay;
	data.taskID  = 2;					//Set value for display algorithm purposes
	toDisplay.taskID = 23;				//Set value for display algorithm purposes
	pdata = pdata;						//Prevent compiler warning
	currValue[10] = '\0';				//String end sign
	for(;;) {
		keyPressedPtr = OSQPend(interpreterDataQueue, 0, &err);
		if(keyPressedPtr != NULLPTR)
			keyPressed = *keyPressedPtr;

		
		switch (keyPressed) {
			case 0x08:	//Backspace
				cursor--;
				currValue[cursor] = ' ';
				if (cursor <= 0) {
					currValue[0] = ' ';
					cursor = 0;				//Make sure not to exceed limits
				}
				toDisplay.givenLoad = strtoul(currValue, NULLPTR, 10);	//Convert string to unsigned long
				OSQPost(displayQueue, &toDisplay);			//Send frame to display
				break;

			case 0x0D: //Enter
				data.currLoad = strtoul(currValue, NULLPTR, 0);
				OSMboxPost(loadReceiveMbox, &data);		//Send to broadcast
				data.currLoad = strtoul(currValue, NULLPTR, 0);
				OSQPost(displayQueue, &data);			//Send to display
				cursor = 0;
				for (i = 0; i < 10; i++)					//Clear screen
					currValue[i] = ' ';

				toDisplay.givenLoad = strtoul(currValue, NULLPTR, 0);
				OSQPost(displayQueue, &toDisplay);
				if (cursor) {
					do {
						currValue[cursor] = ' ';
						cursor--;
					} while (cursor);
					currValue[cursor] = ' ';
				}
				break;

			case 0x1B:
				PC_DOSReturn();
				break;

			default:
				if (isdigit(keyPressed)) {		//Is number?
					if (cursor < 10) {			//Ignore everything if buffer size is exceeded
						currValue[cursor] = keyPressed;
						toDisplay.givenLoad = strtoul(currValue, NULLPTR, 10);
						OSQPost(displayQueue, &toDisplay);
						cursor++;
					}
					else						//Make sure not to exceed limits
						cursor = 10;
				}
				break;
		}
	}
}
 
void broadcastTask(void *pdata) {
	INT8U  err;
	INT32U i = 0, localSerialNumber = 1;
	INT32U *newLoadPtr = NULLPTR;
	char buffer[32];
	dataFrame *receivedData = NULLPTR;
	dataFrame *mboxTaskData = NULLPTR; 
	dataFrame *queueTaskData[5] = {NULLPTR};
	//dataFrame* tmp = NULLPTR;

	pdata = pdata;

	for (i = 0; i < 5; i++)				//Serial number init
		queueTaskData[i]->serialNumber = 0;

	for(;;)
	{
		receivedData = OSMboxPend(loadReceiveMbox, 0, &err); //Receive load from interpreter task

		if(err == OS_NO_ERR){								
			OSSemPend(globalTaskSemaphore, 0, &err);
			globalValue = receivedData->currLoad;
			OSSemPost(globalTaskSemaphore);

			for(i = 0; i < 5; i++) {						//Broadcast frame to load tasks and disp
				
				newLoadPtr = OSMemGet(M_loadTaskMemPart, &err);
				*newLoadPtr = receivedData->currLoad;
  
				if(OSMboxPost(loadTaskMbox[i], newLoadPtr)) {	//IF NOT EMPTY -> MAKE EMPTY AND NOTICE USER
					mboxTaskData = OSMemGet(Q_loadTaskMemPart, NULLPTR);
					mboxTaskData->taskID = i + 10;
					mboxTaskData->taskState = 17;
					OSQPost(displayQueue, mboxTaskData);
					mboxTaskData = OSMboxAccept(loadTaskMbox[i]);
					OSMboxPost(loadTaskMbox[i], newLoadPtr);
				}

				queueTaskData[i] = OSMemGet(Q_loadTaskMemPart, &err);
				queueTaskData[i]->taskID = i + 5;
				queueTaskData[i]->currLoad = receivedData->currLoad;
				queueTaskData[i]->serialNumber = localSerialNumber;
 
				err = OSQPost(loadTaskQueue, queueTaskData[i]);		//Future error handling
			}
			localSerialNumber++;									//Increase serial number
			
		}
		
	}
}
 
void displayTask(void *pdata) {
	dataFrame *daneDisplay = NULLPTR;		//Receiveing structure
	INT8U err = 0;
	INT32U newDelta[15] = {0};
	char loadString[16] = {0};				//String buffers for PC_DispStr purposes
	char processedLoadBuffer[16] = {0};
	char givenLoadString[16] = {0};
	char deltaString[16] = {0};
	char taskIterationsBuffer[16] = {0};
	char taskIDBuffer[2] = {0};
	char taskStateBuffer[6] = {0};
	INT32U currLoad = 0;
	INT8U i = 0;
 
	pdata = pdata;
	for(;;)
	{
 
		daneDisplay = OSQPend(displayQueue, 0, &err);
 
		sprintf(taskIDBuffer, "%d", daneDisplay->taskID);
		sprintf(loadString, "%10lu", daneDisplay->currLoad);
		sprintf(taskIterationsBuffer, "%10lu", daneDisplay->taskIterations);
		if (daneDisplay->taskState == 0)
			sprintf(taskStateBuffer, "DONE");
		else
			sprintf(taskStateBuffer, "BUSY");
 
			if(daneDisplay->taskID == 2) {						//Process new load and prepare screen
				currLoad = daneDisplay->currLoad;
				sprintf(givenLoadString, "%10lu", currLoad);
				PC_DispStr(30, 3, givenLoadString, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
			}
			else if(daneDisplay->taskID == 23) {
				sprintf(processedLoadBuffer, "%10lu", daneDisplay->givenLoad);
				PC_DispStr(30, 2, processedLoadBuffer, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
			}
			else {
 
				if(daneDisplay->taskID>4 && daneDisplay->taskID<20) {
					if(daneDisplay->taskState == 17)		//MBOX FULL - possible data loss
						PC_DispStr(57, daneDisplay->taskID + 1, "DATA_OVERRIDE", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);

					else {
						newDelta[daneDisplay->taskID - 5]++;		//Delta handling
						PC_DispStr(8, daneDisplay->taskID + 1, taskIDBuffer, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(18, daneDisplay->taskID + 1, loadString, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(35, daneDisplay->taskID + 1, taskIterationsBuffer, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(75, daneDisplay->taskID + 1, taskStateBuffer, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(57, daneDisplay->taskID + 1, "             ", DISP_FGND_WHITE + DISP_BGND_LIGHT_GRAY);
						
						}
 
				} 
				else {
					for(i = 0; i < 15; i++) {						//Display delta
							sprintf(deltaString, "%10lu", newDelta[i] / 2);
							PC_DispStr(47, i + 6, deltaString, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
							newDelta[i] = 0;
					}
				}
			} 
	}
}
 
void TaskSem(void *pdata) {
	dataFrame data;
	INT32U i = 0;
	INT32U load = 0;
	data.taskID = *(INT8U *)pdata;			//Task's structure init
	data.currLoad = 0;
	data.taskIterations = 0;
	data.delta = 0;
 
	for(;;) {
		data.taskState = 1;					//Set BUSY state

		OSSemAccept(globalTaskSemaphore);	//Lock semaphore
		data.currLoad = globalValue;		//Set new load value
		OSSemPost(globalTaskSemaphore);		//Release semaphore

		OSQPost(displayQueue, &data);		//Pass data to disp task
 
		for(i = 0; i < data.currLoad; i++)	//Actual load
			load++;

		data.taskIterations++;
		data.taskState = 0;					//Set DONE state
		OSQPost(displayQueue, &data);		//Pass data to disp task

		OSTimeDly(1);						//Suspend
	}
}
 
void TaskMail(void *pdata) {
	INT32U *receivedData;
	dataFrame dataToDisp;
	INT32U i = 0;
	INT32U load = 0;
	dataToDisp.taskID = *(INT8U *)pdata;
 
	for(;;) {
		dataToDisp.taskState = 1;	//Set BUSY state
		receivedData = OSMboxAccept(loadTaskMbox[dataToDisp.taskID - 10]);	//Get data from mailbox adequate to task's ID
			if(receivedData != NULLPTR) {
				dataToDisp.currLoad = *receivedData;
				OSMemPut(M_loadTaskMemPart, receivedData);
			}
 
		OSQPost(displayQueue, &dataToDisp);				//Pass data to disp task
		for(i = 0; i < dataToDisp.currLoad;  i++)		//Actual load
			load++;

		dataToDisp.taskIterations++;					//Increase tasks executions
		dataToDisp.taskState = 0;						//Set DONE state
		OSQPost(displayQueue, &dataToDisp);				//Pass data to disp task

		OSTimeDly(1);
	}
}
 
void TaskQ(void *pdata) {
	dataFrame *daneQueue, daneQueueSend;
	INT32U i = 0;
	INT32U load = 0;
	INT32U savedSerialNumber = 0;
	daneQueueSend.taskID = *(INT8U *)pdata;				//Task's struct init
	daneQueueSend.currLoad = 0;
	daneQueueSend.taskIterations = 0;
	for(;;) {
		daneQueueSend.taskState = 1;					//Set BUSY state
		for (i = 0; i < queueLength; i++) {			//Check if value is dedicated to operating task
			daneQueue = OSQAccept(loadTaskQueue);		//Get struct from queue

			if(daneQueue != NULLPTR) {
				if(daneQueue->taskID == daneQueueSend.taskID && savedSerialNumber + 1 == daneQueue->serialNumber) {		//Perform test to check if frame belongs to THIS tasks
					daneQueueSend.currLoad = daneQueue->currLoad;
					OSMemPut(Q_loadTaskMemPart, daneQueue);
					savedSerialNumber++;
					break;
				}
				else OSQPost(loadTaskQueue, daneQueue);																	//If NOK send back to queue
			}
			else break;
		}
		
		OSQPost(displayQueue, &daneQueueSend);			//Pass data to disp task
 
		for(i = 0; i < daneQueueSend.currLoad; i++)		//Actual load
			load++;

		daneQueueSend.taskIterations++;
		daneQueueSend.taskState = 0;					//Set DONE state
		OSQPost(displayQueue, &daneQueueSend);			//Pass data to disp task

		OSTimeDly(1); 
	}
}
