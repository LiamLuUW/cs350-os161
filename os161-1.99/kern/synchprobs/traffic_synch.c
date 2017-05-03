#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;

struct cv *Readytogo; //cv used to check if a car can enter the intersection or not
static int PassCheck(Direction origin, Direction des);
static void EnterIntersection(Direction origin, Direction des);
static void ExitIntersection(Direction origin, Direction des);
// lock for direction switch
struct lock *IntersectionLock;

int signal[12]={0}; //the signal array that used to save each trip's condition
/*  0 means N->S
    1 means N->W
    2 means N->E
    3 means S->N
    4 means S->E
    5 means S->W
    6 means E->N
    7 means E->S
    8 means E->W
    9 means W->E
    10 means W->S
    11 means W->N
*/

/*Helper Function*/

/*
This function take a origin and destination 
and return 0 if a car if allowed to enter for this trip
else return a number >0
*/
int PassCheck(Direction origin, Direction des){
    if(origin == north && des == south){
        return signal[0];
    }else if(origin == north && des == west){
        return signal[1];
    }else if(origin == north && des == east){
        return signal[2];
    }else if (origin == south && des == north){
        return signal[3];
    }else if(origin == south && des == east){
        return signal[4];
    }else if(origin == south && des == west){
        return signal[5];
    }else if(origin == east && des == north){
        return signal[6];
    }else if(origin == east && des == south){
        return signal[7];
    }else if(origin == east && des == west){
        return signal[8];
    }else if (origin == west && des == east){
        return signal[9];
    }else if(origin == west && des == south){
        return signal[10];
    }else if(origin == west && des == north){
        return signal[11];
    }else{
      return 99;
      panic("WRONG DIRECTION CHECK\n；");
    }

}

/*
    This function updates the Signal Array when a car is entered
*/
void EnterIntersection(Direction origin, Direction des){
    if(origin == north && des == south){
         signal[5]++;
         signal[11]++;
         signal[9]++;
         signal[7]++;
         signal[8]++;
         signal[10]++;
    }else if(origin == north && des == west){
         signal[5]++;
         signal[8]++;
    }else if(origin == north && des == east){
         signal[3]++;
         signal[5]++;
         signal[7]++;
         signal[8]++;
         signal[9]++;
         signal[11]++;
         signal[4]++;
    }else if (origin == south && des == north){
         signal[9]++;
         signal[11]++;
         signal[2]++;
         signal[8]++;
         signal[7]++;
         signal[6]++;
    }else if(origin == south && des == east){
         signal[2]++;
         signal[9]++;
    }else if(origin == south && des == west){
         signal[0]++;
         signal[2]++;
         signal[7]++;
         signal[8]++;
         signal[9]++;
         signal[11]++;
         signal[1]++;
    }else if(origin == east && des == north){
         signal[3]++;
         signal[11]++;
    }else if(origin == east && des == south){
         signal[0]++;
         signal[2]++;
         signal[9]++;
         signal[11]++;
         signal[3]++;
         signal[5]++;
         signal[10]++;
    }else if(origin == east && des == west){
         signal[3]++;
         signal[5]++;
         signal[0]++;
         signal[2]++;
         signal[11]++;
         signal[1]++;
    }else if (origin == west && des == east){
         signal[0]++;
         signal[2]++;
         signal[3]++;
         signal[5]++;
         signal[7]++;
         signal[4]++;
    }else if(origin == west && des == south){
         signal[0]++;
         signal[7]++;
    }else if(origin == west && des == north){
         signal[0]++;
         signal[2]++;
         signal[3]++;
         signal[5]++;
         signal[7]++;
         signal[8]++;
         signal[6]++;
    }else{
      panic("WRONG DIRECTION CHECK\n；");
    }

}

/*
    This function unflag the signal array when a car exit the intersection
*/
void ExitIntersection(Direction origin, Direction des){
     if(origin == north && des == south){
         signal[5]--;
         signal[11]--;
         signal[9]--;
         signal[7]--;
         signal[8]--;
         signal[10]--;
    }else if(origin == north && des == west){
         signal[5]--;
         signal[8]--;
    }else if(origin == north && des == east){
         signal[3]--;
         signal[5]--;
         signal[7]--;
         signal[8]--;
         signal[9]--;
         signal[11]--;
         signal[4]--;
    }else if (origin == south && des == north){
         signal[9]--;
         signal[11]--;
         signal[2]--;
         signal[8]--;
         signal[7]--;
         signal[6]--;
    }else if(origin == south && des == east){
         signal[2]--;
         signal[9]--;
    }else if(origin == south && des == west){
         signal[0]--;
         signal[2]--;
         signal[7]--;
         signal[8]--;
         signal[9]--;
         signal[11]--;
         signal[1]--;
    }else if(origin == east && des == north){
         signal[3]--;
         signal[11]--;
    }else if(origin == east && des == south){
         signal[0]--;
         signal[2]--;
         signal[9]--;
         signal[11]--;
         signal[3]--;
         signal[5]--;
         signal[10]--;
    }else if(origin == east && des == west){
         signal[3]--;
         signal[5]--;
         signal[0]--;
         signal[2]--;
         signal[11]--;
         signal[1]--;
    }else if (origin == west && des == east){
         signal[0]--;
         signal[2]--;
         signal[3]--;
         signal[5]--;
         signal[7]--;
         signal[4]--;
    }else if(origin == west && des == south){
         signal[0]--;
         signal[7]--;
    }else if(origin == west && des == north){
         signal[0]--;
         signal[2]--;
         signal[3]--;
         signal[5]--;
         signal[7]--;
         signal[8]--;
         signal[6]--;
    }else{
      panic("WRONG DIRECTION CHECK\n；");
    }

}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{

  IntersectionLock = lock_create("IntersectionLock");
  if (IntersectionLock == NULL) {
    panic("could not create IntersectionLock lock");
  }


  Readytogo = cv_create("");
  if (Readytogo == NULL) {
    panic("could not create Readytogo cv");
  }

}
/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  //kprintf("cleaning1!!!!!!!\n");
  /* replace this default implementation with your own implementation */
  KASSERT(IntersectionLock != NULL);
  cv_destroy(Readytogo); //destroy the cv

  lock_destroy(IntersectionLock); // destroy the lock
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */

  KASSERT(IntersectionLock != NULL);

  lock_acquire(IntersectionLock); //lock the intersection


    while(PassCheck(origin, destination)!=0){ //enter the loop if the signal of this incoming car is >0 (blocked)
        cv_wait(Readytogo, IntersectionLock); //send this car to sleep
    }

    //Allow to enter intersection
    EnterIntersection(origin, destination); //update signal

  lock_release(IntersectionLock);

}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  KASSERT(IntersectionLock != NULL);

 lock_acquire(IntersectionLock);

     ExitIntersection(origin, destination); //Unflag the signal array
     cv_broadcast(Readytogo,IntersectionLock); //tell all the other cars that in the wchan that I left

  lock_release(IntersectionLock);
  //kprintf("Lock released from exit \n");

}
