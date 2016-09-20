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

#elif _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

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

static char *const CK_JSON_KEY = "ck_json=";

static char *const JSON_PARAM_NAME_COMMAND = "action";
static char *const JSON_PARAM_PARAMS = "parameters";
static char *const JSCON_PARAM_VALUE_PUSH = "push";
static char *const JSON_PARAM_FILE_NAME = "filename";
static char *const JSON_PARAM_FILE_CONTENT = "file_content_base64";

/**
 * todo move out to config /etc/ck-crowdnode/ck-crowdnode.properties
 */
static const int MAX_BUFFER_SIZE = 1024;
static const int DEFAULT_SERVER_PORT = 3333;


static char *const DEFAULT_BASE_DIR = "/tmp/";

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
 * 1) Check compile and run on Windows as well
 * 2) Check/Implement concurrent execution
 */

void doProcessing(int sock, char *baseDir);

void sendErrorMessage(int sock, char * errorMessage) {
    perror(errorMessage);

    cJSON *resultJSON = cJSON_CreateObject();
    cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("1"));
    cJSON_AddItemToObject(resultJSON, "error", cJSON_CreateString(errorMessage));
    char * resultJSONtext = cJSON_Print(resultJSON);
    int n = write(sock , resultJSONtext , strlen(resultJSONtext));
    cJSON_Delete(resultJSON);
    free(resultJSONtext);
    if (n < 0) {
        perror("ERROR writing to socket");
        return ;
    }
}

int main( int argc, char *argv[] ) {

    int sockfd, newsockfd;
    socklen_t clilen;
    int portno = DEFAULT_SERVER_PORT;
    char *baseDir = DEFAULT_BASE_DIR;
    unsigned long win_thread_id;

#ifdef _WIN32
    struct thread_win_params twp;
    struct thread_win_params* ptwp=&twp;
#endif

    struct sockaddr_in serv_addr, cli_addr;

    if (argc == 1) {
        portno = DEFAULT_SERVER_PORT;
        printf("[INFO]: Default server port  %i\n", DEFAULT_SERVER_PORT);
    } else if (argc == 2) {
        portno = atol(argv[2]);
    } else if (argc > 2)  {
        printf("USAGE: %s [serverport] \n", argv[0]);
        //todo pass baseDir as param
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    memset((char *) &serv_addr, sizeof(serv_addr), 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    /**
     * Main server loop
     */
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        /**
         * Create child process
         */
#ifdef _WIN32
        ptwp->sock=sockfd;
        ptwp->newsock=newsockfd;

        if (!CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)doProcessingWin,
                         (struct thread_win_params*) ptwp, 0, &win_thread_id))
        {
            perror("ERROR on fork");
            exit(1);
        }

        closesocket(sockfd);
#else
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

char * concat(char *str1, char *str2) {
    char *message = malloc (sizeof(str1) + strlen(str2) + 1);
    strcpy (message, str1);
    strcat (message, str2);
    return message;
}

void doProcessing(int sock, char *baseDir) {
    char *client_message = malloc(MAX_BUFFER_SIZE);
    char *buffer = malloc(MAX_BUFFER_SIZE);
    memset(buffer, MAX_BUFFER_SIZE, 0);
    memset(client_message, MAX_BUFFER_SIZE, 0);
    int buffer_read = 0;
    int total_read = 0;

    //buffered read from socket
    while(1) {
        buffer_read = read(sock, buffer, MAX_BUFFER_SIZE);
        if (buffer_read > 0) {
            client_message = realloc(client_message, total_read + buffer_read);
            memcpy(client_message + total_read, buffer, buffer_read);
            memset(buffer, MAX_BUFFER_SIZE, 0);
            total_read = total_read + buffer_read;
        } else if (buffer_read < 0) {
            /* close file and delete it, since data is not complete
               report error, or whatever */
            perror("[ERROR]: reading from socket");
            exit(1);
        }
        if (buffer_read == 0 || buffer_read < MAX_BUFFER_SIZE) {
            /* message received successfully */
            break;
        }
    }

    char *decodedJSON;
    char *encodedJSONPostData = strstr(client_message, CK_JSON_KEY);
    if (encodedJSONPostData != NULL) {
        char *encodedJSON = encodedJSONPostData + strlen(CK_JSON_KEY);
        decodedJSON = url_decode(encodedJSON, total_read - (encodedJSON - client_message));
    } else {
        decodedJSON = client_message;
    }

    printf("[DEBUG INFO]: decodedJSON message: %s\n", decodedJSON);
    cJSON *commandJSON = cJSON_Parse(decodedJSON);
    if (!commandJSON) {
        printf("[ERROR]: Invalid action JSON format for message: \n");
        //todo check if need to cJSON_Delete(commandJSON) here as well
        sendErrorMessage(sock, "Invalid action JSON format for message");
        return;
    }


    cJSON *actionJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_NAME_COMMAND);
    if (!actionJSON) {
        printf("[ERROR]: Invalid action JSON format for message: \n");
        //todo check if need to cJSON_Delete(commandJSON) here as well
        sendErrorMessage(sock, "Invalid action JSON format for message: no action found");
        return;
    }
    char *action = actionJSON->valuestring;
    printf("[DEBUG INFO]: Get action: %s\n", action);
    char *resultJSONtext;
    if (strncmp(action, JSCON_PARAM_VALUE_PUSH, 4) == 0 ) {
        //  push file (to send file to CK Node )
        cJSON *filenameJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_FILE_NAME);
        if (!filenameJSON) {
            printf("[ERROR]: Invalid action JSON format for provided message\n");
            //todo check if need to cJSON_Delete(commandJSON) here as well
            sendErrorMessage(sock, "Invalid action JSON format for message: no filenameJSON found");
            return;
        }

        char *fileName = filenameJSON->valuestring;
        cJSON *fileContentJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_FILE_CONTENT);
        if (!fileContentJSON) {
            printf("[ERROR]: Invalid action JSON format for message: \n");
            //todo check if need to cJSON_Delete(commandJSON) here as well
            sendErrorMessage(sock, "Invalid action JSON format for message: no fileContentJSON found");
            return;
        }

        char *file_content_base64 = fileContentJSON->valuestring;

        printf("[DEBUG INFO]: File name: %s\n", fileName);
        printf("[DEBUG INFO]: Data: %s\n", file_content_base64);

        char *file_content = malloc(Base64decode_len(file_content_base64));
        if (strlen(file_content_base64) != 0) {
            int bytesDecoded = Base64decode(file_content, file_content_base64);
            if (bytesDecoded == 0) {
                sendErrorMessage(sock, "Failed to Base64 decode file");
            }
            printf("[DEBUG INFO]: bytesDecoded: %i\n", bytesDecoded);
        } else {
            printf("[WARNING]: file content is empty nothing to decode\n");
        }

        printf("[DEBUG INFO]: File content: %s\n", file_content);

        // 2) save locally at tmp dir
        char *filePath = concat(baseDir, fileName);

        FILE *file=fopen(filePath, "wb");

        int results = fputs(file_content, file);
        if (results == EOF) {
            sendErrorMessage(sock, "Failed to write file ");
        }
        fclose(file);
        free(file_content);
        printf("[DEBUG INFO]: file saved to: %s\n", filePath);

        /**
         * return {"return":0, "compileUUID:}
         */
        char *compileUUID = "123123123123123"; // todo remove hardcoded value and provide implementation
        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
        cJSON_AddItemToObject(resultJSON, "compileUUID", cJSON_CreateString(compileUUID));
        resultJSONtext = cJSON_Print(resultJSON);
        printf("[DEBUG INFO]: Push response: %s\n", resultJSONtext);
        cJSON_Delete(resultJSON);
    } else if (strncmp(action, "pull", 4) == 0 ) {
        //  pull file (to receive file from CK node)
        cJSON *filenameJSON = cJSON_GetObjectItem(commandJSON, JSON_PARAM_FILE_NAME);
        if (!filenameJSON) {
            printf("[ERROR]: Invalid action JSON format for provided message\n");
            //todo check if need to cJSON_Delete(commandJSON) here as well
            sendErrorMessage(sock, "Invalid action JSON format for message: no filenameJSON found");
            return;
        }

        char *fileName = filenameJSON->valuestring;
        char *filePath = concat(baseDir, fileName);
        FILE *file=fopen(filePath, "rb");
        if (!file) {
            sendErrorMessage(sock, concat("File not found at path:", filePath));
            return;
        }

        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *fileContent = malloc(fsize + 1);
        memset(fileContent,fsize + 1,0);
        fread(fileContent, fsize, 1, file);
        fclose(file);

        fileContent[fsize] = 0;
        printf("[DEBUG INFO]: File size: %lu\n", fsize);
        printf("[DEBUG INFO]: File content: %s\n", fileContent);

        char *encodedContent = malloc(Base64encode_len(fsize) + 1);
        if (fsize > 0) {
            int encodedSize = Base64encode(encodedContent, fileContent, fsize);
            printf("[DEBUG INFO]: bytes Encoded: %i\n", encodedSize);
        }
                
        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
        cJSON_AddItemToObject(resultJSON, JSON_PARAM_FILE_NAME, cJSON_CreateString(fileName));
        cJSON_AddItemToObject(resultJSON, JSON_PARAM_FILE_CONTENT, cJSON_CreateString(encodedContent));
        resultJSONtext = cJSON_Print(resultJSON);
        printf("[DEBUG INFO]: Pull response: %s\n", resultJSONtext);
        cJSON_Delete(resultJSON);
    } else if (strncmp(action, "run", 3) == 0 ) {
        //  shell (to execute a binary at CK node)
        printf("[DEBUG INFO]: Get shell action");
        // todo implement:
        // 1) find local file by provided name - send JSON error if not found
        // 2) generate run ID
        // 3) fork new process for async execute
        // 3) return run UUID as JSON sync with run UID and send to client
        // 4) in async process convert to JSON with ru UID and send to client

        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
        cJSON_AddItemToObject(resultJSON, "runUUID", cJSON_CreateString("12312312323213")); // todo remove hardcoded value and provide implementation
        resultJSONtext = cJSON_Print(resultJSON);
        cJSON_Delete(resultJSON);
    } else if (strncmp(action, "state", 4) == 0 ) {
        printf("[DEBUG INFO]: Check run state by runUUID ");
        cJSON* params = cJSON_GetObjectItem(commandJSON, JSON_PARAM_PARAMS);
        char* runUUID = cJSON_GetObjectItem(params, "runUUID")->valuestring;
        printf("[DEBUG INFO]: runUUID: %s\n", runUUID);

        //todo implement get actual runing state by runUUID

        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "return", cJSON_CreateString("0"));
        resultJSONtext = cJSON_Print(resultJSON);
        cJSON_Delete(resultJSON);
    } else if (strncmp(action, "clear", 4) == 0 ) {
        printf("[DEBUG INFO]: Clearing tmp files ...");
        // todo implement removing all temporary files saved localy but need check some process could be in running state
        // so need to discus how it should work
    } else if (strncmp(action, "shutdown", 4) == 0 ) {
        printf("[DEBUG INFO]: Shutdown CK node");
        cJSON_Delete(commandJSON);
        return ;
    } else {
        sendErrorMessage(sock, "unknown action");
    }
    int n = write(sock, resultJSONtext, strlen(resultJSONtext));
    free(resultJSONtext);

    if (n < 0) {
        perror("ERROR writing to socket");
        return ;
    }
    cJSON_Delete(commandJSON);
    free(client_message);
}
