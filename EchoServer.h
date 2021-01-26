#include<limits.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<Winsock2.h>
#include<errno.h>
#include<io.h>
#include<pthread.h>
#include<sys/stat.h>
#define ERROR -1
#define MAX_CLIENTS 5
#define MAX_DATA 1024
#define MAX_RESPONSE 1024*1024
#define MAX_PATH_LENGTH 512

#define NUM_THREADS 4


#define FALSE 0
#define TRUE 1
#define NOT_FOUND 404
#define BAD_METHOD 4040
#define BAD_URI 4041
#define BAD_HTTP_VERSION 4042
#define NUM_OF_FILE_TYPES 8


struct HTTP_RequestParams {
  char *method;
  char *URI;
  char *httpversion;
};

struct TextfileData {
  int port_number;
  char document_root[MAX_PATH_LENGTH];
  char default_web_page[20];
  char extensions[NUM_OF_FILE_TYPES+1][512];
  char encodings [NUM_OF_FILE_TYPES+1][512];
};

void send_response(int client, int status_code, struct HTTP_RequestParams *params, char *full_path);
int handle_file_serving(char *path, char *body, struct TextfileData *config_data, int *result_status);
void client_handler(int client, struct TextfileData *config_data);
int validate_request_headers(struct HTTP_RequestParams *params, int *decision);
void extract_request_parameters(char *response, struct HTTP_RequestParams *params);
void removeSubstring(char *s,const char *toremove);
int setup_socket (int port_number, int max_clients);
void setup_server(struct TextfileData *config_data);
void construct_file_response(char *full_path, int client);
