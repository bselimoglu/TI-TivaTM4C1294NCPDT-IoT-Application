# TI-TivaTM4C1294NCPDT-IoT-Application
The purpose of the project is to write a program that obtains weather information of the city(Eskişehir) from a webpage which is openweathermap.org.
# Background
TI-RTOS is a multithreading and pre-emptive real-time operating system. Multitasking, in an operating system, is allowing a user to
perform more than one computer task at a time. Mutual exclusion is needed to be implemented on a program, since while multitasking, 
multiple tasks may try to access a critical section. This may and probably will cause an error to occur. This problem can be solved 
by using semaphores. A semaphore is a variable or abstract data type that is used for controlling access, by multiple processes, 
to a common resource in a concurrent system such as a multiprogramming operating system.
# Algorithm
Launchpad is connected to the router via ethernet port and, to the PC via debugger port. Programming had been done in Code Composer Studio.
Two tasks had been created. First task, httpTask, waits for semaphore0, from the SWI which is activated by clock HWI, 
gets the current weather information from openweathermap.org. Clock HWI is triggered every 30 seconds. 
After getting the information, semaphore1 is posted to let the task that sends acquired information to the server, tcpSocketTask, 
which waits forever to semaphore1 to be posted. As can be seen, communication between the tasks is done by using semaphores.
