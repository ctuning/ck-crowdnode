/*
# ck-crowdnode
#
# Standalone, thin and portable server to let users participate in experiment crowdsourcing via CK
#
# See LICENSE.txt for licensing details.
# See Copyright.txt for copyright details.
#
# Developer: Daniil Efremov
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(__linux__) || defined(__APPLE__)
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h> /* socket, connect */
    #include <netdb.h> /* struct hostent, gethostbyname */
    #include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
    #include <ctype.h>
#include <sys/stat.h>
#include <ifaddrs.h>
#include <stropts.h>
#include <sys/ioctl.h>

#elif _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>

    struct thread_win_params {
      int sock;
      int newsock;
      char * baseDir;
    };

    void doProcessingWin (struct thread_win_params* twp);

    #pragma comment(lib,"ws2_32.lib") //Winsock Library

#else
#endif

#include "cJSON.h"
#include "base64.h"
#include "urldecoder.h"
#include "net_uuid.h"

static char *const CK_JSON_KEY = "ck_json=";

static char *const JSON_PARAM_NAME_COMMAND = "action";
static char *const JSON_PARAM_PARAMS = "parameters";
static char *const JSCON_PARAM_VALUE_PUSH = "push";
static char *const JSON_PARAM_FILE_NAME = "filename";
static char *const JSON_PARAM_FILE_CONTENT = "file_content_base64";
static char *const JSON_PARAM_EXTRA_PATH = "extra_path";

static char *const JSON_PARAM_SHELL_COMMAND = "cmd";

/**
 * todo move out to config /etc/ck-crowdnode/ck-crowdnode.properties
 */
#define MAX_BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 3333
static const int MAXPENDING = 5;    /* Maximum outstanding connection requests */

static char *const JSON_CONFIG_PARAM_PORT = "port";
static char *const JSON_CONFIG_PARAM_PATH_TO_FILES = "path_to_files";
static char *const JSON_CONFIG_PARAM_SECRET_KEY = "secret_key";

#ifdef _WIN32
static char *const DEFAULT_BASE_DIR = "%LOCALAPPDATA%/ck-crowdnode-files/";
static char *const DEFAULT_CONFIG_DIR = "%LOCALAPPDATA%/.ck-crowdnode/";
static char *const DEFAULT_CONFIG_FILE_PATH = "%LOCALAPPDATA%/.ck-crowdnode/ck-crowdnode-config.json";
static char *const HOME_DIR_TEMPLATE = "%LOCALAPPDATA%";
static char *const HOME_DIR_ENV_KEY = "LOCALAPPDATA";
#define FILE_SEPARATOR "\\"
#else
static char *const DEFAULT_BASE_DIR = "$HOME/ck-crowdnode-files/";
static char *const DEFAULT_CONFIG_DIR = "$HOME/.ck-crowdnode/";
static char *const DEFAULT_CONFIG_FILE_PATH = "$HOME/.ck-crowdnode/ck-crowdnode-config.json";
static char *const HOME_DIR_TEMPLATE = "$HOME";
static char *const HOME_DIR_ENV_KEY = "HOME";
#define FILE_SEPARATOR "/"

int WSAGetLastError() {
	return 0;
}
#endif

/**
 * Input: command in CK JSON format TDB
 * Output: Execution result in CK JSON format
 *
 * Examples:
 * push command
 *   input JSON:
 *     {"command":"push", "parameters": {"filename":"file1", "data":"<base64 encoded binary file data >"} }
 *
 *   output result JSON:
 *     {"state":"finished", "compileUUID":"567567567567567"}
 *
 * run command
 *   input JSON:
 *     {"command":"run", "parameters":{"compileUUID":"567567567567567"} }
 *
 *   output result JSON:
 *     {"state":"in progress", "runUUID":"12312312323213"}
 *     {"state":"finished ok"}
 *     {"state":"finished error", "errorMessage":"File not found"}
 *
 * state command
 *   input JSON:
 *     {"command":"state", "parameters":{"runUUID":"12312312323213"} }
 *
 *   output result JSON:
 *     {"state":"in progress"}
 *     {"state":"finished ok"}
 *     {"state":"finished error", "errorMessage":"File not found"}
 *
 * pull command
 *   input JSON:
 *     {"command":"pull", "parameters":{"runUUID":"12312312323213"}}
 *
 *   output result JSON:
 *     {"state":"finished", "parameters": {"filename":"file1", "data":"<base64 encoded binary file data >"} }
 *
 *
 * todo list:
 * 2) Check/Implement concurrent execution - looks like thread fors well at linus and windows as well
 * 3) Implement "shell' commnad
 */

void doProcessing(int sock, char *baseDir);

int sockSend(int sock, const void* buf, size_t len) {
#ifdef _WIN32
    return send(sock, buf, len, 0);
#else
    return write(sock, buf, len);
#endif
}

int sockSendAll(int sock, const void* buf, size_t len) {
    const char* p = buf;
    while (0 < len) {
        int n = sockSend(sock, p, len);
        if (0 >= n) {
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

int sendHttpResponse(int sock, int httpStatus, char* payload, int size) {
    // send HTTP headers
    char buf[300];
    int n = sprintf(buf, "HTTP/1.1 %d OK\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %d\r\n\r\n", httpStatus, size);
    if (0 >= n) {
        perror("sprintf failed");
        return -1;
    }
    if (0 > sockSendAll(sock, buf, n)) {
        perror("Failed to send HTTP response headers");
        return -1;
    }

    // send payload
    if (0 > sockSendAll(sock, payload, size)) {
        perror("Failed to send HTTP response body");
        return -1;
    }

    return 0;
}

void sendErrorMessage(int sock, char * errorMessage, const char *errorCode) {
	perror(errorMessage);

	cJSON *resultJSON = cJSON_CreateObject();
    if (!resultJSON) {
        perror("[ERROR]: resultJSON cannot be created");
        return;
    }

    cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString(errorCode));
	cJSON_AddItemToObject(resultJSON, "error", cJSON_CreateString(errorMessage));
	char *resultJSONtext = cJSON_PrintUnformatted(resultJSON);
    if (!resultJSONtext) {
        perror("[ERROR]: resultJSONtext cannot be created");
        return;
    }
    int n = sendHttpResponse(sock, 200, resultJSONtext, strlen(resultJSONtext));
    if (n < 0) {
		perror("ERROR writing to socket");
		return ;
	}
    free(resultJSONtext);
    cJSON_Delete(resultJSON);
}

char* concat(const char *str1, const char *str2) {
    size_t totalSize = strlen(str1) + strlen(str2) + sizeof(char);
    char *message = malloc(totalSize);
    memset(message, 0, totalSize);

    if(!message){
        printf("[ERROR]: Memory not allocated for concat\n");
        exit(-1);
    }

    strcat(message, str1);
    strcat(message + strlen(str1), str2);
    return message;
}

void dieWithError(char *error) {
    printf("Connection error: %s %i", error, WSAGetLastError());
    exit(1);
}

typedef struct {
    int	port;
    char *pathToFiles;
    char *secretKey;

} CKCrowdnodeServerConfig;

CKCrowdnodeServerConfig *ckCrowdnodeServerConfig;
char *serverSecretKey;


static char *const JSON_PARAM_NAME_SECRETKEY = "secretkey";
static char *const ERROR_MESSAGE_SECRET_KEY_MISSMATCH = "secret keys do not match";
static char *const ERROR_CODE_SECRET_KEY_MISMATCH = "3";
static char *const ERROR_CODE = "1";

static const int DEFAULT_DIR_MODE = 0700;

char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

char *getEnvValue(char *param, char** envp ) {
    char * value;
    while (*envp) {
        if (strstr(*envp, param) != NULL) {
            value = malloc(strlen(*envp) + 1);
            if (!value) {
                perror("[ERROR]: Memory not allocated for getEnvValue");
                return NULL;
            }
            strcpy(value, *envp);
            char *rep = concat(param, "=");
            char *string = str_replace(value, rep, "");
            return string;
        }
        ++envp;
    }
    return NULL;
}


char* getAbsolutePath(char *pathToFiles, char** envp) {
    size_t size = strlen(pathToFiles) + sizeof(char);
    char * absolutePath = malloc(size);
    memset(absolutePath, 0, size);
    strcpy(absolutePath, pathToFiles);
    if (strstr(absolutePath, HOME_DIR_TEMPLATE) != NULL) {
        return str_replace(absolutePath, HOME_DIR_TEMPLATE, getEnvValue(HOME_DIR_ENV_KEY, envp));
    }
    return absolutePath;
}

int loadConfigFromFile(CKCrowdnodeServerConfig *ckCrowdnodeServerConfig, char** envp) {
    char *filePath = getAbsolutePath(DEFAULT_CONFIG_FILE_PATH, envp);

    FILE *file=fopen(filePath, "rb");
    if (!file) {
        printf("[WARN]: File not found at path: %s\n", filePath);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *fileContent = malloc(fsize + 1);
    memset(fileContent,0, fsize + 1);
    fread(fileContent, fsize, 1, file);
    fclose(file);

    cJSON *configSON = cJSON_Parse(fileContent);
    if (!configSON) {
        printf("[ERROR]: Invalid JSON format for configuration file %s\n", filePath);
        return 0;
    }

    cJSON *portJSON= cJSON_GetObjectItem(configSON, JSON_CONFIG_PARAM_PORT);
    if (!portJSON) {
        printf("[ERROR]: Invalid JSON format for provided message, attribute %s not found\n", JSON_CONFIG_PARAM_PORT);
        if (configSON != NULL) {
            cJSON_Delete(configSON);
        }
        return 0;
    }
    int port = portJSON->valueint;
    ckCrowdnodeServerConfig->port =port;

    cJSON *pathSON = cJSON_GetObjectItem(configSON, JSON_CONFIG_PARAM_PATH_TO_FILES);
    if (!pathSON) {
        printf("[ERROR]: Invalid JSON format for provided message, attribute %s not found\n", JSON_CONFIG_PARAM_PATH_TO_FILES);
        if (configSON != NULL) {
            cJSON_Delete(configSON);
        }
        return 0;
    }
    char *pathToFiles = getAbsolutePath(pathSON->valuestring, envp);
    ckCrowdnodeServerConfig->pathToFiles = concat(pathToFiles, FILE_SEPARATOR);

    char * secretKey;
    cJSON *secretKeyJSON = cJSON_GetObjectItem(configSON, JSON_CONFIG_PARAM_SECRET_KEY);
    if (!secretKeyJSON) {
        printf("[ERROR]: Invalid JSON format for provided message, attribute %s not found\n", JSON_CONFIG_PARAM_SECRET_KEY);
        if (configSON != NULL) {
            cJSON_Delete(configSON);
        }
        return 0;
    } else {
        secretKey = secretKeyJSON->valuestring;
    }
    size_t size = strlen(secretKey) + sizeof(char);
    ckCrowdnodeServerConfig->secretKey = malloc(size);
    memset(ckCrowdnodeServerConfig->secretKey, 0, size);
    strcpy(ckCrowdnodeServerConfig->secretKey, secretKey);
    cJSON_Delete(configSON);
    return 1;
}

int createCKFilesDirectoryIfDoesnotExist(char * dirPath) {
    int createDirState = 0;
    printf("[INFO]: Check CK crowdnode server files directory: %s\n", dirPath);
    createDirState = mkdir(dirPath, DEFAULT_DIR_MODE);
    if (createDirState<0) {
        perror("[WARN]: Directory was not created");
    } else {
        printf("[INFO]: CK crowdnode server files directory created: %s\n", dirPath);
    }
    return createDirState;
}

void loadDefaultConfig(CKCrowdnodeServerConfig *ckCrowdnodeServerConfig, char** envp) {
    ckCrowdnodeServerConfig->port = DEFAULT_SERVER_PORT;
    ckCrowdnodeServerConfig->pathToFiles = getAbsolutePath(DEFAULT_BASE_DIR, envp);
    char generatedSecretKey[38];
    get_uuid_string(generatedSecretKey, sizeof(generatedSecretKey));
    size_t generatedSecretKeySize = strlen(generatedSecretKey) + sizeof(char);
    ckCrowdnodeServerConfig->secretKey = malloc(generatedSecretKeySize);
    memset(ckCrowdnodeServerConfig->secretKey, 0, generatedSecretKeySize);
    strcpy(ckCrowdnodeServerConfig->secretKey, generatedSecretKey);

    char *configDir = getAbsolutePath(DEFAULT_CONFIG_DIR, envp);
    int createDirState = 0;
    createDirState = mkdir(configDir, DEFAULT_DIR_MODE);
    if (createDirState<0) {
        perror("[WARN]: Configuration directory was not created");
    }

    char *configFilePath = getAbsolutePath(DEFAULT_CONFIG_FILE_PATH, envp);
    FILE *file = fopen(configFilePath, "wb");
    if (!file) {
        perror("[ERROR]: Could not created default configuration file\n");
        exit(1);
    }

    printf("[DEBUG]: Open default configuration file to write %s\n", configFilePath);

    cJSON *defaultConfigJSON = cJSON_CreateObject();
    if (!defaultConfigJSON) {
        perror("[ERROR]: Memory not allocated for defaultConfigJSON\n");
        exit(1);
    }

    char *defaultCrowdnodeServerConfig = malloc(generatedSecretKeySize);
    memset(defaultCrowdnodeServerConfig, 0, generatedSecretKeySize);
    strcpy(defaultCrowdnodeServerConfig, generatedSecretKey);
    cJSON_AddNumberToObject(defaultConfigJSON, JSON_CONFIG_PARAM_PORT, DEFAULT_SERVER_PORT);
    cJSON_AddItemToObject(defaultConfigJSON, JSON_CONFIG_PARAM_PATH_TO_FILES, cJSON_CreateString(getAbsolutePath(DEFAULT_BASE_DIR, envp)));
    cJSON_AddItemToObject(defaultConfigJSON, JSON_CONFIG_PARAM_SECRET_KEY, cJSON_CreateString(defaultCrowdnodeServerConfig));
    char *file_content = cJSON_PrintUnformatted(defaultConfigJSON);
    printf("[INFO]: Default configuration JSON created: %s\n", file_content);

    int results = fwrite(file_content, 1, strlen(file_content), file);
    if (results == EOF) {
        perror("[ERROR]: Failed to write  default configuration file");
        exit(1);

    }
    fclose(file);
    free(file_content);
    cJSON_Delete(defaultConfigJSON);
}

char *getLocalIPv4Adress() {
    char * ip= malloc(NI_MAXHOST + 1);
    if (!ip) {
        perror("[ERROR]: Could not allocate memory for ip address string");
    }
#ifdef _WIN32
    WSADATA wsaData;
    char name[255];
    PHOSTENT hostinfo;
    if ( WSAStartup( MAKEWORD( 2, 0 ), &wsaData ) == 0 ) {

        if( gethostname ( name, sizeof(name)) == 0) {
              if((hostinfo = gethostbyname(name)) != NULL) {
                    ip = inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list);
              }
        }

        WSACleanup( );
    }
#else
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if((strcmp(ifa->ifa_name,"wlan0")==0)&&(ifa->ifa_addr->sa_family==AF_INET)) {
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            printf("\tInterface : <%s>\n",ifa->ifa_name );
            printf("\t  Address : <%s>\n", ip);
        }
    }
    freeifaddrs(ifaddr);
#endif
    return ip;
}


int main( int argc, char *argv[] , char** envp) {

    printf("[INFO]: CK-crowdnode-server starting ...\n");
    printf("[INFO]: %s env value: %s\n", HOME_DIR_TEMPLATE, getEnvValue(HOME_DIR_ENV_KEY, envp));
    printf("[INFO]: Configuration file absolute path: %s\n", getAbsolutePath(DEFAULT_CONFIG_FILE_PATH, envp));
    ckCrowdnodeServerConfig = malloc(sizeof(CKCrowdnodeServerConfig));
    if (!ckCrowdnodeServerConfig) {
        perror("[ERROR]: Memory not allocated for ckCrowdnodeServerConfig\n");
        exit(1);
    }

    if (!loadConfigFromFile(ckCrowdnodeServerConfig, envp)) {
        loadDefaultConfig(ckCrowdnodeServerConfig, envp);
        printf("[WARN]: CK-crowdnode-server configuration file problem. Server will be started with default configuration\n");
    } else {
        printf("[INFO]: CK-crowdnode-server configuration file loaded successfully with configuration\n");
    }

    printf("\n");
    printf("[INFO for CK client]: server real IP:       %s\n", getLocalIPv4Adress());
    printf("[INFO for CK client]: server port:          %i\n", ckCrowdnodeServerConfig->port);
    printf("[INFO for CK client]: server path to files: %s\n", ckCrowdnodeServerConfig->pathToFiles);
    printf("[INFO for CK client]: secret key:           %s\n", ckCrowdnodeServerConfig->secretKey);
    printf("\n");

    createCKFilesDirectoryIfDoesnotExist(getAbsolutePath(ckCrowdnodeServerConfig->pathToFiles, envp));

    serverSecretKey = ckCrowdnodeServerConfig->secretKey;
    int sockfd, newsockfd;
	socklen_t clilen;
    int portno = ckCrowdnodeServerConfig->port;
	char *baseDir = malloc(strlen(ckCrowdnodeServerConfig->pathToFiles) * sizeof(char) + 1);
    if (!baseDir) {
        perror("Could not allocate memory for baseDir");
        exit(1);
    }
    strcpy(baseDir, ckCrowdnodeServerConfig->pathToFiles);
	unsigned long win_thread_id;

#ifdef _WIN32
	struct thread_win_params twp;
	struct thread_win_params* ptwp=&twp;
#endif

	struct sockaddr_in serv_addr, cli_addr;

#ifdef _WIN32
    int servSock;                    /* Socket descriptor for server */
    int clntSock;                    /* Socket descriptor for client */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned short echoServPort;     /* Server port */
    unsigned int clntLen;            /* Length of client address data structure */
    WSADATA wsaData;                 /* Structure for WinSock setup communication */

    echoServPort = DEFAULT_SERVER_PORT;

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) /* Load Winsock 2.0 DLL */
    {
        fprintf(stderr, "WSAStartup() failed");
        exit(1);
    }

    /* Create socket for incoming connections */
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        dieWithError("socket() failed");
    }

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Bind to the local address */
    if (bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
        dieWithError("bind() failed");
    }

    /* Mark the socket so it will listen for incoming connections */
    if (listen(servSock, MAXPENDING) < 0) {
        dieWithError("listen() failed");
    }

#else
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		perror("ERROR opening socket");
        printf("WSAGetLastError() %i\n", WSAGetLastError()); //win
		exit(1);
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR on binding");
		exit(1);
	}
	printf("[INFO]: Server started at port  %i\n", portno);

	listen(sockfd,5);
	clilen = sizeof(cli_addr);
#endif


	/**
     * Main server loop
     */
	while (1) {

		/**
         * Create child process
         */
#ifdef _WIN32
        /* Set the size of the in-out parameter */
        clntLen = sizeof(echoClntAddr);

        /* Wait for a client to connect */
        printf("[INFO] CK-crowdnode-server listen commands on port %i\n", portno);
        if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0) {
            dieWithError("accept() failed");
        }

        ptwp->sock=servSock;
		ptwp->newsock=clntSock;
        ptwp->baseDir=baseDir;

		if (!CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)doProcessingWin,
						  (struct thread_win_params*) ptwp, 0, &win_thread_id))
		{
			perror("ERROR on fork");
			exit(1);
		}

/*		closesocket(sockfd); */
#else

        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd < 0) {
			perror("ERROR on accept");
            printf("WSAGetLastError() %i\n", WSAGetLastError()); //win
			exit(1);
		}
		pid_t pid = fork();

        if (pid < 0) {
            perror("ERROR on fork");
            exit(1);
        }

        if (pid == 0) {
            close(sockfd);
            doProcessing(newsockfd, baseDir);
            exit(0);
        } else {
            close(newsockfd);
        }
#endif
	}
}

#ifdef _WIN32
void doProcessingWin (struct thread_win_params* ptwp)
{
	int sockfd=ptwp->sock;
	int newsockfd=ptwp->newsock;
	char *baseDir = ptwp->baseDir;

	// Child process - talk with connected client
	doProcessing (newsockfd, baseDir);

	if (shutdown (newsockfd, 2)!=0)
	{
		perror("Error on fork");
		exit(1);
	}

	closesocket(newsockfd);

	return;
}
#endif

/**
 * Tries to detect message length by the given buffer, which contains the beginning of the message.
 * The buffer passed must be of at least (size+1) length.
 * 
 * Returns -1, if the length is still unknown (in this case the caller must provide a bigger part of the message).
 * 
 * Returns -2, if the length can never be determined, i.e. HTTP headers don't contain 'Content-Length'.
 *
 * If 0 or more is returned, it is the total size of the message (size of the headers + size of the body).
 */
int detectMessageLength(char* buf, int size) {
    buf[size] = 0;
    
    // trying to find where headers end
    char* s = strstr(buf, "\r\n\r\n");
    int header_stop_len = 4;
    if (NULL == s) {
        s = strstr(buf, "\n\n");
        header_stop_len = 2;
    }
    if (NULL == s) {
        return -1;
    }
    const long header_len = (s - buf) + header_stop_len;

    const char* content_len_key = "Content-Length:";
    // trying to find Content-Length
    char* content_len_header = strstr(buf, content_len_key);
    if (NULL == content_len_header || (content_len_header - buf) >= header_len) {
        return -2;
    }

    long l = strtol(content_len_header + strlen(content_len_key), NULL, 10);
    return header_len + l;
}

void sendJson(int sock, cJSON* json) {
    char* txt = cJSON_PrintUnformatted(json);
    if (NULL == txt) {
        perror("Failed to convert JSON to string");
        exit(1);
    }
    int n1 = sendHttpResponse(sock, 200, txt, strlen(txt));
    free(txt);
    if (n1 < 0) {
        perror("ERROR sending JSON to socket");
    }
}

void processPush(int sock, char* baseDir, cJSON* commandJSON) {
    //  push file (to send file to CK Node )
    cJSON *filenameJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_FILE_NAME);
    if (!filenameJSON) {
        printf("[ERROR]: Invalid action JSON format for provided message\n");
        sendErrorMessage(sock, "Invalid action JSON format for message: no filenameJSON found", ERROR_CODE);
        return;
    }
    char *fileName = filenameJSON->valuestring;
    printf("[DEBUG]: File name: %s\n", fileName);

    char *finalBaseDir = concat(baseDir, FILE_SEPARATOR);

    //  Optional param extra_path
    cJSON *extraPathJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_EXTRA_PATH);
    char *extraPath = "";
    if (extraPathJSON) {
        extraPath = extraPathJSON->valuestring;
        printf("[INFO]: Extra path provided: %s\n", extraPath);

        finalBaseDir = concat(finalBaseDir, extraPath);
        finalBaseDir = concat(finalBaseDir, FILE_SEPARATOR);
        createCKFilesDirectoryIfDoesnotExist(finalBaseDir);
    }

    cJSON *fileContentJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_FILE_CONTENT);
    if (!fileContentJSON) {
        printf("[ERROR]: Invalid action JSON format for message: \n");
        sendErrorMessage(sock, "Invalid action JSON format for message: no fileContentJSON found", ERROR_CODE);
        return;
    }
    char *file_content_base64 = fileContentJSON->valuestring;
    printf("[DEBUG]: File content base64 length: %lu\n", (unsigned long) strlen(file_content_base64));

    int targetSize = ((unsigned long) strlen(file_content_base64) + 1) * 4 / 3;
    unsigned char *file_content = malloc(targetSize);

    int bytesDecoded = 0;
    if (strlen(file_content_base64) != 0) {
        bytesDecoded = base64_decode(file_content_base64, file_content, targetSize);
        if (bytesDecoded == 0) {
            sendErrorMessage(sock, "Failed to Base64 decode file", ERROR_CODE);
        }
        file_content[bytesDecoded] = '\0';
        printf("[INFO]: Bytes decoded: %i\n", bytesDecoded);
    } else {
        printf("[WARNING]: file content is empty nothing to decode\n");
    }

    char *filePath = concat(finalBaseDir,fileName);

    FILE *file = fopen(filePath, "wb");
    if (!file) {
        char *message = concat("Could not write file at path: ", filePath);
        printf("[ERROR]: %s\n", message);
        sendErrorMessage(sock, message, ERROR_CODE);
        free(file_content);
        return;
    }

    printf("[DEBUG]: Open file to write %s\n", filePath);
    printf("[DEBUG]: Bytes to write %i\n", bytesDecoded);
    int results = fwrite(file_content, 1, bytesDecoded, file);
    if (results == EOF) {
        sendErrorMessage(sock, "Failed to write file ", ERROR_CODE);
    }
    fclose(file);
    free(file_content);
    printf("[INFO]: File saved to: %s\n", filePath);

    /**
     * return successful response message, example:
     *   {"return":0, "compileUUID": <generated UID>}
     */
    // char compileUUID[38];
    // get_uuid_string(compileUUID, sizeof(compileUUID));

    cJSON *resultJSON = cJSON_CreateObject();
    if (!resultJSON) {
        perror("[ERROR]: Memory not allocated for resultJSON");
        exit(1);
    }
    printf("[INFO]: resultJSON created\n");
    cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
    // cJSON_AddItemToObject(resultJSON, "compileUUID", cJSON_CreateString(compileUUID));
    sendJson(sock, resultJSON);
    cJSON_Delete(resultJSON);
}

void processPull(int sock, char* baseDir, cJSON* commandJSON) {
    //  pull file (to receive file from CK node)
    cJSON *filenameJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_FILE_NAME);
    if (!filenameJSON) {
        printf("[ERROR]: Invalid action JSON format for provided message\n");
        sendErrorMessage(sock, "Invalid action JSON format for message: no filenameJSON found", ERROR_CODE);
        return;
    }

    char *fileName = filenameJSON->valuestring;
    printf("[DEBUG]: File name: %s\n", fileName);

    char *finalBaseDir = concat(baseDir,FILE_SEPARATOR);

    //  Optional param extra_path
    cJSON *extraPathJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_EXTRA_PATH);
    char *extraPath = "";
    if (extraPathJSON) {
        extraPath = extraPathJSON->valuestring;
        printf("[INFO]: Extra path provided: %s\n", extraPath);

        finalBaseDir = concat(finalBaseDir, extraPath);
        finalBaseDir = concat(finalBaseDir, FILE_SEPARATOR);
    }

    char *filePath = concat(finalBaseDir, fileName);
    printf("[DEBUG]: Reading file: %s\n", filePath);
    FILE *file = fopen(filePath, "rb");
    if (!file) {
        char *message = concat("File not found at path:", filePath);
        printf("[ERROR]: %s", message);
        sendErrorMessage(sock, message, ERROR_CODE);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *fileContent = malloc(fsize + 1);
    memset(fileContent, 0, fsize + 1);
    fread(fileContent, fsize, 1, file);
    fclose(file);

    fileContent[fsize] = 0;
    printf("[DEBUG]: File size: %lu\n", fsize);

    unsigned long targetSize = (unsigned long) ((fsize) * 4 / 3 + 5);
    printf("[DEBUG]: Target encoded size: %lu\n", targetSize);
    char *encodedContent = malloc(targetSize);
    if (!encodedContent) {
        perror("[ERROR]: Memory not allocated for encodedContent");
        exit(1);
    }

    if (fsize > 0) {
        base64_encode(fileContent, fsize, encodedContent, targetSize);
    }

    /**
     * return successful response message, example:
     *   {"return":0, "filename": <file name from requies>, "file_content_base64":<base 64 encoded requested file content>}
     */
    cJSON *resultJSON = cJSON_CreateObject();
    if (!resultJSON) {
        perror("[ERROR]: Memory not allocated for resultJSON");
        exit(1);
    }
    cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
    cJSON_AddItemToObject(resultJSON, JSON_PARAM_FILE_NAME, cJSON_CreateString(fileName));
    cJSON_AddItemToObject(resultJSON, JSON_PARAM_FILE_CONTENT, cJSON_CreateString(encodedContent));
    sendJson(sock, resultJSON);
    cJSON_Delete(resultJSON);
    free(encodedContent);
}

void processShell(int sock, cJSON* commandJSON, char *baseDir) {
    //  shell (to execute a binary at CK node)
    // todo implement:
    // 1) find local file by provided name - send JSON error if not found
    // 2) generate run ID
    // 3) fork new process for async execute
    // 3) return run UUID as JSON sync with run UID and send to client
    // 4) in async process convert to JSON with ru UID and send to client

    cJSON *shellCommandJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_SHELL_COMMAND);
    if (!shellCommandJSON) {
        printf("[ERROR]: Invalid action JSON format for provided message\n");
        sendErrorMessage(sock, "Invalid action JSON format for message: no filenameJSON found", ERROR_CODE);
        return;
    }

    char *shellCommand = shellCommandJSON->valuestring;

    if (!shellCommand) {
        printf("[ERROR]: Invalid action JSON format for provided message\n");
        sendErrorMessage(sock, "Invalid action JSON format for message: no filenameJSON found", ERROR_CODE);
        return;
    }

    chdir(baseDir);

    int systemReturnCode = 0;

    char path[MAX_BUFFER_SIZE + 1];
    unsigned char *stdoutText = malloc(MAX_BUFFER_SIZE + 1);
    if (stdoutText == NULL) {
        perror("[ERROR]: Memory not allocated for stdoutText first time");
        exit(1);
    }
    memset(stdoutText, 0, MAX_BUFFER_SIZE + 1);

    char tmpFilename[38];
    get_uuid_string(tmpFilename, sizeof(tmpFilename));

    char *tmpStdErrFilePath = concat(baseDir, FILE_SEPARATOR);
    tmpStdErrFilePath = concat(tmpStdErrFilePath,tmpFilename);
    char *redirectString = concat(" 2>", tmpStdErrFilePath);
    char *shellCommandWithStdErr = concat(shellCommand, redirectString);
    printf("[INFO]: Run command: %s\n", shellCommandWithStdErr);
    /* Open the command for reading. */
    FILE *fp;
#ifdef _WIN32
    fp = _popen(shellCommandWithStdErr, "r");
#else
    fp = popen(shellCommandWithStdErr, "r");
#endif
    if (fp == NULL) {
        printf("[ERROR]: Failed to run command: %s\n", shellCommand);
        exit(1);
    }

    int totalRead = 0;
    while (fgets(path, sizeof(path) - 1, fp) != NULL) {
        unsigned long pathSize = (unsigned long)(strlen(path));
        stdoutText = realloc(stdoutText, totalRead + pathSize + 1);
        if (stdoutText == NULL) {
            perror("[ERROR]: Memory not allocated stdout");
            exit(1);
        }
        memcpy(stdoutText + totalRead, path, pathSize);
        totalRead = totalRead + pathSize;
    }

#ifdef _WIN32
    systemReturnCode = _pclose(fp);
#else
    systemReturnCode = pclose(fp);
#endif

    stdoutText[totalRead] ='\0';
    printf("[INFO]: total stdout length: %i\n", totalRead);
    printf("[DEBUG]: stdout: %s\n", stdoutText);

    char *encodedContent = "";
    if (totalRead > 0) {
        unsigned long targetSize = (unsigned long) ((totalRead) * 4 / 3 + 5);
        printf("[DEBUG]: Target stdout encoded size: %lu\n", targetSize);
        encodedContent = malloc(targetSize);
        if (!encodedContent) {
            perror("[ERROR]: Memory not allocated for encoded stdout Content");
            exit(1);
        }
        memset(encodedContent, 0, targetSize);
        base64_encode(stdoutText, totalRead, encodedContent, targetSize);
    }

    free(stdoutText);

    cJSON *resultJSON = cJSON_CreateObject();
    cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));

    cJSON_AddNumberToObject(resultJSON, "return_code", systemReturnCode);

    cJSON_AddItemToObject(resultJSON, "stdout", cJSON_CreateString(encodedContent));

    long fsize = 0;
    FILE *stdErrFile = fopen(tmpStdErrFilePath, "rb");
    if (!stdErrFile) {
        sendErrorMessage(sock, "can't find stderr tmp file", ERROR_CODE);
        return;
    }

    unsigned char *stdErr = "";
    if (stdErrFile) {
        fseek(stdErrFile, 0, SEEK_END);
        fsize = ftell(stdErrFile);
        fseek(stdErrFile, 0, SEEK_SET);

        stdErr = malloc(fsize + 1);
        memset(stdErr, 0, fsize + 1);
        fread(stdErr, fsize, 1, stdErrFile);
        fclose(stdErrFile);
    }
    printf("[DEBUG]: stderr file size: %lu\n", fsize);

    char *encodedStdErr = "";
    if (fsize > 0) {
        unsigned long targetSize = (unsigned long) ((fsize) * 4 / 3 + 5);
        printf("[DEBUG]: Target stderr encoded size: %lu\n", targetSize);
        encodedStdErr = malloc(targetSize);
        if (!encodedStdErr) {
            perror("[ERROR]: Memory not allocated for encoded stderr content\n");
            exit(1);
        }
        memset(encodedStdErr, 0, targetSize);
        base64_encode(stdErr, fsize, encodedStdErr, targetSize);
        free(stdErr);
    }

    cJSON_AddItemToObject(resultJSON, "stderr", cJSON_CreateString(encodedStdErr));

    sendJson(sock, resultJSON);
    cJSON_Delete(resultJSON);

    int ret = remove(tmpStdErrFilePath);
    if(ret == 0) {
        printf("[INFO]: tmp stderr file %s deleted successfully\n", tmpStdErrFilePath);
    } else {
        perror("[ERROR]: unable to delete the tmp stderr file\n");
    }
}

void doProcessing(int sock, char *baseDir) {
    char *client_message = malloc(MAX_BUFFER_SIZE + 1);
    if (client_message == NULL) {
        perror("[ERROR]: Memory not allocated for client_message first time");
        exit(1);
    }

    char *buffer = malloc(MAX_BUFFER_SIZE + 1);
    if (buffer == NULL) {
        perror("[ERROR]: Memory not allocated buffer");
        exit(1);
    }

    memset(buffer, 0, MAX_BUFFER_SIZE);
    int buffer_read = 0;
    int total_read = 0;
    int message_len = -1;

    //buffered read from socket
    int i = 0;
    while(1) {
        buffer_read = recv(sock, buffer, MAX_BUFFER_SIZE, 0);
        if (buffer_read > 0) {
            client_message = realloc(client_message, total_read + buffer_read + 1);
            if (client_message == NULL) {
                perror("Error ! Memory not allocated client_message");
                exit(1);
            }
            buffer[buffer_read] = '\0';
            memcpy(client_message + total_read, buffer, buffer_read);
            total_read = total_read + buffer_read;
            printf("Next %i part of buffer\n", i);
            i++;
            if (-1 == message_len) {
                message_len = detectMessageLength(buffer, total_read);
            }
        } else if (buffer_read < 0) {
            perror("[ERROR]: reading from socket");
            printf("WSAGetLastError() %i\n", WSAGetLastError()); //win
            exit(1);
        }
        if (buffer_read == 0 || total_read >= message_len || -2 == message_len) {
            /* message received successfully */
            break;
        }
    }
    if (buffer == NULL) {
        perror("Error ! Try to free not allocated memory buffer");
        exit(1);
    }
    free(buffer);
    client_message[total_read] = '\0';
    printf("[DEBUG]: Post request length: %lu\n", (unsigned long) strlen(client_message));

	char *decodedJSON;
	char *encodedJSONPostData = strstr(client_message, CK_JSON_KEY);
	if (encodedJSONPostData != NULL) {
		char *encodedJSON = encodedJSONPostData + strlen(CK_JSON_KEY);
		decodedJSON = url_decode(encodedJSON, total_read - (encodedJSON - client_message));
        free(client_message);
	} else {
		decodedJSON = client_message;
	}

	cJSON *commandJSON = cJSON_Parse(decodedJSON);
    free(decodedJSON);
	if (!commandJSON) {
		sendErrorMessage(sock, "Invalid action JSON format for message", ERROR_CODE);
		return;
	}


    cJSON *secretkeyJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_NAME_SECRETKEY);
    if (!secretkeyJSON) {
        if (commandJSON != NULL) {
            cJSON_Delete(commandJSON);
        }
        sendErrorMessage(sock, ERROR_MESSAGE_SECRET_KEY_MISSMATCH, ERROR_CODE_SECRET_KEY_MISMATCH);
        return;
    }
    char *clientSecretKey = secretkeyJSON->valuestring;
    printf("[DEBUG]: Got secretkey: %s from client\n", clientSecretKey);
    if (!serverSecretKey || strncmp(clientSecretKey, serverSecretKey, strlen(serverSecretKey)) == 0 ) {
        cJSON *actionJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_NAME_COMMAND);
        if (!actionJSON) {
            printf("[ERROR]: Invalid action JSON format for message: \n");
            if (commandJSON != NULL) {
                cJSON_Delete(commandJSON);
            }
            sendErrorMessage(sock, "Invalid action JSON format for message: no action found", ERROR_CODE);
            return;
        }
        char *action = actionJSON->valuestring;

        printf("[INFO]: Get action: %s\n", action);
        char *resultJSONtext;
        if (strncmp(action, JSCON_PARAM_VALUE_PUSH, 4) == 0) {
            processPush(sock, baseDir, commandJSON);
        } else if (strncmp(action, "pull", 4) == 0) {
            processPull(sock, baseDir, commandJSON);
        } else if (strncmp(action, "shell", 4) == 0) {
            processShell(sock, commandJSON, baseDir);
        } else if (strncmp(action, "state", 4) == 0) {
            printf("[DEBUG]: Check run state by runUUID ");
            cJSON *params = cJSON_GetObjectItem(commandJSON, JSON_PARAM_PARAMS);
            char *runUUID = cJSON_GetObjectItem(params, "runUUID")->valuestring;
            printf("[DEBUG]: runUUID: %s\n", runUUID);

            //todo implement get actual runing state by runUUID

            cJSON *resultJSON = cJSON_CreateObject();
            cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
            sendJson(sock, resultJSON);
            cJSON_Delete(resultJSON);
        } else if (strncmp(action, "clear", 4) == 0) {
            printf("[DEBUG]: Clearing tmp files ...");
            // todo implement removing all temporary files saved localy but need check some process could be in running state
            // so need to discus how it should work
            sendHttpResponse(sock, 200, "", 0);
        } else if (strncmp(action, "shutdown", 4) == 0) {
            printf("[DEBUG]: Start shutdown CK node");
            sendHttpResponse(sock, 200, "", 0);
        } else {
            sendErrorMessage(sock, "unknown action", ERROR_CODE);
        }
    } else {
        sendErrorMessage(sock, ERROR_MESSAGE_SECRET_KEY_MISSMATCH, ERROR_CODE_SECRET_KEY_MISMATCH);
    }
	cJSON_Delete(commandJSON);

	printf("[INFO]: Action completed successfuly\n");
}

