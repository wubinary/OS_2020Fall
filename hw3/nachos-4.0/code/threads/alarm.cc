// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to 
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom)
{
    timer = new Timer(doRandom, this);
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice 
//      if we're currently running something (in other words, not idle).
//	Also, to keep from looping forever, we check if there's
//	nothing on the ready list, and there are no other pending
//	interrupts.  In this case, we can safely halt.
//----------------------------------------------------------------------

void 
Alarm::CallBack() 
{
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();
	
	bool woken = _sleepList.PutToReady();    
    if (status == IdleMode && !woken && _sleepList.IsEmpty()) {	
		// is it time to quit?
        if (!interrupt->AnyFutureInterrupts()) {
	    	timer->Disable();	// turn off the timer
		}
    } else {			// there's someone to preempt
		if (kernel->scheduler->getSchedulerType()==RR || 
			kernel->scheduler->getSchedulerType() == Priority){
			cout << "=== interrupt->YieldOnReturn ===" << endl;
			interrupt->YieldOnReturn();
		}
    }
}

void
Alarm::WaitUntil(int x){
	//關中斷
	IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
	Thread* t = kernel->currentThread;

	int worktime = kernel->stats->userTicks - t->getStartTime();
	t->setBurstTime(t->getBurstTime()+worktime);
	t->setStartTime(kernel->stats->userTicks);

	//cout << "Alarm::WaitUntil go sleep" << endl;
	_sleepList.PutToSleep(t, x);
	//開中斷
	kernel->interrupt->SetLevel(oldLevel);
}

bool
sleepList::IsEmpty(){
	return _threadlist.size() == 0;
}

void
sleepList::PutToSleep(Thread *t, int x){
	ASSERT(kernel->interrupt->getLevel() == IntOff);
	_threadlist.push_back(sleepThread(t, _current_interrupt + x));
	t->Sleep(false);
}

bool
sleepList::PutToReady() {
	bool woken=false;
	_current_interrupt ++;
	for(std::list<sleepThread>::iterator it = _threadlist.begin(); it!=_threadlist.end(); ){
		if(_current_interrupt >= it->when){
			woken=true;
			//cout << "sleepList::PutToReady Thread woken" << endl;
			kernel->scheduler->ReadyToRun(it->sleeper);
			it = _threadlist.erase(it);
		} else {
			it++;
		}
	}
	return woken;
}



