#include <stdio.h>  /* for puts() */
#include <string.h> /* for memset() */
#include <unistd.h> /* for sleep() */
#include <stdlib.h> /* for EXIT_SUCCESS */

#include <signal.h> /* for `struct sigevent` and SIGEV_THREAD */
#include <time.h>   /* for timer_create(), `struct itimerspec`,
                     * timer_t and CLOCK_REALTIME 
                     */
#include <gpiod.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <termios.h>
#include <fcntl.h>
#include <stdint.h>

#define P 900000000
#define pinmux "/sys/devices/platform/ocp/ocp:P9_16_pinmux/state"

		
int rt_init(void);
void gpio_init(void);
void timer_init(void);
char kbhit(void);
void pwm_init(void);

        pthread_attr_t attr;
        int ret;	
		const char *chipname = "gpiochip1";
		struct gpiod_chip *chip;
		struct gpiod_line *lineRed; //Red LED
        char info[] = "900ms elapsed.";
        timer_t timerid;
        struct sigevent sev;
        struct itimerspec trigger;
		char k;
		int i=0;
		pthread_cond_t cv, cv1 ;
		pthread_mutex_t lock;
		int f;
        int fd;
		int valconv ;
		int duty_cycle;
void thread_handler(union sigval sv) 
{
        char *s = sv.sival_ptr;

        /* Will print "100ms elapsed." */
        puts(s);      	
	
		gpiod_line_set_value(lineRed, (i& 1) != 0);	i++;
		
		pthread_mutex_lock(&lock);
		pthread_cond_signal(&cv);
		pthread_mutex_unlock(&lock);
       
       timer_settime(timerid, 0, &trigger, NULL);
}
void *thread_func(void *data) //adc
{
        /* Do RT specific stuff here */
        char val[4];
        while (1)
        {
		pthread_mutex_lock(&lock);
		pthread_cond_wait(&cv, &lock);
		pthread_mutex_unlock(&lock);

		fd = open("/sys/devices/platform/ocp/44e0d000.tscadc/TI-am335x-adc.0.auto/iio:device0/in_voltage3_raw", O_RDONLY);
		if (fd<0) 
		{
			perror("Unable to open gpio->value");
			exit(1);
		}
		read(fd, &val, 4) ;
		close (fd);
		valconv= atoi(val);
		printf("ADC = %d\n",valconv);
		
		pthread_mutex_lock(&lock);
		pthread_cond_signal(&cv1);
		pthread_mutex_unlock(&lock);
	}
        return NULL;
}

void update(int val)
{
	char duty_cycle[10];
	int f = open("/sys/devices/platform/ocp/48302000.epwmss/48302200.pwm/pwm/pwmchip4/pwm-4:0/duty_cycle", O_WRONLY);
       
	sprintf(duty_cycle,"%d",(int)val);
	write(f, duty_cycle, strlen(duty_cycle));
	close(f);

}
void *thread3(void *v)// PWM
 {
	while (1)
	{
		pthread_mutex_lock(&lock);
		pthread_cond_wait(&cv1, &lock);
		pthread_mutex_unlock(&lock);
    	duty_cycle = 1000000*duty_cycle>>12;
		update(duty_cycle);
		printf(" PWM updated\n");
	}
	return NULL;
}

int main(int argc, char **argv)
{
        pthread_t thread1, thread2;
		if(rt_init()) exit(0);
		    
         timer_init();
		 gpio_init();
		 pwm_init();
         
               /* Create a pthread with specified attributes */

		
		        ret = pthread_create(&thread1, &attr, thread_func, NULL);
        if (ret) {
                printf("create pthread 2 failed\n");
                goto out;
        }


        /* Join the thread and wait until it is done */
        ret = pthread_join(thread1, NULL);
        
        if (ret)
                {printf("join pthread 2 failed: %m\n");}
		
		ret = pthread_create(&thread2, &attr, thread3, NULL);
        if (ret) {
                printf("create pthread 3 failed\n");
                goto out;
        }


        /* Join the thread and wait until it is done */
        ret = pthread_join(thread2, NULL);
        
        if (ret)
                {printf("join pthread 3 failed: %m\n");}
						
		while(k != 's')
		{
			k= kbhit();
		}
		// Release lines and chip
		gpiod_line_release(lineRed);
		gpiod_chip_close(chip);
        /* Delete (destroy) the timer */
        timer_delete(timerid);
out:
        return ret;

}

void timer_init(void)
{
        /* Set all `sev` and `trigger` memory to 0 */
        memset(&sev, 0, sizeof(struct sigevent));
        memset(&trigger, 0, sizeof(struct itimerspec));

        /* 
         * Set the notification method as SIGEV_THREAD:
         *
         * Upon timer expiration, `sigev_notify_function` (thread_handler()),
         * will be invoked as if it were the start function of a new thread.
         *
         */
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = &thread_handler;
        sev.sigev_value.sival_ptr = &info;

        /* Create the timer. In this example, CLOCK_REALTIME is used as the
         * clock, meaning that we're using a system-wide real-time clock for
         * this timer.
         */
        timer_create(CLOCK_REALTIME, &sev, &timerid);

        /* Timer expiration will occur withing 5 seconds after being armed
         * by timer_settime().
         */
        trigger.it_value.tv_sec = 0;
        trigger.it_value.tv_nsec = P;  // 100ms

        /* Arm the timer. No flags are set and no old_value will be retrieved.
         */
        timer_settime(timerid, 0, &trigger, NULL);

        /* Wait 10 seconds under the main thread. In 5 seconds (when the
         * timer expires), a message will be printed to the standard output
         * by the newly created notification thread.
         */
}

void gpio_init(void)
{
	//open GPIO chip
	chip=gpiod_chip_open_by_name(chipname);
	
	//open GPIO lines
	lineRed = gpiod_chip_get_line(chip,28);
	
	//Open LED lines for outpus
	gpiod_line_request_output(lineRed , "tim", 0);
	
	//open switch line for input
	//gpiod_line_request_input(lineButton,"exemple1");
	
	//Blink LEDs in a binary pattern
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

void pwm_init(void)
{

		fd = open("/sys/devices/platform/ocp/48302000.epwmss/48302200.pwm/pwm/pwmchip4/pwm-4:0/period", O_WRONLY);
		write(fd, "1000000", strlen("1000000"));
		close(fd);
		
		fd = open("/sys/devices/platform/ocp/48302000.epwmss/48302200.pwm/pwm/pwmchip4/pwm-4:0/enable", O_WRONLY);
		write(fd, "1", strlen("1"));
		close(fd);

		fd = open("/sys/devices/platform/ocp/ocp:P9_14_pinmux/state", O_WRONLY);
		write(fd, "pwm", strlen("pwm"));
		close(fd);
}
