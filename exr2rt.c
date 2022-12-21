

/*                                                                  
 * POSIX Real Time Example
 * using a single pthread as RT thread
 */
 
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h> /* for sleep() */
#include  <gpiod.h>
#include <stdio.h>
int rt_init(void);
pthread_cond_t cv;
pthread_mutex_t lock;
         pthread_attr_t attr;
        int ret;
        char kbhit(void) ;
const char *chipname = "gpiochip1";
const char *chipname1 = "gpiochip2";
  struct gpiod_chip *chip;
  struct gpiod_chip *chip1;
  struct gpiod_line *lineRed;    // Red LED
  struct gpiod_line *lineGreen ;      //Green LED   
void *thread_func1(void *data)
{
        
       for(;;)
    {
		// Blink LEDs in a binary pattern
  	pthread_mutex_lock(&lock);
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&lock);
     gpiod_line_set_value(lineRed, 1);
     sleep(1);
     gpiod_line_set_value(lineRed, 0);
     sleep(1);
	}
 
}
void *thread_func2(void *data)
{
        
       for(;;)
    {
		// Blink LEDs in a binary pattern
  	pthread_mutex_lock(&lock);
	pthread_cond_wait(&cv, &lock); //counter
	pthread_mutex_unlock(&lock);
     gpiod_line_set_value(lineGreen, 1);
     sleep(1);
     gpiod_line_set_value(lineGreen, 0);
     sleep(1);
	}
 
}
 
int main(int argc, char* argv[])
{

 
	  
        pthread_t thread1,thread2 ;
        /*LEDred*/
         // Open GPIO chip
	  chip = gpiod_chip_open_by_name(chipname);
	  // Open GPIO lines
	  lineRed = gpiod_chip_get_line(chip, 28);
	  // Open LED lines for output
	  gpiod_line_request_output(lineRed, "BONE", 0);
	      /*LEDGreen*/
	   // Open GPIO chip
	  chip1 = gpiod_chip_open_by_name(chipname);
	  // Open GPIO lines
	  lineGreen= gpiod_chip_get_line(chip, 16);
	  // Open LED lines for output
	  gpiod_line_request_output(lineGreen, "BONE", 0);
        
          
        if(rt_init()) exit(0);
    
        
               /* Create a pthread with specified attributes */
        ret = pthread_create(&thread1, &attr, thread_func2, NULL);
        if (ret) {
                printf("create pthread failed\n");
                goto out;
        }
         ret = pthread_create(&thread2, &attr, thread_func1, NULL);
        if (ret) {
                printf("create pthread failed\n");
                goto out;
        }
 
        /* Join the thread and wait until it is done */
     /*ret = pthread_join(thread1, NULL);
       if (ret)
       {
               printf("join pthread failed: %m\n");
             }   
               ret = pthread_join(thread2, NULL);
    if (ret)
             printf("join pthread failed: %m\n");
   */
		while(kbhit() != 'q')
			{
				//
			}
	
		// Release lines and chip
		gpiod_line_release(lineRed);

		gpiod_chip_close(chip);    
		  gpiod_line_release(lineGreen);
		//gpiod_chip_close(chip1);       
            
 
out:
        return ret;
        
}

int rt_init(void)
{
     
        struct sched_param param;
     
        /* Lock memory */
        if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
                printf("mlockall failed: %m\n");
                exit(-2);
        }
 
        /* Initialize pthread attributes (default values) */
        ret = pthread_attr_init(&attr);
        if (ret) printf("init pthread attributes failed\n");
 
        /* Set a specific stack size  */
        ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
        if (ret) printf("pthread setstacksize failed\n");
           
        /* Set scheduler policy and priority of pthread */
        ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        if (ret) printf("pthread setschedpolicy failed\n");
              
        param.sched_priority = 80;
        ret = pthread_attr_setschedparam(&attr, &param);
        if (ret) printf("pthread setschedparam failed\n");
                
        /* Use scheduling parameters of attr */
        ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        if (ret) printf("pthread setinheritsched failed\n");
        
        return ret;
        
}
 
