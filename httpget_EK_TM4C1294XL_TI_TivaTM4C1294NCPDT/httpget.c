#include <xdc/std.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/GPIO.h>
#include "Board.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Idle.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/net/http/httpcli.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <xdc/cfg/global.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/sysctl.h"

#define HOSTNAME          "api.openweathermap.org"
#define REQUEST_URI       "/data/2.5/weather?q=Eskisehir&units=metric&APPID=86564f18f4ea69b98df9c8df82c1200d"
#define USER_AGENT        "HTTPCli (ARM; TI-RTOS)"
#define SOCKETTEST_IP     "10.10.96.61"
#define TASKSTACKSIZE   4096


char   tempstr[20];
char   total[40];
uint32_t ADCValues[4];
int array[20];
int index=-1;
int averageTemp=0;
int fromnet;
char arr[10]="";
void printError(char *errString, int code)
{
    System_printf("Error! code = %d, desc = %s\n", code, errString);
    BIOS_exit(code);
}

void sendData2Server(char *serverIP, int serverPort, char *data, int size)
{
    int sockfd;
    struct sockaddr_in serverAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1) {
        System_printf("Socket not created");
        BIOS_exit(-1);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));  /* clear serverAddr structure */
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);     /* convert port # to network order */
    inet_pton(AF_INET, serverIP, &(serverAddr.sin_addr));

    int connStat = connect(sockfd, (struct sockaddr *)&serverAddr, /* connecting….*/
                  sizeof(serverAddr));
    if(connStat < 0) {
        System_printf("Error while connecting to server\n");
        if (sockfd > 0)
            close(sockfd);
        BIOS_exit(-1);
    }


    int numSend = send(sockfd, data, size, 0);       /* send data to the server*/
    if(numSend < 0) {
        System_printf("Error while sending data to server\n");
        if (sockfd > 0) close(sockfd);
        BIOS_exit(-1);
    }

    if (sockfd > 0) close(sockfd);
}

Void tcpSocketTask(UArg arg0, UArg arg1)
{
    while(1) {
        // wait for the semaphore that httpTask() will signal
        // when temperature string is retrieved from api.openweathermap.org site
        //
        Semaphore_pend(semaphore1, BIOS_WAIT_FOREVER);

        GPIO_write(Board_LED0, 1); // turn on the LED

        // connect to SocketTest program on the system with given IP/port
        // send hello message whihc has a length of 5.
        //
        //int j;
        char sentfromnet[20];//sentfromnet[0]="-";for(j=1;j<20;j++){sentfromnet[j]=}
        char temp[20];
        /*if(fromnet<0){
            fromnet=fromnet*(-1);
            strcpy(sentfromnet,"-");
            strcpy(total,  "1.from net 2.from ADC : ");
            sprintf(temp,"%d",fromnet);
            strcat(sentfromnet,temp);
            strcat(total,sentfromnet);
            strcat(total,", ");
            strcat(total,arr);
        }
        else
            {*/
            //sprintf(sentfromnet,"%d",fromnet);
            strcpy(total,  "1.from net 2.from ADC : ");
            strcat(total,tempstr);
            strcat(total,", ");
            strcat(total,arr);
            //}



        sendData2Server(SOCKETTEST_IP, 5011, total, strlen(total));
        strcpy(total,  "1.from net 2.from ADC :             ");
        GPIO_write(Board_LED0, 0);  // turn off the LED

    }
}

Void httpTask(UArg arg0, UArg arg1)
{
        while(1) {
            Semaphore_pend(semaphore0, BIOS_WAIT_FOREVER);

        bool moreFlag = false;
        char data[64], *s1, *s2;
        int ret, temp_received=0, len;
        struct sockaddr_in addr;

        HTTPCli_Struct cli;
        HTTPCli_Field fields[3] = {
            { HTTPStd_FIELD_NAME_HOST, HOSTNAME },
            { HTTPStd_FIELD_NAME_USER_AGENT, USER_AGENT },
            { NULL, NULL }
        };


        System_printf("Sending a HTTP GET request to '%s'\n", HOSTNAME);
        System_flush();

        HTTPCli_construct(&cli);

        HTTPCli_setRequestFields(&cli, fields);

        ret = HTTPCli_initSockAddr((struct sockaddr *)&addr, HOSTNAME, 0);
        if (ret < 0) {
            printError("httpTask: address resolution failed", ret);
        }

        ret = HTTPCli_connect(&cli, (struct sockaddr *)&addr, 0, NULL);
        if (ret < 0) {
            printError("httpTask: connect failed", ret);
        }

        ret = HTTPCli_sendRequest(&cli, HTTPStd_GET, REQUEST_URI, false);
        if (ret < 0) {
            printError("httpTask: send failed", ret);
        }

        ret = HTTPCli_getResponseStatus(&cli);
        if (ret != HTTPStd_OK) {
            printError("httpTask: cannot get status", ret);
        }

        System_printf("HTTP Response Status Code: %d\n", ret);

        ret = HTTPCli_getResponseField(&cli, data, sizeof(data), &moreFlag);
        if (ret != HTTPCli_FIELD_ID_END) {
            printError("httpTask: response field processing failed", ret);
        }

        len = 0;
        do {
            ret = HTTPCli_readResponseBody(&cli, data, sizeof(data), &moreFlag);
            if (ret < 0) {
                printError("httpTask: response body processing failed", ret);
            }
            else {
                // string is read correctly
                // find "temp:" string
                //
                s1=strstr(data, "temp");
                if(s1) {
                    if(temp_received) continue;     // temperature is retrieved before, continue
                    // is s1 is not null i.e. "temp" string is found
                    // search for comma
                    s2=strstr(s1, ",");
                    if(s2) {
                        *s2=0;                      // put end of string
                        strcpy(tempstr, s1+6);      // copy the string
                        temp_received = 1;
                    }
                }
            }

            len += ret;     // update the total string length received so far
        } while (moreFlag);
        //float a = atof(tempstr);
        //uint32_t fromnet = a-273.15;
        System_printf("Recieved %d bytes of payload\n", len);
        System_printf("Temperaturefromnet %s\n", tempstr);
        System_flush();                                         // write logs to console

        HTTPCli_disconnect(&cli);                               // disconnect from openweathermap

        Semaphore_post(semaphore1);                             // activate socketTask

        //Task_sleep(5000);                                       // sleep 5 seconds

    HTTPCli_destruct(&cli);
    }
}
Void WeatherTimer (UArg arg0, UArg arg1){

    Semaphore_post(semaphore0);
}
Void swi(UArg arg0, UArg arg1)
{
    double sum=0;
    int i;
   // int length = sizeof(array)/sizeof(int);
    if (index==19)
    {
        for(i=0;i<20;i++)
        {
            sum+=array[i];
            array[i]=0;
        }
     index=-1;
     averageTemp=(int)(sum/20);

     sprintf(arr,"%d",averageTemp);
     System_printf("TemperatureADC %s\n", arr);
     System_flush();
    }


}

Void HwiTimer(UArg arg0, UArg arg1)     // Uarglara bak
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlDelay(3);
    //
    // Enable sample sequence 3 with a processor signal trigger. Sequence 3
    // will do a single sample when the processor sends a singal to start the
    // conversion. Each ADC module has 4 programmable sequences, sequence 0
    // to sequence 3. This example is arbitrarily using sequence 3.
    //
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    //
    // Configure step 0 on sequence 3. Sample the temperature sensor
    // (ADC_CTL_TS) and configure the interrupt flag (ADC_CTL_IE) to be set
    // when the sample is done. Tell the ADC logic that this is the last
    // conversion on sequence 3 (ADC_CTL_END). Sequence 3 has only one
    // programmable step. Sequence 1 and 2 have 4 steps, and sequence 0 has
    // 8 programmable steps. Since we are only doing a single conversion using
    // sequence 3 we will only configure step 0. For more information on the
    // ADC sequences and steps, reference the datasheet.
    //
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_TS | ADC_CTL_IE | ADC_CTL_END);
    //
    // Since sample sequence 3 is now configured, it must be enabled.
    //
    ADCSequenceEnable(ADC0_BASE, 3);
    //
    // Clear the interrupt status flag. This is done to make sure the
    // interrupt flag is cleared before we sample.
    //
    ADCIntClear(ADC0_BASE, 3);
    ADCProcessorTrigger(ADC0_BASE, 3);
    while(!ADCIntStatus(ADC0_BASE, 3, false)) { }
    //
    // Clear the ADC interrupt flag.
    //
    ADCIntClear(ADC0_BASE, 3);
    //
    // Read ADC Value.
    //
    ADCSequenceDataGet(ADC0_BASE, 3, ADCValues);
    //
    // Convert to Centigrade
    //
    int TempValueC = (int)(147.5-((75.0*3.3 *(float)ADCValues[0])) / 4096.0);
    array[++index]=TempValueC;

    Swi_post(swi0);
}
void netIPAddrHook(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{
       static Task_Handle taskHandle1, taskHandle2;
       Task_Params taskParams;
       Error_Block eb;

       // Create a HTTP task when the IP address is added
       if (fAdd && !taskHandle1 && !taskHandle2) {

       Error_init(&eb);
       Task_Params_init(&taskParams);
       taskParams.stackSize = 4096;
       taskParams.priority = 1;
       taskHandle1 = Task_create((Task_FuncPtr)httpTask, &taskParams, &eb);
//=========================================================================================================
       Task_Params_init(&taskParams);
       taskParams.stackSize = 4096;
       taskParams.priority = 1;
       taskHandle2 = Task_create((Task_FuncPtr)tcpSocketTask, &taskParams, &eb);

       if (taskHandle1 == NULL || taskHandle2 == NULL) {
           printError("netIPAddrHook: Failed to create HTTP and Socket Tasks\n", -1);
       }
       Timer_start(timer0);
       Timer_start(timer1);
   }
}



/*
 *  ======== main ========
 */
int main(void)
{

    Board_initGeneral();
    Board_initGPIO();
    Board_initEMAC();
    /* Turn on user LED  */
    GPIO_write(Board_LED0, Board_LED_ON);

    /* Start BIOS */
    BIOS_start();

    return (0);
}
