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
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include <stdlib.h>
#include "cJSON.h"

static char *const JSON_PARAM_NAME_COMMAND = "command";
static char *const JSON_PARAM_PARAMS = "parameters";
static char *const JSCON_PARAM_VALUE_PUSH = "push";

/**
 * todo move out to config /etc/ck-crowdnode/ck-crowdnode.properties
 */
static const int MAX_BUFFER_SIZE = 20000;
static const int DEFAULT_SERVER_PORT = 8888;

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

void doProcessing(int sock);

int main( int argc, char *argv[] ) {

    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int pid;

    if (argc == 1) {
        portno = DEFAULT_SERVER_PORT;
        printf("[INFO]: Default server port  %i\n", DEFAULT_SERVER_PORT);
    } else if (argc == 2) {
        portno = argv[2];
    } else if (argc > 2)  {
        printf("USAGE: %s [serverport]  %i\n", argv[0]);
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));

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
        pid = fork();

        if (pid < 0) {
            perror("ERROR on fork");
            exit(1);
        }

        if (pid == 0) {
            /* This is the client process */
            close(sockfd);
            doProcessing(newsockfd);
            exit(0);
        }
        else {
            close(newsockfd);
        }
    }
}

void doProcessing(int sock) {
    cJSON *commandJSON;

    int n;
    char client_message[MAX_BUFFER_SIZE];
    bzero(client_message,MAX_BUFFER_SIZE);

    //todo think abount big messages: while((read_size = recv(client_sock , client_message , MAX_BUFFER_SIZE, 0)) > 0 )
    n = read(sock, client_message, MAX_BUFFER_SIZE - 1);

    if (n < 0) {
        perror("[ERROR]: reading from socket");
        exit(1);
    }

    printf("[DEBUG INFO]: Get client message: %s\n", client_message); // todo remove debug info later
    commandJSON = cJSON_Parse(client_message);
    if (!commandJSON) {
        printf("[ERROR]: Invalid command JSON format for message: %s\n", client_message);
        //todo check if need to cJSON_Delete(commandJSON) here as well
        return;
    }

    // Supported JSON commands:
    char*  command = cJSON_GetObjectItem(commandJSON, JSON_PARAM_NAME_COMMAND)->valuestring;
    printf("[DEBUG INFO]: Get command: %s\n", command);
    char *resultJSONtext;
    if (strncmp(command, JSCON_PARAM_VALUE_PUSH, 4) == 0 ) {
        //  push file (to send file to CK Node )
        cJSON* params = cJSON_GetObjectItem(commandJSON, JSON_PARAM_PARAMS);
        char* fileName = cJSON_GetObjectItem(params, "filename")->valuestring;
        char* encodedData = cJSON_GetObjectItem(params, "data")->valuestring;

        printf("[DEBUG INFO]: File name: %s\n", fileName);
        printf("[DEBUG INFO]: Data: %s\n", encodedData);

        // todo implement:
        // 1) decode encoded binary from JSON paramter
        // 2) save locally at tmp dir
        // 3) send back JSON with compile UID + путь, etc .

        char *compileUUID = "123123123123123"; // todo remove hardcoded value and provide implementation
        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "state", cJSON_CreateString("finished_ok")); // todo remove hardcoded value and provide implementation
        cJSON_AddItemToObject(resultJSON, "compileUUID", cJSON_CreateString(compileUUID));
        resultJSONtext = cJSON_Print(resultJSON);
        cJSON_Delete(resultJSON);
    } else if (strncmp(command, "pull", 4) == 0 ) {
        //  pull file (to receive file from CK node)
        printf("[DEBUG INFO]: Get pull command");
        cJSON* params = cJSON_GetObjectItem(commandJSON, JSON_PARAM_PARAMS);
        char* runUUID = cJSON_GetObjectItem(params, "runUUID")->valuestring;

        printf("[DEBUG INFO]: runUUID: %s\n", runUUID);

        // todo implement:
        // 1) find local file by provided name
        // 2) encode file to send
        // 3) convert to JSON and send to client

        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "filename", cJSON_CreateString("file1")); // todo remove hardcoded value and provide implementation
        cJSON_AddItemToObject(resultJSON, "data", cJSON_CreateString("sdffffffffffffffffffffffffffff4533333333333333333333333333333333"));
        resultJSONtext = cJSON_Print(resultJSON);
        cJSON_Delete(resultJSON);
    } else if (strncmp(command, "run", 4) == 0 ) {
        //  shell (to execute a binary at CK node)
        printf("[DEBUG INFO]: Get shell command");
        // todo implement:
        // 1) find local file by provided name - send JSON error if not found
        // 2) generate run ID
        // 3) fork new process for async execute
        // 3) return run UUID as JSON sync with run UID and send to client
        // 4) in async process convert to JSON with ru UID and send to client

        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "state", cJSON_CreateString("in progress")); // todo remove hardcoded value and provide implementation
        cJSON_AddItemToObject(resultJSON, "runUUID", cJSON_CreateString("12312312323213")); // todo remove hardcoded value and provide implementation
        resultJSONtext = cJSON_Print(resultJSON);
        cJSON_Delete(resultJSON);
    } else if (strncmp(command, "state", 4) == 0 ) {
        printf("[DEBUG INFO]: Check run state by runUUID ");
        cJSON* params = cJSON_GetObjectItem(commandJSON, JSON_PARAM_PARAMS);
        char* runUUID = cJSON_GetObjectItem(params, "runUUID")->valuestring;
        printf("[DEBUG INFO]: runUUID: %s\n", runUUID);

        //todo implement get actual runing state by runUUID

        cJSON *resultJSON = cJSON_CreateObject();
        cJSON_AddItemToObject(resultJSON, "state", cJSON_CreateString("finished_ok")); // todo remove hardcoded value and provide implementation
        resultJSONtext = cJSON_Print(resultJSON);
        cJSON_Delete(resultJSON);
    } else if (strncmp(command, "clear", 4) == 0 ) {
        printf("[DEBUG INFO]: Clearing tmp files ...");
        // todo implement removing all temporary files saved localy but need check some process could be in running state
        // so need to discus how it should work
    } else if (strncmp(command, "shutdown", 4) == 0 ) {
        printf("[DEBUG INFO]: Shutdown CK node");
        cJSON_Delete(commandJSON);
        return ;
    } else {
        perror("ERROR unknown command");
    }
    n = write(sock , resultJSONtext , strlen(resultJSONtext));

    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }

    cJSON_Delete(commandJSON);
}
