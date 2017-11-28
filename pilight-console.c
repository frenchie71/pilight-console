// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////
// pilight-console
// ////////////////////////////////////////////////////////////////////////////
// provides the interface to akeypad and LCD Display
// driven by an arduino
// purpose: arm/disarm (fire) alarm  
// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////


// ////////////////////////////////////////////////////////////////////////////
// includes
// ////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <jansson.h>
#include <time.h>


// ////////////////////////////////////////////////////////////////////////////
// pre-compiler defines and global variables
// ////////////////////////////////////////////////////////////////////////////

#define PIDFILE "/var/run/pilight-console.pid"

#define BUFFER_SIZE 1024
#define PILIGHTPORT 5000

static int arduinoState;
#define ST_OFFLINE 0
#define ST_ONLINE  1


#define LCDWIDTH 20
#define LCDHEIGHT 4
 
static int systemState; // Are we having an alarm?
#define ST_ALARM 1
#define ST_NOALARM 0
#define ST_PINCODE_ENTERED 2

static int pinValid=0;

static int serfd; // the file descriptor for the serial port
static int tcpfd; // the file descriptor for the TCP communication
char *serialString;
char *tcpString;

json_t *globalConfig;
json_t *globalDevices=NULL;
json_t *globalAlarms=NULL;
json_t *pilightStatus=NULL;
json_t *pilightConfig=NULL;
json_t *lastAlarm=NULL;

// ////////////////////////////////////////////////////////////////////////////
// ReadFile
// ////////////////////////////////////////////////////////////////////////////
// returns the content of a file
// ////////////////////////////////////////////////////////////////////////////


char* ReadFile(char *filename)
{
   char *buffer = NULL;
   int string_size, read_size;
   FILE *handler = fopen(filename, "r");

   if (handler)
   {
       // Seek the last byte of the file
       fseek(handler, 0, SEEK_END);
       // Offset from the first to the last byte, or in other words, filesize
       string_size = ftell(handler);
       // go back to the start of the file
       rewind(handler);

       // Allocate a string that can hold it all
       buffer = (char*) malloc(sizeof(char) * (string_size + 1) );

       // Read it all in one operation
       read_size = fread(buffer, sizeof(char), string_size, handler);

       // fread doesnt set it so put a char. 0 in the last position
       // and buffer is now officially a string
       buffer[string_size] = '\0';

       if (string_size != read_size)
       {
           // Something went wrong, throw away the memory and set
           // the buffer to NULL
           free(buffer);
           buffer = NULL;
       }

       // Always remember to close the file.
       fclose(handler);
    }

    return buffer;
}

// ////////////////////////////////////////////////////////////////////////////
// load_json
// ////////////////////////////////////////////////////////////////////////////
/*
 * Parse text into a JSON object. If text is valid JSON, returns a
 * json_t structure, otherwise prints and error and returns null.
 */
// ////////////////////////////////////////////////////////////////////////////


json_t *load_json(const char *text) {
    json_t *root;
    json_error_t error;

    root = json_loads(text, 0, &error);

    if (root) {
        return root;
    } else {
        fprintf(stderr, "json error on line %d: %s\n", error.line, error.text);
        return (json_t *)0;
    }
}


// ////////////////////////////////////////////////////////////////////////////
// readGlobalConfig
// ////////////////////////////////////////////////////////////////////////////
// reads in the config file and stores the config in globalConfig,
// alarms and devices
// ////////////////////////////////////////////////////////////////////////////

void readGlobalConfig()
{
   char *configFile = NULL;
   
   if (configFile = ReadFile("/etc/pilight/pilightconsole.json"))
   {
       globalConfig = load_json(configFile);
       if (globalConfig)
       {
           const char *key;
           json_t *value;
       

           json_object_foreach(globalConfig, key, value) 
           {
               if (strstr(key,"devices"))
                   globalDevices=value;
               if (strstr(key,"alarms"))
                   globalAlarms=value;
           }
            if (globalDevices)
            {
                json_object_foreach(globalDevices, key, value) 
                printf("device monitored: \"%s\"\n", key);
            }

            if (globalAlarms)
            {
                json_object_foreach(globalAlarms, key, value) 
                printf("alarm monitored: \"%s\"\n", key);
            }
        }
        free(configFile);
   }
   
}

// ////////////////////////////////////////////////////////////////////////////
// socket_connect
// ////////////////////////////////////////////////////////////////////////////
// creates a socket on a given port and host and connects to it
// the socket is non-blocking
// ////////////////////////////////////////////////////////////////////////////


int socket_connect(char *host, in_port_t port){
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sock;     

	
	if((hp = gethostbyname(host)) == NULL){
		herror("gethostbyname");
		exit(1);
	}
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if(sock == -1){
		perror("setsockopt");
		exit(1);
	}
	
	if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
		perror("connect");
		exit(1);

	}
	
	// make non-blocking
	
	long save_fd = fcntl( sock, F_GETFL );
	save_fd |= O_NONBLOCK;
	fcntl( sock, F_SETFL, save_fd );
  
	
	return sock;
}


// ////////////////////////////////////////////////////////////////////////////
// set_interface_attribs
// ////////////////////////////////////////////////////////////////////////////
// sets the attributes for communication on the serial port
// from stackoverflow.com (by RicoRico, sawdust)
// ////////////////////////////////////////////////////////////////////////////

int set_interface_attribs(int speed, int mcount)
{
    struct termios tty;

    if (tcgetattr(serfd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);                    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;                              /* */
    tty.c_cflag |= CS8;                                 /* 8-bit characters */
    tty.c_cflag &= ~PARENB;                             /* no parity bit */
    tty.c_cflag &= ~CSTOPB;                             /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;                            /* no hardware flowcontrol */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);             /* No XON/XOFF */
    tty.c_lflag &= ~(ECHO | ECHOE | ICANON | ISIG);     /* */
    tty.c_oflag &= ~OPOST;                              /* */

    /* fetch bytes as they become available */

    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(serfd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }

    tty.c_cc[VMIN] =  mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;                /* half second timer */

    if (tcsetattr(serfd, TCSANOW, &tty) < 0)
    {
        printf("Error tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// ////////////////////////////////////////////////////////////////////////////
// waitabit
// ////////////////////////////////////////////////////////////////////////////
// uses nanosleep to delay less than a second as sleep() only has seconds 
// granularity
// ////////////////////////////////////////////////////////////////////////////

void waitabit()
{
   struct timespec tim, tim2;
   tim.tv_sec = 0;
   tim.tv_nsec = 300000000;

   nanosleep(&tim , &tim2);
   
}


// ////////////////////////////////////////////////////////////////////////////
// sendCommand - send a command to Arduino or pilight
// ////////////////////////////////////////////////////////////////////////////

void sendCommand (int fd, char* theCommand )
{
    int wlen = write(fd, theCommand, strlen(theCommand));
    if (wlen != strlen(theCommand)) {
        printf("Error from write: %d, %d\n", wlen, errno);
    }
    
    printf ("COMMAND %s", theCommand);

    tcdrain(fd);    /* delay for output */
    waitabit();
}




// ////////////////////////////////////////////////////////////////////////////
// handleDevice
// ////////////////////////////////////////////////////////////////////////////
// wertet eine JSON update Nachricht aus.
// ////////////////////////////////////////////////////////////////////////////


/*

Example of a device section in the config file

"devices":	
{
	"feuerscharf"   : {"friendlyname":"Feuermelder",   "value":"state",       "translate":{"on":"scharf", "off":"aus"},"line":1},
	"alarmscharf"   : {"friendlyname":"Alarmanlage",   "value":"state",       "translate":{"on":"scharf", "off":"aus"},"line":2},
	"Aussensensor"  : {"friendlyname":"Aussentemp.",   "value":"temperature",                                          "line":0}
},

Example update messages

{"origin":"update","type":3,"devices":["Aussensensor"],"values":{"timestamp":1510915637,"temperature":6.1,"humidity":74.0,"battery":0.0}}
{"origin":"update","type":1,"devices":["feuerscharf"],"values":{"timestamp":1510915945,"state":"on"}}

*/


void handleDevice (json_t *updateMessage)
{
    // lets have a look at the device node of the incoming message
     
    json_t *myJson = json_object_get(updateMessage,"devices");
    int isAlarm = 0;
    int i;

    // the device node is an array, so we cycle through it

    for(i = 0; i < json_array_size(myJson); i++)
    {
        // lets have a look at one single device
        
        json_t *data = json_array_get(myJson, i);
        if(json_is_string(data))
        {
            // now we look into the configured devices and alarms
            // updatedDevice contains the device name
            
            const char *updatedDevice = json_string_value(data);
            json_t *configNode;
                
            if ( configNode= json_object_get(globalDevices,updatedDevice))   // we have configured this device
                isAlarm=0;
            else
                if ( configNode= json_object_get(globalAlarms,updatedDevice)) // we have it configured as alarm device
                         isAlarm=3;

            if ( configNode)
            {
                // read out the value of the device from the values node of the incoming message
                
                json_t *newValues = json_object_get(updateMessage,"values");
                int lineNumber =0;
                if (!isAlarm) 
                    lineNumber= json_integer_value(json_object_get(configNode,"line"));
                const char *friendlyName = json_string_value(json_object_get(configNode,"friendlyname"));
                
                // find the key we are interested in from the value node of the configured device
                
                const char *valueKey = json_string_value(json_object_get(configNode,"value")); 
                json_t *theValue = json_object_get(newValues,valueKey);
                
                char theStringValue[30];
                json_t *translateValue  = NULL;
                json_t *translatedValue = NULL;
                
                // we might want to translate it, hence see if there is a translate node in the config
                
                if (translateValue = json_object_get(configNode,"translate"))
                {
                   if (translatedValue =  json_object_get(translateValue,json_string_value(theValue)))
                    {
                        theValue = translatedValue;
                    }
                }

                // depending on the type of the field we need to do some formatting (temperature is real, on/off is string etc.)

                switch (json_typeof(theValue)) 
                {
                    case JSON_REAL:
                        sprintf(theStringValue,"%4.1f",json_real_value(theValue));
                        break;
                    case JSON_INTEGER:
                        sprintf(theStringValue,"%d",json_integer_value(theValue));
                        break;
                    case JSON_STRING:
                        sprintf(theStringValue,"%s",json_string_value(theValue));
                        break;
                }        
                //printf ("%d : %s : %s = %s\n", lineNumber, friendlyName, valueKey , theStringValue);
                    
                // for alarm codes we have defined triggervalue and resetvalue, i.e.
                // if the device goes to state triggervalue then we have an alarm.
                // in order to reset it we need to compare to resetvalue

            
                const char *springValue = json_string_value(json_object_get(configNode,"triggervalue"));
                const char *resetValue  = json_string_value(json_object_get(configNode,"resetvalue"));

                char theLine[40];
                bzero(theLine,40);

                // Case 1 : We have received "Alarm on" code

                if  (isAlarm && (strstr(theStringValue,springValue))) 
                {
                    systemState=ST_ALARM;
                    sendCommand(serfd,"CLEAR\n");
                    sprintf (theLine,"MESSAGE %d 0 %d PINCODE ->\n", isAlarm, LCDHEIGHT-1);
                    sendCommand(serfd,theLine);
                    bzero(theLine,40);
                    sprintf (theLine,"MESSAGE %d 0 %d %s !!!", isAlarm, lineNumber, friendlyName);
                    lastAlarm = configNode;
                }

                // Case 2 : We have received "Alarm off" code

                if (isAlarm && (strstr(theStringValue,resetValue)) && (systemState == ST_ALARM)) 
                {
                    systemState=ST_NOALARM;
                    lastAlarm=NULL;
                    sendCommand(serfd,"CLEAR\n");
                    sprintf (theLine,"MESSAGE %d 0 %d PINCODE ->\n", isAlarm, LCDHEIGHT-1);
                    sendCommand(serfd,theLine);
                    bzero(theLine,40);
                    sprintf (theLine,"MESSAGE %d 0 %d %s: %s", isAlarm-1, lineNumber, friendlyName, theStringValue);
                    sendCommand  (tcpfd,"{\"action\": \"request values\" }\r\n");
                }
                    
                // Case 3 : We have received a non-Alarm code

                if   (!isAlarm)
                {
                    json_object_set(configNode, "currentvalue", json_string(theStringValue));
                    if (systemState != ST_ALARM) 
                    {                        
                        sprintf (theLine,"MESSAGE %d 0 %d PINCODE ->\n", isAlarm, LCDHEIGHT-1);
                        sendCommand(serfd,theLine);
                        bzero(theLine,40);
                        sprintf (theLine,"MESSAGE %d 0 %d %s: %s", isAlarm, lineNumber, friendlyName, theStringValue);
                    }
                    
                }

                // We only print if the line is not empty
                
                if (strlen(theLine)>0)
                {

                    // Fill the line with spaces until LCDWIDTH in order to have clean printing on the display
                     
                    int j;
                    for (j=strlen(theLine);j<(LCDWIDTH+strlen("MESSAGE 0 0 1 "));j++)
                        theLine[j]=' ';
                    theLine[LCDWIDTH+strlen("MESSAGE 0 0 1 ")]='\n';
                    theLine[LCDWIDTH+strlen("MESSAGE 0 0 1 ")+1]='\0';
                    sendCommand (serfd, theLine);
                }
                
                // we are done with json objects here
                
                //json_decref(translateValue); // borrowed reference
                //json_decref(newValues); // borrowed reference
                //json_decref(theValue); // borrowed reference
                // json_decref(configNode); // borrowed reference
            }
        }
        // json_decref(data); //borrowed reference
    }
    // json_decref(myJson); //borrowed reference
    
}


// ////////////////////////////////////////////////////////////////////////////
// parseStrings
// ////////////////////////////////////////////////////////////////////////////
// analyzes received string values and tries to interpret them
// ////////////////////////////////////////////////////////////////////////////



void parseStrings()
{

    char* tokenizedString;  // We might receive multiple lines and need to tokenize 


    // //////////////////////////////////////////////
    // werte Eingabe vom Arduino aus
    // //////////////////////////////////////////////
    

    if (strchr(serialString,'\n'))
    {

        tokenizedString = strtok(serialString,"\n");
        
        while (tokenizedString != NULL)
        {

            printf("SERIAL: %s\n",tokenizedString);
        
            char Command[100];
            bzero(Command,100);
            const char *key;
            json_t *value;
            
            // /////////////////////////
            // Arduino OFFLINE
            // /////////////////////////
        
            if (strstr(tokenizedString,"OFFLINE"))   // Arduino says it switched backlight off
            {
                arduinoState = ST_OFFLINE;
                if (pinValid)
                {
                    // /////////////////////////
                    // erase toggle keys
                    // /////////////////////////

                    json_object_foreach(globalDevices, key, value)
                    {                  
                        const char *theKey = json_string_value(json_object_get(value,"key"));
                        int theLine = json_integer_value(json_object_get(value,"line"));
                        if (theKey)
                        {
                            sprintf(Command,"MESSAGE 0 %d %d  \n",LCDWIDTH-1,theLine);
                            sendCommand(serfd,Command);    
                        }
                    }
                    pinValid=0;
                }
            }
            else

            // /////////////////////////
            // Arduino ONLINE
            // /////////////////////////


            if ( strstr(tokenizedString,"ONLINE") ) // Arduino says it switched backlight on
            {
                arduinoState = ST_ONLINE;
            }
            else                                    // something else, e.g. pincode or toggle switch
            {
                int i=strlen(tokenizedString);

                // /////////////////////////
                // pincode
                // /////////////////////////


                const char *pinCode = json_string_value(json_object_get(globalConfig,"pin"));
                if (strcmp(pinCode, tokenizedString) == 0)
                {
                    printf("PINVALID\n");
                    pinValid=1;
                    if (systemState == ST_ALARM)
                    {
       
                        json_object_foreach(globalAlarms, key, value) 
                        if (value == lastAlarm)
                        {
                             sprintf(Command,"{ \"action\": \"control\", \"code\": { \"device\": \"%s\", \"%s\": \"%s\"}}\n",(char *) key,(char *) json_string_value(json_object_get(lastAlarm,"value")), (char *) json_string_value(json_object_get(lastAlarm,"resetvalue")) );
                             sendCommand(tcpfd,Command);
                             
                        }
                    }
                    else

                    // /////////////////////////
                    // show toggle keys
                    // /////////////////////////

                    {
                        // show the keys which can be used to toggle switches
                        
                        json_object_foreach(globalDevices, key, value)
                        {
                        
                            const char *theKey = json_string_value(json_object_get(value,"key"));
                            int theLine = json_integer_value(json_object_get(value,"line"));
                            if (theKey)
                            {
                                sprintf(Command,"MESSAGE 0 %d %d %s\n",LCDWIDTH-1,theLine,theKey);
                                sendCommand(serfd,Command);    
                            }
                        }
                    }
                }
                else
                {
 
                    // /////////////////////////
                    // toggle Values
                    // /////////////////////////
 
                    if (pinValid)
                    json_object_foreach(globalDevices, key, value)
                    {
                        const char *theKey = json_string_value(json_object_get(value,"key"));
                        if ( (theKey) &&  (strcmp(theKey, tokenizedString) == 0) )
                        {
                            json_t *toggleValues = json_object_get(value,"toggles");
                            const char *toggleValue1 = json_string_value(json_array_get(toggleValues,0));
                            const char *toggleValue2 = json_string_value(json_array_get(toggleValues,1));
                            const char *currentValue = json_string_value(json_object_get(value,"currentvalue"));
                            
                            const char *tkey;
                            json_t *tvalue;
                            
                            json_object_foreach(json_object_get(value,"translate"), tkey, tvalue)
                            {
                                if (strcmp(currentValue,json_string_value(tvalue))==0) 
                                    currentValue = tkey;
                            }
                            
                            if (strcmp(currentValue,toggleValue1)==0)
                                tkey=toggleValue2;
                            else
                                tkey=toggleValue1;
                            
                            
                            printf("DEVICE %s TOGGLED from %s to %s \n",key,currentValue, tkey);
                            
                            sprintf(Command,"{ \"action\": \"control\", \"code\": { \"device\": \"%s\", \"%s\": \"%s\"}}\n",(char *) key,(char *) json_string_value(json_object_get(value,"value")), tkey );
                            sendCommand(tcpfd,Command);
                            
                            
                        }
                    }
                    
                }
            }

                


            tokenizedString = strtok(NULL,"\n");
        }
        free (serialString);
        serialString = (char *) malloc(1);
        serialString[0]='\0';
    }


    // //////////////////////////////////////////////
    // werte Eingabe vom pilight Daemon aus
    // //////////////////////////////////////////////

    json_t *SocketCom = NULL;

    
    if (strchr(tcpString,'\n'))
    {
        tokenizedString = strtok(tcpString,"\n");
        while (tokenizedString != NULL)
        {
            printf("SOCKET: %s\n",tokenizedString);
            
            if ( (SocketCom = load_json(tokenizedString))     &&
               ( json_typeof(SocketCom) == JSON_OBJECT) )
            {
                const char *key;
                json_t *value;
                json_t *myJson;

                // ////////////////////////////////////////////////////////////
                // The JSON object we have received from the pilight daemon
                // is the status message which hopefully is "success"
                // ////////////////////////////////////////////////////////////

                if ( (myJson= json_object_get(SocketCom,"status")) && (json_is_string(myJson)) )
                    pilightStatus=myJson;

                // ////////////////////////////////////////////////////////////
                // The JSON object we have received from the pilight daemon
                // contains the initial values
                // ////////////////////////////////////////////////////////////

                if (myJson = json_object_get(SocketCom,"message"))
                {
                    if  ( (myJson = json_object_get(SocketCom,"values")) &&
                          (json_is_array(myJson)) )
                    {
                            pilightConfig=myJson;
                            int i;
                            for(i = 0; i < json_array_size(myJson); i++)
                            {
                                json_t *data = json_array_get(myJson, i);
                                handleDevice(data);
                            }
                    }
                    
                }

                // ////////////////////////////////////////////////////////////
                // The JSON object we have received from the pilight daemon
                // contains an update - we need to check if it is about a device 
                // that we monitor by cross-checking with the global settings
                // ////////////////////////////////////////////////////////////

                if ( (myJson = json_object_get(SocketCom,"origin"))  &&
                     (json_is_string(myJson))                        && 
                     (strstr (json_string_value(myJson),"update")) )
                {
                    if ( (myJson = json_object_get(SocketCom,"devices")) &&
                         (json_is_array(myJson)))
                    {
                        handleDevice(SocketCom);
                    }
                    
                }
                // if (SocketCom) json_decref(SocketCom); //new reference
            }

            // get the next line if any
            tokenizedString=strtok(NULL,"\n");
        }
        
        free (tcpString);
        tcpString = (char *) malloc(1);
        tcpString[0]='\0';
    }
    
}


// ////////////////////////////////////////////////////////////////////////////
// readHandle
// ////////////////////////////////////////////////////////////////////////////
// reads from a given File descriptor, tcp or serial
// modifies global Strings serialString or tcpString
// returns length read
// ////////////////////////////////////////////////////////////////////////////

int readHandle (int fd)
{
    char buffer[BUFFER_SIZE];  // tcp    buffer
    char *sOutput = (fd==serfd) ? serialString : tcpString;
    int oSize = strlen(sOutput);
    bzero(buffer, BUFFER_SIZE);
    int rdlen;
    rdlen = read(fd, buffer, BUFFER_SIZE - 2);
    buffer[rdlen]='\0';
    
    if (rdlen > 0) 
	{
        sOutput = (char *) realloc(sOutput, rdlen + oSize + 1);
        strncat(sOutput,buffer,rdlen);
        if (fd==serfd) 
            serialString =sOutput;
        else 
            tcpString = sOutput;
     }
     return(rdlen);
}

// ////////////////////////////////////////////////////////////////////////////
// main
// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////


int main( int argc, char *argv[] )  
{
	
/*
	if(argc < 3)
	{
		fprintf(stderr, "Usage: %s <SerialPort> <hostname>\n", argv[0]);
		exit(1); 
	}
    
    char *portname = argv[1];
    char *hostname = argv[2];
    
*/

    pid_t process_id = 0;


    readGlobalConfig();
    systemState=ST_NOALARM;
	arduinoState=ST_OFFLINE;

    json_t *pilightConfig = json_object_get(globalConfig,"pilight");
    
    const int portnumber   = json_integer_value(json_object_get(pilightConfig,"port"));
    const char *hostname   = json_string_value(json_object_get(pilightConfig,"server"));
    const char *portname   = json_string_value(json_object_get(globalConfig,"pinano"));
    serialString = (char *) malloc(1);   serialString[0]='\0';
    tcpString = (char *) malloc(1);      tcpString[0]='\0';
		
    printf ("pilight-console\n\nopening %s ...\n",portname);
    serfd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    printf ("nopening %s:%d ...\n",hostname, portnumber);
	tcpfd = socket_connect((char *) hostname, portnumber); 
    printf("connected\n");
	
    if (serfd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        return -1;
    }
	long save_fd = fcntl( serfd, F_GETFL );
	save_fd |= O_NONBLOCK;
	fcntl( serfd, F_SETFL, save_fd );
	printf ("1\n");
    set_interface_attribs(B57600,0);
    printf("OK\nport open, waiting for Arduino...");
    sleep(5); // wait for arduino to reset
    printf("OK\n");

    sendCommand (serfd,"CLEAR\n");
    sendCommand (serfd,"MESSAGE 1 0 0 pilight-console\n");

    const char *status;
    int rdlen;

    do 
    {
        printf("Waiting for registration with Pilight\n");
        sendCommand (tcpfd,"{\"action\": \"identify\", \"options\": { \"core\": 0, \"receiver\": 0, \"config\": 1, \"forward\": 0 }, \"uuid\": \"0000-d0-63-00-101010\", \"media\": \"all\" }\r\n");
        rdlen = readHandle(tcpfd);
        if ((rdlen > 0) && (strlen(tcpString) > 0))
          parseStrings();
        status = json_string_value(pilightStatus);
    } while (!strstr(status,"success"));

    // Create child process
    process_id = fork();
    // Indication of fork() failure
    if (process_id < 0)
    {
    printf("fork failed!\n");
    // Return failure in exit status
    exit(1);
    }
    // PARENT PROCESS. Need to kill it.
    if (process_id > 0)
    {
    printf("process_id of child process %d \n", process_id);
    // return success in exit status
    exit(0);
    }

    int pidFilehandle = open(PIDFILE, O_RDWR|O_CREAT, 0600);
    if (pidFilehandle == -1 )
    {
        /* Couldn't open lock file */
        printf("Could not open PID lock file %s, exiting", PIDFILE);
    }
    else
        /* Try to lock file */
        if (lockf(pidFilehandle,F_TLOCK,0) == -1)
        {
            /* Couldn't get lock on lock file */
            printf( "Could not lock PID lock file %s, exiting", PIDFILE);
        }
        else
        {
        char str[10];
        /* Get and format PID */
        sprintf(str,"%d\n",getpid());
        /* write pid to lockfile */
        write(pidFilehandle, str, strlen(str));
        }
 
         




	sendCommand  (tcpfd,"{\"action\": \"request values\" }\r\n");
    
    // main loop
    
    do 
	{

        rdlen = readHandle(serfd);
        rdlen = readHandle(tcpfd);
        if ((strlen(serialString) > 0) || (strlen(tcpString) > 0))
          parseStrings();
        waitabit();
    } while (1);


}