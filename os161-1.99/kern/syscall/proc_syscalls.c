#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include "mips/trapframe.h"
#include <synch.h>
#include <limits.h>
#include <vm.h>
#include <vfs.h>
#include <kern/fcntl.h>

#ifdef OPT_A2

/*Helper function*/
// check if the pid is inside the array
static bool CheckPidExist(struct array *list, pid_t pid){
  int numoflist = array_num(list);
  if(numoflist!= 0){
    for(int i=0; i< numoflist; i++){
      struct proc *tempproc = array_get(list,i);
      if(tempproc->mypid == pid){
          return true;
      }
    }
  }else{
    return false;
  }
  return false;
}




#endif
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */


void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

#ifdef OPT_A2
  KASSERT(p!= NULL);

  //set this proc to exit but not destroy yet
 
  p->p_exited = true;
  p->p_exitcode = _MKWAIT_EXIT(exitcode);
  // kprintf("I am allowed to exit : %d and exitcode is set!\n", p->mypid);
  //since this proc is exited then it will not call waitpid anymore,
  //so we can set this proc's children's parent pointer to be NULL
  int numofchild = array_num(&p->mychildren);
  //kprintf("numofchid = %d\n",numofchild);
  if(numofchild!=0){
    for(int i=0; i< numofchild; i++){
      
      struct proc *child = array_get(&p->mychildren, i); //get actual child proc
      KASSERT(child != NULL);
      //kprintf("this chils's pid is %d\n", child->mypid);
      child->myparent = NULL; //detach the parent from this child
      cv_broadcast(child->exit_cv,child->exit_lk);
    }
  }


  lock_acquire(p->waitpid_lk);
  cv_broadcast(p->waitpid_cv, p->waitpid_lk); // notfify my parent i EXITED
  lock_release(p->waitpid_lk);
  /*
    This cv check's if the parent of this proc has exited or not, if not then it will put this 
    proc to wait and only allow proc to destory after its parent has exited
  }*/

  lock_acquire(p->exit_lk);
  while(p->myparent != NULL){
    cv_wait(p->exit_cv,p->exit_lk);
  }
  lock_release(p->exit_lk);
  //now this proc is allow to exit 
  //kprintf("I am allowed to destroy : %d\n", p->mypid);

#endif

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  //kprintf("I am allowed to destroy : %d\n", p->mypid);

  proc_destroy(p);
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #ifdef OPT_A2
    //kprintf("GETPID CALLED");
    *retval = curproc->mypid;
    return(0);
  #else
    *retval = 1;
    return(0);
  #endif
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
#ifdef OPT_A2
  //check if the pid exits or not
  if(ptable[pid]== NULL) return ESRCH;

  //check if the proc is calling its own children
  bool childexist = CheckPidExist(&curproc->mychildren, pid);;
  if(!childexist) return ECHILD;

  // now we get the child proc
    struct proc *child = ptable[pid];
   // kprintf("Witspid called child's mypid is %d\n Calling pid is %d\n", child->mypid,pid);
    KASSERT(child->mypid == pid);

    //if the child is not exited then put parent proc into its wait channel
    lock_acquire(child->waitpid_lk);
    while(child->p_exited == false){
        cv_wait(child->waitpid_cv, child->waitpid_lk);
    }
    lock_release(child->waitpid_lk);

    //get the exit code after the child has exited
    exitstatus =child->p_exitcode;

    result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  //assign return value
  *retval = pid;
  return(0);
}

#else
  exitstatus =0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}
#endif


#ifdef OPT_A2


//struct lock *forklock;

int sys_fork(struct trapframe *ptf, pid_t *retval){

  struct proc *mychild;
  struct proc *parent = curproc;

  //create a child process structure
  mychild = proc_create_runprogram("child");
 // kprintf("fork called %d is now created\n",mychild->mypid);
  if (mychild == NULL) {
    kprintf("Cannot create child process");
    return EMPROC; 
  }


  // Create and copy address space

  int error = as_copy(parent->p_addrspace, &(mychild->p_addrspace));
  if (error != 0){
    kprintf("Cannot copy as for the child");
    proc_destroy(mychild);
    return ENPROC;
  }


  // Assign the child-parent relationship
  mychild->myparent = curproc;
  // put this new child into mychildren array
  array_add(&parent->mychildren, mychild, NULL);

  
  //Create thread for the child
  struct trapframe *tfcopy = kmalloc(sizeof(struct trapframe));
  if (tfcopy == NULL) {
    kprintf("Cannot create tfcopy");
    return ENPROC; 
  }


  //lock_acquire(fork_lk);
  memcpy(tfcopy, ptf, sizeof(struct trapframe)); // copy parent trapframe into the heap's tfcopy
  //lock_release(fork_lk);


  int error2 = thread_fork(mychild->p_name, mychild, &enter_forked_process, tfcopy, mychild->mypid);
  if (error2 != 0){
    kprintf("Cannot create thread_fork");
    proc_destroy(mychild);
    kfree(tfcopy);
    return ENPROC; 
  }

  *retval = mychild->mypid; //return value for child
  return 0;
}


int sys_execv(const char *program, char **args){
    struct addrspace *as;
    struct addrspace *oldas;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
  

  if (program == NULL){
    kprintf("program is a NULL pointer");
    return ENOENT;
  }

  //get the current old address space
  oldas = curproc_getas();
  KASSERT(oldas!=NULL); 

  //copy args into kernal space
  char **temp = args; // put it onto kernal stack, temp use only
  if(args== NULL){
    return EFAULT;
  }

 //count the number of arguments
  int j=0; //just an interator
  int numofargc = 0;

  while(temp[j] != NULL){
    //numofargc = strlen(args[i]);
    numofargc++;
    j++;
  }
 // kprintf("num of args checked: %d\n",numofargc);
  //check num of args not exceed limit
  if(numofargc > ARG_MAX){
    return E2BIG;
  }

  char **arglist = kmalloc(sizeof(char *) * (numofargc+1)); // add 1 for null, the argc list on kernal
  if(arglist == NULL) return ENOMEM;
  

  int argc_size_list[numofargc]; //a list used to contain each argc's size
  int copyerror; //error code

  for(int i=0; i<numofargc; i++){
    char *kargc = temp[i]; // get each argc's address from temp

    //get each argv length
    argc_size_list[i]=strlen(kargc)+1; 
   // kprintf("%d's length is: %d\n", i,argc_size_list[i] );
    arglist[i] = kmalloc(sizeof(char) * PATH_MAX); //allocate space in my kenal space
    copyerror = copyinstr((const_userptr_t)kargc, arglist[i], PATH_MAX, NULL);
    if(copyerror){
      //kprintf("iamhere\n");
      kfree(arglist);
      //kfree(temp);
      return copyerror;
    }
    //kprintf(arglist[i]);
    //kprintf("\n");
  }

  arglist[numofargc] = NULL; //set the last argc to be NULL in the array

  //copy the program path into kernal stack
  char *program_copy= kmalloc(sizeof(char) * PATH_MAX);
  if(program_copy==NULL) return ENOMEM;

  int pathcopyerror = copyinstr((const_userptr_t)program,program_copy, PATH_MAX, NULL);

  if(pathcopyerror){
    return pathcopyerror;
  }


  //open the program file
   int fileopenerror = vfs_open(program_copy, O_RDONLY, 0, &v);
  if (fileopenerror) {
    return fileopenerror;
  }


  //create new address space and activate it

  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  int loaderror = load_elf(v, &entrypoint);
  if (loaderror) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return loaderror;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  int stackerror = as_define_stack(as, &stackptr);
  if (stackerror) {
    //kfree(myagrv);
    /* p_addrspace will go away when curproc is destroyed */
    return stackerror;
  }

//copy argvs into new user space
  vaddr_t  new_arglist[numofargc+1]; //an array of vaddress use to save each argc's address, add 1 for NULL

  for(int i=numofargc-1; i>=0; i--){
      char * new_argc = arglist[i]; //get from kenal's argv list
     // kprintf(new_argc);
      //kprintf("\n");
      int arg_length = argc_size_list[i]; //get length from the size array
      stackptr = stackptr - arg_length; //update stack pointer
      stackerror = copyoutstr(new_argc, (userptr_t) stackptr, arg_length+1, NULL);
      if(stackerror){
        kfree(arglist);
        kfree(program_copy);
        return stackerror;
      }
      new_arglist[i]= stackptr; //save this argv's address into address array

  }
  new_arglist[numofargc]= (vaddr_t) NULL; //assign address for NULL

  int last_length = argc_size_list[0]; //this is the last copyed string's length
  //kprintf("stack pointer is ar : %04x \n",stackptr);

  if(stackptr%8!=0){ //check if the last stack address is divisible by 8
    stackptr= ROUNDUP(stackptr-last_length, 8); //if not we align it by 8
  //kprintf("temp is at : %04x \n", stackptr);
  }

  //now we need save address pointerss
  //first the null pointer, then others
  for(int i=numofargc;i>=0; i--){
    stackptr = stackptr - 4; //since these are char* fix size , just make them all to be 4 bytes
    stackerror = copyout((char *)&new_arglist[i], (userptr_t) stackptr, 4);
    if(stackerror){
      kfree(arglist);
      kfree(program_copy);
      return stackerror;
    }
  }

  if(stackptr%8!=0){ //check if the stackpointer is divisible by 8
    stackptr= ROUNDUP(stackptr-4, 8); //fot the stack pointer we aligned it 8 bytes
  }

//frees old address space and others
  kfree(program_copy);
  as_destroy(oldas);
  kfree(arglist);

  //kfree(myagrv);
  /* Warp to user mode. */
  enter_new_process(numofargc /*argc*/, (userptr_t) stackptr /*userspace addr of argv*/,
        stackptr, entrypoint);

  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}
#endif
